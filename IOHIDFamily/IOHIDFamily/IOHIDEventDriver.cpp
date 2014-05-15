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

#include "IOHIDEventDriver.h"
#include "IOHIDInterface.h"
#include "IOHIDKeys.h"
#include "IOHIDTypes.h"
#include "AppleHIDUsageTables.h"
#include "IOHIDPrivateKeys.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/IOLib.h>
#include <IOKit/usb/USB.h>
#include "IOHIDFamilyTrace.h"
#include "IOHIDEventTypes.h"

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

#define kReportHandlerSlots    8

#define GetReportHandlerSlot(id)    \
    ((id) & (kReportHandlerSlots - 1))

#define GetElementArray(slot, type) \
    _reportHandlers[slot].array[type]

#define GetReportType( type )                                               \
    ((type <= kIOHIDElementTypeInput_ScanCodes) ? kIOHIDReportTypeInput :   \
    (type <= kIOHIDElementTypeOutput) ? kIOHIDReportTypeOutput :            \
    (type <= kIOHIDElementTypeFeature) ? kIOHIDReportTypeFeature : -1)

#define GET_AXIS_COUNT(usage) (usage-kHIDUsage_GD_X+ 1)
#define GET_AXIS_INDEX(usage) (usage-kHIDUsage_GD_X)


#define kDefaultAbsoluteAxisRemovalPercentage           15
#define kDefaultButtonAbsoluteAxisRemovalPercentage     60

//===========================================================================
// IOHIDEventDriver class

#define super IOHIDEventService

OSDefineMetaClassAndStructors( IOHIDEventDriver, IOHIDEventService )

#define _digitizer                      _reserved->digitizer
#define _absoluteAxisRemovalPercentage  _reserved->absoluteAxisRemovalPercentage
#define _multiAxis                      _reserved->multiAxis


