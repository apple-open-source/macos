/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * 07 Jan 2002 		ryepez
 *			This class is based off IOHIPointing and handles
 *			USB HID report based pointing devices
 */

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>

#include "IOHIDPointing.h"
#include "IOHIDKeys.h"


#define kMaxButtons	32	// Is this defined anywhere in the event headers?
#define kMaxValues	32	// This should be plenty big to find the X, Y and wheel values - is there some absolute max?

#define kDefaultFixedResolution 	(400 << 16)
#define kDefaultScrollFixedResolution	(10 << 16)


#define super IOHIPointing
OSDefineMetaClassAndStructors(IOHIDPointing, IOHIPointing);

bool IOHIDPointing::init(OSDictionary * properties)
{
    if (!super::init(properties))  return false;
    
    _numButtons = 1;
    _resolution = kDefaultFixedResolution;
    _scrollResolution = kDefaultScrollFixedResolution;
    _preparsedReportDescriptorData = NULL;
    _buttonCollection = -1;
    _xCollection = -1;
    _yCollection = -1;
    _tipPressureCollection = -1;
    _digitizerButtonCollection = -1;
    _scrollWheelCollection = -1;
    _horzScrollCollection = -1;
    _hasInRangeReport = false;
    _tipPressureMin = 255;
    _tipPressureMax = 255;
        
    return true;
}



bool IOHIDPointing::start(IOService *provider)
{
    IOMemoryDescriptor	*descriptor;
    IOReturn		ret;
    
    _provider = OSDynamicCast(IOHIDDevice, provider);
    
    if (!provider)
        return false;
    
        
    // push up properties from our provider
    propagateProperties();
    
    // grab and parse the report descriptor
    ret = _provider->newReportDescriptor(&descriptor);
    
    if ((ret != kIOReturnSuccess) || !descriptor)
    {
        if (descriptor)
            descriptor->release();
            
        return false;
    }
    
    ret = parseReportDescriptor(descriptor);
    descriptor->release();
    
    if (ret != kIOReturnSuccess)
        return false;
    
    return super::start(provider);
}



void IOHIDPointing::free()
{
    // maybe put in stop?
    if (_preparsedReportDescriptorData) 
    {
        HIDCloseReportDescriptor(_preparsedReportDescriptorData);
    }
    //------
    
    super::free();
}

//---------------------------------------------------------------------------
// Parse a report descriptor and setup any relavent pointing data

