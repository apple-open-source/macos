/*
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
/*
 * 07 Jan 2002 		ryepez
 *			This class is based off IOHIPointing and handles
 *			USB HID report based pointing devices
 */

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>

#include "IOHIDParameter.h"
#include "IOHIDPointing.h"
#include "IOHIDKeys.h"
#include "IOHIDElement.h"

#define super IOHITablet
OSDefineMetaClassAndStructors(IOHIDPointing, IOHITablet);

//====================================================================================================
// IOHIDPointing::Pointing
//====================================================================================================
IOHIDPointing * IOHIDPointing::Pointing(
                                UInt32          buttonCount,
                                IOFixed         pointerResolution,
                                IOFixed         scrollResolution,
                                bool            isDispatcher)
{
    IOHIDPointing 	*nub 			= new IOHIDPointing;    
    
    if ((nub == 0) || !nub->initWithMouseProperties(buttonCount, pointerResolution, scrollResolution, isDispatcher) )
    {
        if (nub) nub->release();
        return 0;
    }
        
    return nub;
}


//====================================================================================================
// IOHIDPointing::initWithMouseProperties
//====================================================================================================
bool IOHIDPointing::initWithMouseProperties(
                                UInt32          buttonCnt,
                                IOFixed         pointerResolution,
                                IOFixed         scrollResolution,
                                bool            isDispatcher)
{
    if (!super::init(0))  return false;
    
    _numButtons         = (buttonCnt > 0) ? buttonCnt : 1;
    _resolution         = pointerResolution;
    _scrollResolution   = scrollResolution;
    _isDispatcher       = isDispatcher;
    
    return true;
}

//====================================================================================================
// IOHIDPointing::start
//====================================================================================================
bool IOHIDPointing::start(IOService *provider)
{

	_provider = OSDynamicCast(IOHIDEventService, provider);
	
	if (!_provider)
		return false;
		
    // push up properties from our provider
    setupProperties();
	
    return super::start(provider);
}

//====================================================================================================
// IOHIDPointing::dispatchAbsolutePointerEvent
//====================================================================================================
void IOHIDPointing::dispatchAbsolutePointerEvent(
                                AbsoluteTime                timeStamp,
                                IOGPoint *                  newLoc,
                                IOGBounds *                 bounds,
                                UInt32                      buttonState,
                                bool                        inRange,
                                SInt32                      tipPressure,
                                SInt32                      tipPressureMin,
                                SInt32                      tipPressureMax,
                                IOOptionBits                options)
{
    bool    convertToRelative   = ((options & kHIDDispatchOptionPointerAbsolutToRelative) != 0);
    bool    accelerate          = ((options & kHIDDispatchOptionPointerNoAcceleration) == 0);
    UInt32  pointingMode        = getPointingMode();
    
    if ( (((pointingMode & kAccelMouse) != 0) != accelerate) && (((pointingMode & kAbsoluteConvertMouse) != 0) != convertToRelative))
    {
        if ( accelerate )
            pointingMode |= kAccelMouse;
        else
            pointingMode &= ~kAccelMouse;
            
        if ( convertToRelative )
            pointingMode |= kAbsoluteConvertMouse;
        else
            pointingMode &= ~kAbsoluteConvertMouse;
            
        setPointingMode(pointingMode);
    }

	super::dispatchAbsolutePointerEvent(newLoc, bounds, buttonState, inRange, tipPressure, tipPressureMin, tipPressureMax, 90, timeStamp);
}

//====================================================================================================
// IOHIDPointing::dispatchRelativePointerEvent
//====================================================================================================
void IOHIDPointing::dispatchRelativePointerEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      dx,
                                SInt32                      dy,
                                UInt32                      buttonState,
                                IOOptionBits                options)
{
    bool    accelerate      = ((options & kHIDDispatchOptionPointerNoAcceleration) == 0);
    UInt32  pointingMode    = getPointingMode();
    
    if ( ((pointingMode & kAccelMouse) != 0) != accelerate)
    {
        if ( accelerate )
            pointingMode |= kAccelMouse;
        else
            pointingMode &= ~kAccelMouse;
            
        setPointingMode(pointingMode);
    }

	super::dispatchRelativePointerEvent(dx, dy, buttonState, timeStamp);
}

//====================================================================================================
// IOHIDPointing::dispatchScrollWheelEvent
//====================================================================================================
void IOHIDPointing::dispatchScrollWheelEvent(
                                AbsoluteTime                timeStamp,
                                SInt32                      deltaAxis1,
                                SInt32                      deltaAxis2,
                                UInt32                      deltaAxis3,
                                IOOptionBits                options)
{
    // no good initial check
    {
        UInt32  oldEventType    = getScrollType();
        UInt32  newEventType    = oldEventType & ~( kScrollTypeMomentumAny | kScrollTypeOptionPhaseAny );
        bool    setScroll       = false;
        
        UInt32 dispatchKey = kHIDDispatchOptionScrollMomentumContinue;
        UInt32 eventKey    = kScrollTypeMomentumContinue;
        bool   dispatchVal = options & dispatchKey ? true : false;
        bool   eventVal    = oldEventType & eventKey ? true : false;
        if (dispatchVal != eventVal) {
            if (dispatchVal) {
                newEventType |= eventKey;
            }
            setScroll = true;
        }
        
        dispatchKey = kHIDDispatchOptionScrollMomentumStart;
        eventKey    = kScrollTypeMomentumStart;
        dispatchVal = options & dispatchKey ? true : false;
        eventVal    = oldEventType & eventKey ? true : false;
        if (dispatchVal != eventVal) {
            if (dispatchVal) {
                newEventType |= eventKey;
            }
            setScroll = true;
        }
        
        dispatchKey = kHIDDispatchOptionScrollMomentumEnd;
        eventKey    = kScrollTypeMomentumEnd;
        dispatchVal = options & dispatchKey ? true : false;
        eventVal    = oldEventType & eventKey ? true : false;
        if (dispatchVal != eventVal) {
            if (dispatchVal) {
                newEventType |= eventKey;
            }
            setScroll = true;
        }
        
        // Slight idiom change here because kHIDDispatchOptionPhaseAny << 4 == kScrollTypeOptionPhaseAny 
        dispatchKey = (options & kHIDDispatchOptionPhaseAny) << 4;
        eventKey    = (oldEventType & kScrollTypeOptionPhaseAny);
        if (dispatchKey != eventKey) {
            newEventType |= dispatchKey;
            setScroll = true;
        }
        
        if (setScroll) {
            setScrollType(newEventType);
        }
    }
        
    bool    accelerate      = ((options & kHIDDispatchOptionScrollNoAcceleration) == 0);
    UInt32  pointingMode    = getPointingMode();

    if (((pointingMode & kAccelScroll) != 0) != accelerate)
    {
        if ( accelerate )
            pointingMode |= kAccelScroll;
        else
            pointingMode &= ~kAccelScroll;
            
        setPointingMode(pointingMode);
    }
    
	super::dispatchScrollWheelEvent(deltaAxis1, deltaAxis2, deltaAxis3, timeStamp);
}