//====================================================================================================
// IOHIDEventDriver::init
//====================================================================================================
bool IOHIDEventDriver::init( OSDictionary * dictionary )
{
    if ( ! super::init ( dictionary ) )
        return false;

    _reserved = IONew(ExpansionData, 1);
    bzero(_reserved, sizeof(ExpansionData));

    _cachedRangeState               = true;
    _relativeButtonCollection       = false;
    _multipleReports                = false;
    _absoluteAxisRemovalPercentage  = kDefaultAbsoluteAxisRemovalPercentage;

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

    if (_reserved)
    {
        IODelete(_reserved, ExpansionData, 1);
        _reserved = NULL;
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



    if (!_interface->open(this, 0,  OSMemberFunctionCast(IOHIDInterface::InterruptReportAction, this, &IOHIDEventDriver::handleInterruptReport), NULL))
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

    setProperty("BootProtocol", number);
    OSSafeReleaseNULL(number);

    number = (OSNumber*)copyProperty(kIOHIDAbsoluteAxisBoundsRemovalPercentage, gIOServicePlane);
    if ( OSDynamicCast(OSNumber, number) ) {
        _absoluteAxisRemovalPercentage = number->unsigned32BitValue();
    }
    OSSafeReleaseNULL(number);
    
    OSArray *elements = _interface->createMatchingElements();
    bool result = false;

    if ( elements ) {
        if ( findElements ( elements, bootProtocol )) {
            result = true;
        }
    }
    OSSafeRelease(elements);
    
    return result;
}

//====================================================================================================
// IOHIDEventDriver::getTransport
//====================================================================================================
OSString * IOHIDEventDriver::getTransport ()
{
    return _interface ? _interface->getTransport() : (OSString *)OSSymbol::withCString("unknown:") ;
}

//====================================================================================================
// IOHIDEventDriver::getManufacturer
//====================================================================================================
OSString * IOHIDEventDriver::getManufacturer ()
{
    return _interface ? _interface->getManufacturer() : (OSString *)OSSymbol::withCString("unknown:") ;
}

//====================================================================================================
// IOHIDEventDriver::getProduct
//====================================================================================================
OSString * IOHIDEventDriver::getProduct ()
{
    return _interface ? _interface->getProduct() : (OSString *)OSSymbol::withCString("unknown:") ;
}

//====================================================================================================
// IOHIDEventDriver::getSerialNumber
//====================================================================================================
OSString * IOHIDEventDriver::getSerialNumber ()
{
    return _interface ? _interface->getSerialNumber() : (OSString *)OSSymbol::withCString("unknown:") ;
}

//====================================================================================================
// IOHIDEventDriver::getLocationID
//====================================================================================================
UInt32 IOHIDEventDriver::getLocationID ()
{
    return _interface ? _interface->getLocationID() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getVendorID
//====================================================================================================
UInt32 IOHIDEventDriver::getVendorID ()
{
    return _interface ? _interface->getVendorID() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getVendorIDSource
//====================================================================================================
UInt32 IOHIDEventDriver::getVendorIDSource ()
{
    return _interface ? _interface->getVendorIDSource() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getProductID
//====================================================================================================
UInt32 IOHIDEventDriver::getProductID ()
{
    return _interface ? _interface->getProductID() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getVersion
//====================================================================================================
UInt32 IOHIDEventDriver::getVersion ()
{
    return _interface ? _interface->getVersion() : -1 ;
}

//====================================================================================================
// IOHIDEventDriver::getCountryCode
//====================================================================================================
UInt32 IOHIDEventDriver::getCountryCode ()
{
    return _interface ? _interface->getCountryCode() : -1 ;
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
    if (_interface)
        _interface->close(this);
    _interface = NULL;

    return super::didTerminate(provider, options, defer);
}

//====================================================================================================
// IOHIDEventDriver::findElements
//====================================================================================================
bool IOHIDEventDriver::findElements ( OSArray* elementArray, UInt32 bootProtocol)
{
    UInt32              count               = 0;
    UInt32              index               = 0;
    UInt32              usage               = 0;
    UInt32              usagePage           = 0;
    bool                stored              = false;
    bool                pointer             = false;
    bool                supportsInk         = false;
    IOHIDElement *      element             = NULL;
    IOHIDElement *      buttonCollection    = NULL;
    IOHIDElement *      relativeCollection  = NULL;
    IOHIDElement *      zAxis               = NULL;
    IOHIDElement *      rzAxis              = NULL;
    OSArray *           buttonArray         = NULL;

    if ( bootProtocol == kBootProtocolMouse )
        _bootSupport = kBootMouse;

    if ( elementArray )
    {
        _supportedElements = elementArray;
        _supportedElements->retain();

        count = elementArray->getCount();

        for ( index = 0; index < count; index++ )
        {
            bool    isRelative  = false;
            UInt32  reportID    = 0;

            element = (IOHIDElement *) elementArray->getObject(index);

            if ( element == NULL )
                continue;

            if ( element->getType() == kIOHIDElementTypeCollection ) {

                if ( usagePage == kHIDPage_Digitizer ) {
                    switch ( usage )  {
                        case kHIDUsage_Dig_TouchScreen:
                        case kHIDUsage_Dig_TouchPad:
                        case kHIDUsage_Dig_Finger:
                            _digitizer.type = kDigitizerTransducerTypeFinger;
                            break;
                        default:
                            break;

                    }
                }
                continue;
            }

            reportID = element->getReportID();

            if ( reportID > 0)
                _multipleReports = true;

            if ( element->getFlags() & kIOHIDElementFlagsRelativeMask)
                isRelative = true;

            usagePage   = element->getUsagePage();
            usage       = element->getUsage();

            if ( usagePage == kHIDPage_GenericDesktop )
            {
                switch ( usage )
                {
                    case kHIDUsage_GD_Dial:
                    case kHIDUsage_GD_Wheel:

                        if ((element->getFlags() & (kIOHIDElementFlagsNoPreferredMask|kIOHIDElementFlagsRelativeMask)) == 0) {
                            calibratePreferredStateElement(element, _absoluteAxisRemovalPercentage);
                        }

                        stored |= storeReportElement(element);
                        pointer = true;
                        break;

                    case kHIDUsage_GD_X:
                    case kHIDUsage_GD_Y:
                        _bootSupport &= (usage==kHIDUsage_GD_X) ? ~kMouseXAxis : ~kMouseYAxis;

                        processMultiAxisElement(element, &_multiAxis.capable, &supportsInk, &relativeCollection);

                        if ( _multiAxis.capable ) {
                            calibratePreferredStateElement(element, _absoluteAxisRemovalPercentage);

                            if ( reportID > _multiAxis.sendingReportID )
                                _multiAxis.sendingReportID = reportID;

                        } else if ( !isRelative ) {
                            calibrateDigitizerElement(element, _absoluteAxisRemovalPercentage);
                        }

                        stored |= storeReportElement ( element );
                        pointer = true;
                        break;

                    case kHIDUsage_GD_Z:
                        processMultiAxisElement(element, &_multiAxis.capable);
                        zAxis = element;
                        stored |= storeReportElement(element);
                        pointer = true;
                        break;

                    case kHIDUsage_GD_Rx:
                    case kHIDUsage_GD_Ry:
                        if ( _multiAxis.capable ) {
                            _multiAxis.options |= kMultiAxisOptionRotationForTranslation;

                            if ( reportID > _multiAxis.sendingReportID )
                                _multiAxis.sendingReportID = reportID;
                        }

                    case kHIDUsage_GD_Rz:
                        pointer = true;
                        processMultiAxisElement(element, &_multiAxis.capable);

                        if ( _multiAxis.capable ) {
                            rzAxis = element;
                            stored |= storeReportElement ( element );
                        }
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
                        _digitizer.containsRange = true;
                        _cachedRangeState = false;
                        stored |= storeReportElement ( element );
                        break;
                    case kHIDUsage_Dig_TipPressure:
                        supportsInk = true;
                    case kHIDUsage_Dig_BarrelPressure:
                        calibrateDigitizerElement(element, _absoluteAxisRemovalPercentage);
                        stored |= storeReportElement ( element );
                        break;
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

            else if ( usagePage == kHIDPage_Consumer || usagePage == kHIDPage_Telephony )
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

                if ( !buttonCollection )
                    buttonCollection = element->getParentElement();

                stored |= storeReportElement ( element );
            }
        }
        buttonArray->release();
    }

    if ( zAxis ) {

        if ( (_multiAxis.capable & ((1<<GET_AXIS_INDEX(kHIDUsage_GD_Rx)) | (1<<GET_AXIS_INDEX(kHIDUsage_GD_Ry)))) == 0) {
            _multiAxis.options |= kMultiAxisOptionZForScroll;
        }
        calibratePreferredStateElement(zAxis, _absoluteAxisRemovalPercentage);
    }

    if ( rzAxis ) {
        SInt32 removal = _absoluteAxisRemovalPercentage;


        if ( (_multiAxis.capable & ((1<<GET_AXIS_INDEX(kHIDUsage_GD_Rx)) | (1<<GET_AXIS_INDEX(kHIDUsage_GD_Ry)))) != 0) {
            removal *= 2;
        }
        calibratePreferredStateElement(rzAxis, removal);

    }

    setProperty("MultiAxis", _multiAxis.capable, 32);

    if ( supportsInk )
        setProperty("SupportsInk", 1, 32);

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

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEventDriver::processMultiAxisElement
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void IOHIDEventDriver::processMultiAxisElement(IOHIDElement * element, UInt32 * isMultiAxis, bool * supportsInk, IOHIDElement ** relativeCollection)
{
    // RY: can't deal with array objects
    if ( (element->getFlags() & kIOHIDElementFlagsVariableMask) == 0 ) {
        return;
    }
    
    if (!(*isMultiAxis & (1<<(element->getUsage()-kHIDUsage_GD_X))))
    {
        if ( !element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse) &&
             !element->conformsTo(kHIDPage_Digitizer) )
        {
            bool isAbsolute = (element->getFlags() & (kIOHIDElementFlagsNoPreferredMask|kIOHIDElementFlagsRelativeMask)) == 0;
            if ( isAbsolute ||
                 element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_MultiAxisController) ||
                 element->conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick) )
            {
        *isMultiAxis |= (1<<(element->getUsage()-kHIDUsage_GD_X));
    }
        }
    }

    if ( relativeCollection && isMultiAxis && supportsInk ) {
        if ((element->getFlags() & kIOHIDElementFlagsRelativeMask) != 0) {

            if ( !*isMultiAxis && !*relativeCollection )
                *relativeCollection = element->getParentElement();
        }
        else if ( !*isMultiAxis ) {
            *supportsInk = true;
        }
    }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEventDriver::calibratePreferredStateElement
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void IOHIDEventDriver::calibratePreferredStateElement(IOHIDElement * element, SInt32 removalPercentage)
{
    UInt32 mid      = element->getLogicalMin() + ((element->getLogicalMax() - element->getLogicalMin()) / 2);
    UInt32 satMin   = element->getLogicalMin();
    UInt32 satMax   = element->getLogicalMax();
    UInt32 diff     = ((satMax - satMin) * removalPercentage) / 200;
    UInt32 dzMin    = mid - diff;
    UInt32 dzMax    = mid + diff;
    satMin          += diff;
    satMax          -= diff;

    element->setCalibration(-1, 1, satMin, satMax, dzMin, dzMax);

}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEventDriver::calibrateDigitizerElement
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void IOHIDEventDriver::calibrateDigitizerElement(IOHIDElement * element, SInt32 removalPercentage)
{
#if TARGET_OS_EMBEDDED
    removalPercentage = 0;
#endif /* TARGET_OS_EMBEDDED */
    UInt32 satMin   = element->getLogicalMin();
    UInt32 satMax   = element->getLogicalMax();
    UInt32 diff     = ((satMax - satMin) * removalPercentage) / 200;
    satMin          += diff;
    satMax          -= diff;

    element->setCalibration(0, 1, satMin, satMax);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDEventDriver::calibrateAxisToButtonElement
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void IOHIDEventDriver::calibrateAxisToButtonElement(IOHIDElement * element, SInt32 removalPercentage)
{
    UInt32 satMin   = element->getLogicalMin();
    UInt32 satMax   = element->getLogicalMax();
    UInt32 diff     = ((satMax - satMin) * removalPercentage) / 200;
    satMin          += diff;
    satMax          -= diff;

    element->setCalibration(0, 1, satMin, satMax);
}

//====================================================================================================
// IOHIDEventDriver::getReportElements
//====================================================================================================
OSArray * IOHIDEventDriver::getReportElements()
{
    return _supportedElements;
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
    OSArray *       elements;
    IOHIDElement *  element;
    UInt32          count               = 0;
    UInt32          index               = 0;
    UInt32          usage               = 0;
    UInt32          usagePage           = 0;
    UInt32          buttonState         = _cachedButtonState;
    UInt32          transducerID        = reportID;
    UInt32          volumeHandled       = 0;
    UInt32          volumeState         = 0;
    SInt32          relativeAxis[GET_AXIS_COUNT(kHIDUsage_GD_Y)]    = {};
    IOFixed         absoluteAxis[GET_AXIS_COUNT(kHIDUsage_GD_Rz)]   = {};
    IOFixed         tipPressure         = 0;
    IOFixed         barrelPressure      = 0;
    SInt32          scrollVert          = 0;
    SInt32          scrollHoriz         = 0;
    IOFixed         tiltX               = 0;
    IOFixed         tiltY               = 0;
    IOFixed         twist               = 0;
    bool            isAbsoluteAxis      = false;
    bool            pointingHandled     = false;
    bool            digitizerHandled    = false;
    bool            invert              = false;
    bool            elementIsCurrent    = false;
    bool            inRange             = _cachedRangeState;
    IOOptionBits    options             = 0;
    AbsoluteTime    elementTS;
    AbsoluteTime    reportTS;

    if (!readyForReports())
        return;

    elements = GetElementArray(GetReportHandlerSlot(reportID), reportType);

    IOHID_DEBUG(kIOHIDDebugCode_InturruptReport, reportType, reportID, elements ? elements->getCount() : -1, getRegistryEntryID());

    if ( elements ) {
        count = elements->getCount();

        for (index = 0; index < count; index++) {
            bool elementIsRelative = false;

            element = (IOHIDElement *)elements->getObject(index);

            elementTS           = element->getTimeStamp();
            reportTS            = timeStamp;
            elementIsRelative   = element->getFlags() & kIOHIDElementFlagsRelativeMask;
            elementIsCurrent    = (CMP_ABSOLUTETIME(&elementTS, &reportTS) == 0);

            if ( element->getReportID() != reportID )
                continue;

            usagePage       = element->getUsagePage();
            usage           = element->getUsage();


            if ( usagePage == kHIDPage_GenericDesktop ) {
                switch ( element->getUsage() ) {
                    case kHIDUsage_GD_X:
                    case kHIDUsage_GD_Y:
                        pointingHandled |= true;
                        if ( _multiAxis.capable ) {
                            _multiAxis.axis[GET_AXIS_INDEX(element->getUsage())] = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        }
                        else if (elementIsRelative) {
                            if ( elementIsCurrent )
                                relativeAxis[GET_AXIS_INDEX(element->getUsage())] = element->getValue();
                        }
                        else {
                            digitizerHandled |= elementIsCurrent;
                            isAbsoluteAxis = true;
                            absoluteAxis[GET_AXIS_INDEX(element->getUsage())] = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        }
                        break;
                    case kHIDUsage_GD_Z:
                        if ( _multiAxis.capable ) {
                            _multiAxis.axis[GET_AXIS_INDEX(element->getUsage())] = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        }
                        else if (elementIsRelative) {
                            if ( elementIsCurrent )
                                scrollHoriz = element->getValue();
                        }
                        else {
                            digitizerHandled |= elementIsCurrent;
                            absoluteAxis[GET_AXIS_INDEX(element->getUsage())] = element->getValue();
                        }
                        break;

                    case kHIDUsage_GD_Rx:
                    case kHIDUsage_GD_Ry:
                    case kHIDUsage_GD_Rz:
                        if ( _multiAxis.capable ) {
                            pointingHandled |= true;
                            _multiAxis.axis[GET_AXIS_INDEX(element->getUsage())] = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        }
                        break;
                    case kHIDUsage_GD_Wheel:
                    case kHIDUsage_GD_Dial:
                        if ( elementIsCurrent ) {
                            scrollVert = (element->getFlags() & kIOHIDElementFlagsWrapMask) ?  element->getValue(kIOHIDValueOptionsFlagRelativeSimple) : element->getValue();
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
                            digitizerHandled |= ( inRange != _cachedRangeState );
                        }
                        break;
                    case kHIDUsage_Dig_BarrelPressure:
                        digitizerHandled    |= elementIsCurrent;
                        barrelPressure      = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        break;
                    case kHIDUsage_Dig_TipPressure:
                        digitizerHandled    |= elementIsCurrent;
                        tipPressure         = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        break;
                    case kHIDUsage_Dig_XTilt:
                        digitizerHandled    |= elementIsCurrent;
                        tiltX               = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        break;
                    case kHIDUsage_Dig_YTilt:
                        digitizerHandled    |= elementIsCurrent;
                        tiltY               = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        break;
                    case kHIDUsage_Dig_Twist:
                        digitizerHandled    |= elementIsCurrent;
                        twist               = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        break;
                    case kHIDUsage_Dig_TransducerIndex:
                        digitizerHandled    |= elementIsCurrent;
                        transducerID        = element->getValue();
                        break;
                    case kHIDUsage_Dig_Invert:
                        digitizerHandled    |= elementIsCurrent;
                        invert              = (element->getValue() != 0);
                        break;
                    default:
                        break;
                }
            }
            else if ( usagePage == kHIDPage_Button ) {
                pointingHandled |= true;

                digitizerHandled  |= ( isAbsoluteAxis && elementIsCurrent );

                setButtonState ( &buttonState, (element->getUsage() - 1), element->getValue());
            }
            else if (( usagePage == kHIDPage_KeyboardOrKeypad || usagePage == kHIDPage_Telephony ) && elementIsCurrent ) {
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
        handleBootPointingReport(report, &relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_X)], &relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_Y)], &buttonState);
        pointingHandled |= true;
    }

    if ( pointingHandled || digitizerHandled ) {

        if ( isAbsoluteAxis && !relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_X)] && !relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_Y)] && !_digitizer.containsRange ) {
#if TARGET_OS_EMBEDDED // {
            inRange = buttonState & 0x1;

            digitizerHandled = inRange != _cachedRangeState;
#else // } TARGET_OS_EMBEDDED {
            //IOLog("Correcting for !_digitizer.containsRange for 0x%08llx\n", getRegistryEntryID());
            inRange = digitizerHandled = true;
#endif // } TARGET_OS_EMBEDDED
        }

        if ( invert )
            options |= IOHIDEventService::kDigitizerInvert;

        if ( _multiAxis.capable ) {
            if ( reportID == _multiAxis.sendingReportID ) {
                dispatchMultiAxisPointerEvent(timeStamp, buttonState, _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_X)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Y)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Z)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Rx)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Ry)], _multiAxis.axis[GET_AXIS_INDEX(kHIDUsage_GD_Rz)], _multiAxis.options);
            }
			else {
				// event is dropped
				//IOLog("Dropping event from 0x%08x because %d != %d\n", getRegistryEntryID(), reportID, _multiAxis.sendingReportID);
			}
        }
        else if ( digitizerHandled || (isAbsoluteAxis && !relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_X)] && !relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_Y)] && (inRange || ((buttonState != _cachedButtonState) && !_relativeButtonCollection)))) {
            dispatchDigitizerEventWithTiltOrientation(timeStamp, transducerID, _digitizer.type, inRange, buttonState, absoluteAxis[GET_AXIS_INDEX(kHIDUsage_GD_X)], absoluteAxis[GET_AXIS_INDEX(kHIDUsage_GD_Y)], absoluteAxis[GET_AXIS_INDEX(kHIDUsage_GD_Z)], tipPressure, barrelPressure, twist, tiltX, tiltY, options);
        }
        else if (relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_X)] || relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_Y)] || (buttonState != _cachedButtonState)) {
            dispatchRelativePointerEvent(timeStamp, relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_X)], relativeAxis[GET_AXIS_INDEX(kHIDUsage_GD_Y)], buttonState);
        }
        else {
        	// event is dropped
        	//IOLog("Dropping event from 0x%08x\n", getRegistryEntryID());
        }

        _cachedRangeState = inRange;
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