IOReturn IOHIDPointing::parseReportDescriptor( IOMemoryDescriptor * report,
                                             IOOptionBits         options )
{
    OSStatus             result;
    void *               reportData;
    IOByteCount          reportLength;
    IOByteCount          segmentSize;
    IOReturn             ret = kIOReturnSuccess;
    
    HIDButtonCapabilities	buttonCaps[kMaxButtons];
    UInt32			numButtonCaps = kMaxButtons;
    HIDValueCapabilities	valueCaps[kMaxValues];
    UInt32			numValueCaps = kMaxValues;

    reportData   = report->getVirtualSegment(0, &segmentSize);
    reportLength = report->getLength();

    if ( segmentSize != reportLength )
    {
        reportData = IOMalloc( reportLength );
        if ( reportData == 0 )
            return kIOReturnNoMemory;

        report->readBytes( 0, reportData, reportLength );
    }

    // Parse the report descriptor.

    result = HIDOpenReportDescriptor(
                reportData,      /* report descriptor */
                reportLength,    /* report size in bytes */
                &_preparsedReportDescriptorData,      /* pre-parse data */
                0 );             /* flags */

    if ( segmentSize != reportLength )
    {
        IOFree( reportData, reportLength );
    }

    if ( result != kHIDSuccess )
    {
        return kIOReturnError;
    }

    do {
        result = HIDGetSpecificButtonCapabilities(kHIDInputReport,
                                          kHIDPage_Button,
                                          0,
                                          0,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if ((result == noErr) && (numButtonCaps > 0)) 
        {
            _buttonCollection = buttonCaps[0].collection;	// Do we actually need to look at and store all of the button page collections?
            if (buttonCaps[0].isRange) 
            {
                _numButtons = buttonCaps[0].u.range.usageMax - buttonCaps[0].u.range.usageMin + 1;
            }
            
            /*
             * RY: Publish a property containing the button Count.  This will
             * will be used to determine whether or not the button
             * behaviors can be modified.
             */
            if (_numButtons > 1)
            {
                setProperty(kIOHIDPointerButtonCountKey, buttonCount());
            }

            
        }

        numButtonCaps = kMaxButtons;
        result = HIDGetSpecificButtonCapabilities(kHIDInputReport,
                                          kHIDPage_Digitizer,
                                          0,
                                          0,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if ((result == noErr) && (numButtonCaps > 0)) {
            _digitizerButtonCollection = buttonCaps[0].collection;
        }

        numButtonCaps = kMaxButtons;
        result = HIDGetSpecificButtonCapabilities(kHIDInputReport,
                                          kHIDPage_Digitizer,
                                          0,
                                          kHIDUsage_Dig_InRange,
                                          buttonCaps,
                                          &numButtonCaps,
                                          _preparsedReportDescriptorData);
        if (result == noErr) {
            _hasInRangeReport = true;
        }

        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_X,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0))
        {
            _xCollection = valueCaps[0].collection;
            _absoluteCoordinates = valueCaps[0].isAbsolute;
            _bounds.minx = valueCaps[0].logicalMin;
            _bounds.maxx = valueCaps[0].logicalMax;
            
            // Check to see if this has a different resolution. (Only checking x-axis.) 
            // (Can use equation given in section 6.2.2.7 of the Device Class Definition for HID, v 1.1.)
            // _resolution = (logMax -logMin)/((physMax -physMin) * 10 ** exp)

            // If there is no physical min and max in HID descriptor,
            // cababilites calls set equal to logical min and max.
            // Keep default resolution if we don't have distinct physical min and max.
            if (valueCaps[0].physicalMin != valueCaps[0].logicalMin &&
                valueCaps[0].physicalMax != valueCaps[0].logicalMax)
            {
                SInt32 logicalDiff = (valueCaps[0].logicalMax - valueCaps[0].logicalMin);
                SInt32 physicalDiff = (valueCaps[0].physicalMax - valueCaps[0].physicalMin);
                
                // Since IOFixedDivide truncated fractional part and can't use floating point
                // within the kernel, have to convert equation when using negative exponents:
                // _resolution = ((logMax -logMin) * 10 **(-exp))/(physMax -physMin)

                // Even though unitExponent is stored as SInt32, The real values are only
                // a signed nibble that doesn't expand to the full 32 bits.
                SInt32 resExponent = valueCaps[0].unitExponent & 0x0F;
                
                if (resExponent < 8)
                {
                    for (int i = resExponent; i > 0; i--)
                    {
                        physicalDiff *=  10;
                    }
                }
                else
                {
                    for (int i = 0x10 - resExponent; i > 0; i--)
                    {
                        logicalDiff *= 10;
                    }
                }
                _resolution = (logicalDiff / physicalDiff) << 16;

#if (DEBUGGING_LEVEL > 2)
                IOLog ("   _resolution = %lx\n", _resolution);
#endif
                
                // Before i added in the AppleUSBMouse::init function, resolution was called to calculate
                // the acceleration curves before we ever got to start, which in turn called parseHIDDescriptor
                // where the real resolution was calculated. In that event, if the resolution changed from the
                // default value, we would have to tell IOHIPointing to recalculate the curves based on the
                // new resolution. Per Adam Wang, we could call IOHIPointing::resetPointer() to do the
                // recalculation. Unfortunately IOHIPointing::resetPointer() is private and cannot be used
                // here. Rather than go back to IOHIPointing for an API change, since the new init function
                // seems to have us calling AppleUSBMouse::start first, we no longer need to deal with this.
                // (I am leaving this code snippet here as a reminder in case something changes and i don't
                // want to loose this arcane bit of knowledge.)
                //
                // if (_resolution != kDefaultFixedResolution)
                // {
                //     resetPointer();
                // }
            }

        } else {
            IOLog ("%s: error getting X axis information from HID report descriptor.  err=0x%lx\n", getName(), result);
            ret = kIOReturnError;
            break;
        }

        if (_provider)
        {
            OSNumber *resolution = OSNumber::withNumber(_resolution, 32);
            if (resolution)
                _provider->setProperty(kIOHIDPointerResolutionKey, resolution);
        }
        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_Y,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _yCollection = valueCaps[0].collection;
            _bounds.miny = valueCaps[0].logicalMin;
            _bounds.maxy = valueCaps[0].logicalMax;
        } else {
            IOLog ("%s: error getting Y axis information from HID report descriptor.  err=0x%lx\n", getName(), result);
            ret = kIOReturnError;
            break;
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_Digitizer,
                                         0,
                                         kHIDUsage_Dig_TipPressure,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _tipPressureCollection = valueCaps[0].collection;
            _tipPressureMin = valueCaps[0].logicalMin;
            _tipPressureMax = valueCaps[0].logicalMax;
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_Wheel,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _scrollWheelCollection = valueCaps[0].collection;
            
            // Check to see if this has a different resolution. (Only checking verticle scroll wheel.) 
            // (Can use equation given in section 6.2.2.7 of the Device Class Definition for HID, v 1.1.)
            // _resolution = (logMax -logMin)/((physMax -physMin) * 10 ** exp)

            // If there is no physical min and max in HID descriptor,
            // cababilites calls set equal to logical min and max.
            // Keep default resolution if we don't have distinct physical min and max.
            if (valueCaps[0].physicalMin != valueCaps[0].logicalMin &&
                valueCaps[0].physicalMax != valueCaps[0].logicalMax)
            {
                SInt32 logicalDiff = (valueCaps[0].logicalMax - valueCaps[0].logicalMin);
                SInt32 physicalDiff = (valueCaps[0].physicalMax - valueCaps[0].physicalMin);
                
                // Since IOFixedDivide truncated fractional part and can't use floating point
                // within the kernel, have to convert equation when using negative exponents:
                // _resolution = ((logMax -logMin) * 10 **(-exp))/(physMax -physMin)

                // Even though unitExponent is stored as SInt32, The real values are only
                // a signed nibble that doesn't expand to the full 32 bits.
                SInt32 resExponent = valueCaps[0].unitExponent & 0x0F;
                
                if (resExponent < 8)
                {
                    for (int i = resExponent; i > 0; i--)
                    {
                        physicalDiff *=  10;
                    }
                }
                else
                {
                    for (int i = 0x10 - resExponent; i > 0; i--)
                    {
                        logicalDiff *= 10;
                    }
                }
                _scrollResolution = (logicalDiff / physicalDiff) << 16;

#if (DEBUGGING_LEVEL > 2)
                IOLog ("   _scrollResolution = %lx\n", _scrollResolution);
#endif
                
                // Before i added in the AppleUSBMouse::init function, resolution was called to calculate
                // the acceleration curves before we ever got to start, which in turn called parseHIDDescriptor
                // where the real resolution was calculated. In that event, if the resolution changed from the
                // default value, we would have to tell IOHIPointing to recalculate the curves based on the
                // new resolution. Per Adam Wang, we could call IOHIPointing::resetScroll() to do the
                // recalculation. Unfortunately IOHIPointing::resetScroll() is private and cannot be used
                // here. Rather than go back to IOHIPointing for an API change, since the new init function
                // seems to have us calling AppleUSBMouse::start first, we no longer need to deal with this.
                // (I am leaving this code snippet here as a reminder in case something changes and i don't
                // want to loose this arcane bit of knowledge.)
                //
                // if (_resolution != kDefaultScrollFixedResolution)
                // {
                //     resetScroll();
                // }
            } 
            
            // Unfortunately, due to binary compatibility concerns, there is no virtual function
            // available to get the scrollResolution.  To pass this value to our superclass, we
            // instead must set a property.
            OSNumber *scrollResolution = OSNumber::withNumber(_scrollResolution, 32);
            if (scrollResolution)
                setProperty(kIOHIDScrollResolutionKey, scrollResolution);
        }

        numValueCaps = kMaxValues;
        result = HIDGetSpecificValueCapabilities(kHIDInputReport,
                                         kHIDPage_GenericDesktop,
                                         0,
                                         kHIDUsage_GD_Z,
                                         valueCaps,
                                         &numValueCaps,
                                         _preparsedReportDescriptorData);
        if ((result == noErr) && (numValueCaps > 0)) {
            _horzScrollCollection = valueCaps[0].collection;
        }

    } while (false);
    
    return ret;
}