//====================================================================================================
// IOHIDPointing::generateDeviceID
//====================================================================================================
UInt16 IOHIDPointing::generateDeviceID()
{
    static UInt16 sNextDeviceID = 0x8000;
    return sNextDeviceID++;
}
                                
//====================================================================================================
// IOHIDPointing::dispatchTabletPointEvent
//====================================================================================================
void IOHIDPointing::dispatchTabletEvent(
                                    NXEventData *           tabletEvent,
                                    AbsoluteTime            ts)
{
    super::dispatchTabletEvent(tabletEvent, ts);
}

//====================================================================================================
// IOHIDPointing::dispatchTabletProximityEvent
//====================================================================================================
void IOHIDPointing::dispatchProximityEvent(
                                    NXEventData *           proximityEvent,
                                    AbsoluteTime            ts)
{
    super::dispatchProximityEvent(proximityEvent, ts);
}


// subclasses override

//====================================================================================================
// IOHIDPointing::buttonCount
//====================================================================================================
IOItemCount IOHIDPointing::buttonCount()
{
    return _numButtons;
}

//====================================================================================================
// IOHIDPointing::resolution
//====================================================================================================
IOFixed IOHIDPointing::resolution()
{
    return _resolution;
}


//====================================================================================================
// IOHIDPointing::setupProperties
//====================================================================================================
void IOHIDPointing::setupProperties()
{
    OSNumber *  number  = NULL;
    
	// Store the resolution
    if ( number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDPointerResolutionKey)) )
    {
        IOFixed newResolution = number->unsigned32BitValue();
        if ( newResolution != 0 ) {
            _resolution = number->unsigned32BitValue();
            setProperty(kIOHIDPointerResolutionKey, number);
            _isDispatcher = FALSE;
        }
    }
    else if ( _resolution )
    {
        setProperty(kIOHIDPointerResolutionKey, _resolution, 32);
    }
    
	// Store the scroll resolution
    if ( number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDScrollResolutionKey)) )
    {
        _scrollResolution = number->unsigned32BitValue();
        setProperty(kIOHIDScrollResolutionKey, number);
		_isDispatcher = FALSE;
    }
    else if ( _scrollResolution )
    {
        setProperty(kIOHIDScrollResolutionKey, _scrollResolution, 32);
    }
	
	// deal with buttons
	if ( _numButtons == 1 && (number = OSDynamicCast(OSNumber, _provider->getProperty(kIOHIDPointerButtonCountKey))))
	{
		_numButtons = number->unsigned32BitValue();
		_isDispatcher = FALSE;
	}

    if ( _isDispatcher )
        setProperty(kIOHIDVirtualHIDevice, kOSBooleanTrue);

    setProperty(kIOHIDScrollAccelerationTypeKey, _provider->getProperty( kIOHIDScrollAccelerationTypeKey ));
    setProperty(kIOHIDPointerAccelerationTypeKey, _provider->getProperty( kIOHIDPointerAccelerationTypeKey ));
        
    setProperty(kIOHIDPointerAccelerationTableKey, _provider->getProperty( kIOHIDPointerAccelerationTableKey ));
    setProperty(kIOHIDScrollAccelerationTableKey, _provider->getProperty( kIOHIDScrollAccelerationTableKey ));
    setProperty(kIOHIDScrollAccelerationTableXKey, _provider->getProperty( kIOHIDScrollAccelerationTableXKey ));
    setProperty(kIOHIDScrollAccelerationTableYKey, _provider->getProperty( kIOHIDScrollAccelerationTableYKey ));
    setProperty(kIOHIDScrollAccelerationTableZKey, _provider->getProperty( kIOHIDScrollAccelerationTableZKey ));
    
    setProperty(kIOHIDScrollReportRateKey, _provider->getProperty( kIOHIDScrollReportRateKey ));

    setProperty( kIOHIDScrollMouseButtonKey, _provider->getProperty( kIOHIDScrollMouseButtonKey ));
    
    setProperty(kIOHIDScrollResolutionXKey, _provider->getProperty( kIOHIDScrollResolutionXKey ));
    setProperty(kIOHIDScrollResolutionYKey, _provider->getProperty( kIOHIDScrollResolutionYKey ));
    setProperty(kIOHIDScrollResolutionZKey, _provider->getProperty( kIOHIDScrollResolutionZKey ));
}