//---------------------------------------------------------------------------
// Parse a pointing related report data and post events

IOReturn IOHIDPointing::handleReport(
                    IOMemoryDescriptor * report,
                    IOOptionBits         options) 
{
    OSStatus	status;
    HIDUsage	usageList[kMaxButtons];
    UInt32	usageListSize = kMaxButtons;
    UInt32	buttonState = 0;
    SInt32	usageValue;
    SInt32	pressure = MAXPRESSURE;
    int		dx = 0, dy = 0, scrollWheelDelta = 0, horzScrollDelta = 0;
    AbsoluteTime now;
    bool	inRange = !_hasInRangeReport;
    UInt8 *	mouseData;
    IOByteCount	ret_bufsize;
    IOByteCount segmentSize;

    
    // Get a pointer to the data in the descriptor.

    mouseData   = (UInt8 *)report->getVirtualSegment(0, &segmentSize);
    ret_bufsize = report->getLength();

    if ( ret_bufsize == 0 )
        return kIOReturnBadArgument;

    // Are there multiple segments in the descriptor? If so,
    // allocate a buffer and copy the data from the descriptor.

    if ( segmentSize != ret_bufsize )
    {
        mouseData = (UInt8 *)IOMalloc( ret_bufsize );
        if ( mouseData == 0 )
            return kIOReturnNoMemory;

        report->readBytes( 0, mouseData, ret_bufsize );
    }

    if (_buttonCollection != -1) {
        status = HIDGetButtonsOnPage (kHIDInputReport,
                                      kHIDPage_Button,
                                      _buttonCollection,
                                      usageList,
                                      &usageListSize,
                                      _preparsedReportDescriptorData,
                                      mouseData,
                                      ret_bufsize);
        if (status == noErr) {
            UInt32 usageNum;
            for (usageNum = 0; usageNum < usageListSize; usageNum++) {
                if (usageList[usageNum] <= kMaxButtons) {
                    buttonState |= (1 << (usageList[usageNum] - 1));
                }
            }
        }

    }

    if (_tipPressureCollection != -1) {
        status = HIDGetUsageValue (kHIDInputReport,
                                   kHIDPage_Digitizer,
                                   _tipPressureCollection,
                                   kHIDUsage_Dig_TipPressure,
                                   &usageValue,
                                   _preparsedReportDescriptorData,
                                   mouseData,
                                   ret_bufsize);
        if (status == noErr) {
            pressure = usageValue;
        }
    }

    if (_digitizerButtonCollection != -1) {
        usageListSize = kMaxButtons;
        status = HIDGetButtonsOnPage (kHIDInputReport,
                                      kHIDPage_Digitizer,
                                      _digitizerButtonCollection,
                                      usageList,
                                      &usageListSize,
                                      _preparsedReportDescriptorData,
                                      mouseData,
                                      ret_bufsize);
        if (status == noErr) {
            UInt32 usageNum;
            for (usageNum = 0; usageNum < usageListSize; usageNum++) {
                switch (usageList[usageNum]) {
                    case kHIDUsage_Dig_BarrelSwitch:
                        buttonState |= 2;	// Set the right (secondary) button for the barrel switch
                        break;
                    case kHIDUsage_Dig_TipSwitch:
                        buttonState |= 1;	// Set the left (primary) button for the tip switch
                        break;
                    case kHIDUsage_Dig_InRange:
                        inRange = 1;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    if (_scrollWheelCollection != -1) {
        status = HIDGetUsageValue (kHIDInputReport,
                                   kHIDPage_GenericDesktop,
                                   _scrollWheelCollection,
                                   kHIDUsage_GD_Wheel,
                                   &usageValue,
                                   _preparsedReportDescriptorData,
                                   mouseData,
                                   ret_bufsize);
        if (status == noErr) {
            scrollWheelDelta = usageValue;
        }
    }

    if (_horzScrollCollection != -1) {
        status = HIDGetUsageValue (kHIDInputReport,
                                   kHIDPage_GenericDesktop,
                                   _horzScrollCollection,
                                   kHIDUsage_GD_Z,
                                   &usageValue,
                                   _preparsedReportDescriptorData,
                                   mouseData,
                                   ret_bufsize);
        if (status == noErr) {
            horzScrollDelta = usageValue;
        }
    }

    status = HIDGetUsageValue (kHIDInputReport,
                               kHIDPage_GenericDesktop,
                               _xCollection,
                               kHIDUsage_GD_X,
                               &usageValue,
                               _preparsedReportDescriptorData,
                               mouseData,
                               ret_bufsize);
    if (status == noErr) {
        dx = usageValue;
    }

    status = HIDGetUsageValue (kHIDInputReport,
                               kHIDPage_GenericDesktop,
                               _yCollection,
                               kHIDUsage_GD_Y,
                               &usageValue,
                               _preparsedReportDescriptorData,
                               mouseData,
                               ret_bufsize);
    if (status == noErr) {
        dy = usageValue;
    }

    clock_get_uptime(&now);

    if (_absoluteCoordinates) {
        Point newLoc;

        newLoc.x = dx;
        newLoc.y = dy;

        dispatchAbsolutePointerEvent(&newLoc, &_bounds, buttonState, inRange, pressure, _tipPressureMin, _tipPressureMax, 90, now);
    } else {
        dispatchRelativePointerEvent(dx, dy, buttonState, now);
    }

    if (scrollWheelDelta != 0 || horzScrollDelta != 0) {
        dispatchScrollWheelEvent(scrollWheelDelta, horzScrollDelta, 0, now);
    }
        
    return kIOReturnSuccess;
}

// subclasses override

IOItemCount IOHIDPointing::buttonCount()
{
    return _numButtons;
}

IOFixed IOHIDPointing::resolution()
{
    return _resolution;
}


void IOHIDPointing::propagateProperties()
{
    OSData *data = NULL;
    
    if (_provider) {
        data = OSDynamicCast( OSData, _provider->getProperty( kIOHIDPointerAccelerationTableKey ));
        
        if (data)
            setProperty(kIOHIDPointerAccelerationTableKey, data);
            
        data = OSDynamicCast( OSData, _provider->getProperty( kIOHIDScrollAccelerationTableKey ));
        
        if (data)
            setProperty(kIOHIDScrollAccelerationTableKey, data);
    }

}
