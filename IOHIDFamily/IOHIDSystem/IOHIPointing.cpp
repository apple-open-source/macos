/*
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
/*
 * 17 July 	1998 	sdouglas	Initial creation
 * 01 April 	2002 	ryepez		added support for scroll acceleration
 */

#include <IOKit/IOLib.h>
#include <IOKit/assert.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <libkern/OSByteOrder.h>
#include "IOHIDSystem.h"
#include "IOHIDPointingDevice.h"

#ifndef abs
#define abs(_a)	((_a >= 0) ? _a : -_a)
#endif

#define FRAME_RATE		(67 << 16)
#define SCREEN_RESOLUTION	(96 << 16)
    
#define SCROLL_CLEAR_THRESHOLD_MS 	500
#define SCROLL_EVENT_THRESHOLD_MS 	300

#define _scrollAcceleration	((ExpansionData *)_reserved)->scrollAcceleration
#define _scrollScaleSegments 	((ExpansionData *)_reserved)->scrollScaleSegments
#define _scrollScaleSegCount 	((ExpansionData *)_reserved)->scrollScaleSegCount
#define _scrollTimeDeltas1	((ExpansionData *)_reserved)->scrollTimeDeltas1
#define _scrollTimeDeltas2	((ExpansionData *)_reserved)->scrollTimeDeltas2
#define _scrollTimeDeltas3	((ExpansionData *)_reserved)->scrollTimeDeltas3
#define _scrollTimeDeltaIndex1	((ExpansionData *)_reserved)->scrollTimeDeltaIndex1
#define _scrollTimeDeltaIndex2	((ExpansionData *)_reserved)->scrollTimeDeltaIndex2
#define _scrollTimeDeltaIndex3	((ExpansionData *)_reserved)->scrollTimeDeltaIndex3
#define _scrollLastDeltaAxis1	((ExpansionData *)_reserved)->scrollLastDeltaAxis1
#define _scrollLastDeltaAxis2	((ExpansionData *)_reserved)->scrollLastDeltaAxis2
#define _scrollLastDeltaAxis3	((ExpansionData *)_reserved)->scrollLastDeltaAxis3
#define _scrollLastEventTime1	((ExpansionData *)_reserved)->scrollLastEventTime1
#define _scrollLastEventTime2	((ExpansionData *)_reserved)->scrollLastEventTime2
#define _scrollLastEventTime3	((ExpansionData *)_reserved)->scrollLastEventTime3
#define _hidPointingNub		((ExpansionData *)_reserved)->hidPointingNub
#define _isSeized		((ExpansionData *)_reserved)->isSeized
#define _openClient		((ExpansionData *)_reserved)->openClient


static bool GetOSDataValue (OSData * data, UInt32 * value);
static bool SetupAcceleration (OSData * data, IOFixed desired, IOFixed resolution, void ** scaleSegments, IOItemCount * scaleSegCount);
static void ScaleAxes (void * scaleSegments, int * axis1p, IOFixed *axis1Fractp, int * axis2p, IOFixed *axis2Fractp);

static IOHIDPointingDevice * CreateHIDPointingDeviceNub(IOService * owner, UInt8 buttons, UInt32 resolution, bool scroll)
{
    IOHIDPointingDevice 	*nub = 0;

    nub = IOHIDPointingDevice::newPointingDevice(owner, buttons, resolution, scroll);
                        
    if (nub &&
        (!nub->attach(owner) || 
        !nub->start(owner)))
    {
        nub->release();
        nub = 0;
    }

    return nub;
}


static void DetachHIDPointingDeviceNub(IOService * owner, IOService ** nub)
{
    if ( (*nub) ) {
        (*nub)->stop(owner);
        (*nub)->detach(owner);
        
        (*nub)->release();
        (*nub) = 0;
    }
}


#define super IOHIDevice
OSDefineMetaClassAndStructors(IOHIPointing, IOHIDevice);

bool IOHIPointing::init(OSDictionary * properties)
{
    if (!super::init(properties))  return false;
    
    /*
    * Initialize minimal state.
    */
   
    _reserved = IONew(ExpansionData, 1);
   
    if (!_reserved) return false;
    
    // Initialize scroll wheel accel items
    _scrollScaleSegments 	= 0;
    _scrollScaleSegCount 	= 0;
        
    // Initialize pointer accel items
    _scaleSegments 		= 0;
    _scaleSegCount 		= 0;
    _fractX			= 0;
    _fractY     		= 0;
    
    _acceleration	= -1;
    _convertAbsoluteToRelative = false;
    _contactToMove = false;
    _hadContact = false;
    _pressureThresholdToClick = 128;
    _previousLocation.x = 0;
    _previousLocation.y = 0;
    
    _hidPointingNub = 0;
    
    _isSeized = false;
    
    // default to right mouse button generating unique events
    _buttonMode = NX_RightButton;
    
    _deviceLock = IOLockAlloc();
    
    if (!_deviceLock)  return false;
    
    return true;
}

bool IOHIPointing::start(IOService * provider)
{
  static const char * defaultSettings = "(<00000000>, <00002000>, <00005000>,"
                                         "<00008000>, <0000b000>, <0000e000>," 
                                         "<00010000>)";

  if (!super::start(provider))  return false;

  // default acceleration settings
  if (!getProperty(kIOHIDPointerAccelerationTypeKey))
    setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDMouseAccelerationType);
  if (!getProperty(kIOHIDPointerAccelerationSettingsKey))
  {
    OSObject * obj = OSUnserialize(defaultSettings, 0);
    if (obj) {
      setProperty(kIOHIDPointerAccelerationSettingsKey, obj);
      obj->release();
    }
  }

  /*
   * RY: Publish a property containing the button Count.  This will
   * will be used to determine whether or not the button
   * behaviors can be modified.
   */
  if (buttonCount() > 1)
  {
     setProperty(kIOHIDPointerButtonCountKey, buttonCount(), 32);
  }

  // create a IOHIDPointingDevice to post events to the HID Manager
  _hidPointingNub = CreateHIDPointingDeviceNub(this, buttonCount(), resolution() >> 16, false); 
        
  /*
   * IOHIPointing serves both as a service and a nub (we lead a double
   * life).  Register ourselves as a nub to kick off matching.
   */

  registerService();

  return true;
}

void IOHIPointing::stop(IOService * provider)
{
    DetachHIDPointingDeviceNub(this, &_hidPointingNub);

    super::stop(provider);
}


void IOHIPointing::free()
// Description:	Go Away. Be careful when freeing the lock.
{

    if (_deviceLock)
    {
	IOLock * lock;

	IOLockLock(_deviceLock);

	lock = _deviceLock;
	_deviceLock = NULL;

	IOLockUnlock(lock);
	IOLockFree(lock);
    }
    
    if (_reserved) {
        IODelete(_reserved, ExpansionData, 1);
    }
    
    super::free();
}

bool IOHIPointing::open(IOService *                client,
			IOOptionBits	           options,
                        RelativePointerEventAction rpeAction,
                        AbsolutePointerEventAction apeAction,
                        ScrollWheelEventAction     sweAction)
{
    if (client == this) {
        return super::open(_openClient, options);
    }
    
    return open(client, 
                options,
                0, 
                (RelativePointerEventCallback)rpeAction, 
                (AbsolutePointerEventCallback)apeAction, 
                (ScrollWheelEventCallback)sweAction);
}

bool IOHIPointing::open(IOService *			client,
                      IOOptionBits			options,
                      void *				refcon,
                      RelativePointerEventCallback	rpeCallback,
                      AbsolutePointerEventCallback	apeCallback,
                      ScrollWheelEventCallback		sweCallback)
{
    if (client == this) return true;

    _openClient = client;

    bool returnValue = open(this, options, 
                            (RelativePointerEventAction)_relativePointerEvent, 
                            (AbsolutePointerEventAction)_absolutePointerEvent, 
                            (ScrollWheelEventAction)_scrollWheelEvent);    

    if (!returnValue)
        return false;

    // Note: client object is already retained by superclass' open()
    _relativePointerEventTarget = client;
    _relativePointerEventAction = (RelativePointerEventAction)rpeCallback;
    _absolutePointerEventTarget = client;
    _absolutePointerEventAction = (AbsolutePointerEventAction)apeCallback;
    _scrollWheelEventTarget = client;
    _scrollWheelEventAction = (ScrollWheelEventAction)sweCallback;

    return true;
}

void IOHIPointing::close(IOService * client, IOOptionBits)
{
  _relativePointerEventAction = NULL;
  _relativePointerEventTarget = 0;
  _absolutePointerEventAction = NULL;
  _absolutePointerEventTarget = 0;
  super::close(client);
} 

IOReturn IOHIPointing::message( UInt32 type, IOService * provider,
                                void * argument) 
{
    IOReturn ret = kIOReturnSuccess;
    
    switch(type)
    {
        case kIOHIDSystemDeviceSeizeRequestMessage:
            if (OSDynamicCast(IOHIDDevice, provider))
            {
                _isSeized = (bool)argument;
            }
            break;
            
        default:
            ret = super::message(type, provider, argument);
            break;
    }
    
    return ret;
}

IOReturn IOHIPointing::powerStateWillChangeTo( IOPMPowerFlags powerFlags,
                        unsigned long newState, IOService * device )
{
  return( super::powerStateWillChangeTo( powerFlags, newState, device ));
}

IOReturn IOHIPointing::powerStateDidChangeTo( IOPMPowerFlags powerFlags,
                        unsigned long newState, IOService * device )
{
  return( super::powerStateDidChangeTo( powerFlags, newState, device ));
}

IOHIDKind IOHIPointing::hidKind()
{
  return kHIRelativePointingDevice;
}

struct CursorDeviceSegment {
    SInt32	devUnits;
    SInt32	slope;
    SInt32	intercept;
};
typedef struct CursorDeviceSegment CursorDeviceSegment;

static void AccelerateScrollAxis(int * 		axisp, 
                                 int *		prevScrollAxis,
                                 UInt32 * 	prevScrollTimeDeltas,
                                 UInt8 *	prevScrollTimeDeltaIndex,
                                 AbsoluteTime 	*scrollLastEventTime,
                                 IOFixed	resolution,
                                 void *		scaleSegments) 
{
    IOFixed	avgIndex;
    IOFixed	avgCount;
    IOFixed	timeDeltaMS = 0;
    IOFixed	avgTimeDeltaMS = 0;
    IOFixed	scaledTime = 0;
    UInt64	currentTimeNS = 0;
    UInt64	lastEventTimeNS = 0;
    AbsoluteTime currentTime;
    bool	directionChange;

    if (!scaleSegments)
        return;

    clock_get_uptime(&currentTime);
    
    absolutetime_to_nanoseconds(currentTime, &currentTimeNS);
    absolutetime_to_nanoseconds(*scrollLastEventTime, &lastEventTimeNS);

    *scrollLastEventTime = currentTime;

    timeDeltaMS = (currentTimeNS - lastEventTimeNS) / 1000000;
    
    // RY: To compensate for non continual motion, we have added a second
    // threshold.  This whill allow a user with a standard scroll wheel
    // to continue with acceleration when lifting the finger within a 
    // predetermined time.  We should also clear out the last time deltas
    // if the direction has changed.
    directionChange = (((*prevScrollAxis < 0) && (*axisp > 0)) || 
                        ((*prevScrollAxis > 0) && (*axisp < 0)));
    
    if ((timeDeltaMS > SCROLL_CLEAR_THRESHOLD_MS) || directionChange) {
        for (avgIndex=0; avgIndex<SCROLL_TIME_DELTA_COUNT; avgIndex++) 
            prevScrollTimeDeltas[avgIndex]=0;
        
        *prevScrollTimeDeltaIndex = 0;
    }

    timeDeltaMS = ((timeDeltaMS > SCROLL_EVENT_THRESHOLD_MS) || directionChange) ? 
                    SCROLL_EVENT_THRESHOLD_MS : timeDeltaMS;
    
    prevScrollTimeDeltas[*prevScrollTimeDeltaIndex] = timeDeltaMS;
    *prevScrollTimeDeltaIndex = (*prevScrollTimeDeltaIndex + 1) % SCROLL_TIME_DELTA_COUNT;

    // RY: To eliminate jerkyness associated with the scroll acceleration,
    // we scroll based on the average of the last n events.  This has the
    // effect of make acceleration smoother with accel and decel.
    avgCount = 0;
    avgTimeDeltaMS = 0;
    for (avgIndex=0; avgIndex < SCROLL_TIME_DELTA_COUNT; avgIndex++) {
        if (prevScrollTimeDeltas[avgIndex] == 0)
            continue;
            
        avgTimeDeltaMS += prevScrollTimeDeltas[avgIndex];
        avgCount ++;
    }
    avgTimeDeltaMS = IOFixedDivide((avgTimeDeltaMS<<16), (avgCount<<16));
      
    // RY: Since we want scroll acceleration to work with the
    // time delta and the accel curves, we have come up with
    // this approach:
    //
    // scaledTime = maxDeviceDelta - [(maxDeviceDelta / maxTimeDeltaMS) * avgTimeDeltaMS] 
    //
    // Then we must removed the scaling by doing the following:
    // 
    // scaledTime *= devScale
    //
    // The value acquired from the graph will then be multiplied
    // to the current axis delta.
    IOFixed maxTimeDeltaMS = (SCROLL_EVENT_THRESHOLD_MS << 16);
    IOFixed maxDeviceDelta = (40 << 16);
    
    IOFixed devScale 		= IOFixedDivide(resolution, FRAME_RATE);
    IOFixed crsrScale		= IOFixedDivide(SCREEN_RESOLUTION, FRAME_RATE);

    scaledTime = (maxDeviceDelta - 
                    IOFixedMultiply(
                        IOFixedDivide(maxDeviceDelta, maxTimeDeltaMS), 
                        avgTimeDeltaMS));
                        
    scaledTime = IOFixedMultiply(scaledTime, devScale);
    
    CursorDeviceSegment	*segment;

    // scale
    for(
        segment = (CursorDeviceSegment *) scaleSegments;
        scaledTime > segment->devUnits;
        segment++)	{}

    scaledTime = IOFixedDivide(
            segment->intercept + IOFixedMultiply( scaledTime, segment->slope ),
            scaledTime );
    
    scaledTime = IOFixedMultiply(scaledTime, IOFixedDivide(devScale, crsrScale)) >> 16;

    *axisp *= (scaledTime && scaleSegments) ? (scaledTime) : 1;
    *prevScrollAxis = *axisp;
}

void IOHIPointing::scaleScrollAxes(int * axis1p, int * axis2p, int * axis3p)
// Description:	This method was added to support scroll acceleration.
// 		This will make use of the same accel agorithm used
//		for pointer accel.  Since we can not ensure what
// 		scroll axis corresponds to an xyz axis, this method
// 		will scale based on only one axis.
//
//		NOTE: This will most likely change, after the scaling
//		alogrithms are modified to accomidate xyz.
// Preconditions:
// *	_deviceLock should be held on entry
{
    IOFixed	resolution = scrollResolution();
    
    // Scale axis 1
    if (*axis1p != 0)
        AccelerateScrollAxis(axis1p, 
                         &_scrollLastDeltaAxis1,
                         _scrollTimeDeltas1,
                         &_scrollTimeDeltaIndex1,
                         &_scrollLastEventTime1,
                         resolution,
                         _scrollScaleSegments);
    
    // Scale axis 2
    if (*axis2p != 0)
        AccelerateScrollAxis(axis2p, 
                         &_scrollLastDeltaAxis2,
                         _scrollTimeDeltas2,
                         &_scrollTimeDeltaIndex2,
                         &_scrollLastEventTime2,
                         resolution,
                         _scrollScaleSegments);

    // Scale axis 3
    if (*axis3p != 0)
        AccelerateScrollAxis(axis3p, 
                         &_scrollLastDeltaAxis3,
                         _scrollTimeDeltas3,
                         &_scrollTimeDeltaIndex3,
                         &_scrollLastEventTime3,
                         resolution,
                         _scrollScaleSegments);
    
}

void IOHIPointing::scalePointer(int * dxp, int * dyp)
// Description:	Perform pointer acceleration computations here.
//		Given the resolution, dx, dy, and time, compute the velocity
//		of the pointer over a Manhatten distance in inches/second.
//		Using this velocity, do a lookup in the pointerScaling table
//		to select a scaling factor. Scale dx and dy up as appropriate.
// Preconditions:
// *	_deviceLock should be held on entry
{
    ScaleAxes(_scaleSegments, dxp, &_fractX, dyp, &_fractY);
}

/*
 Routine:    Interpolate
 This routine interpolates to find a point on the line [x1,y1] [x2,y2] which
 is intersected by the line [x3,y3] [x3,y"].  The resulting y' is calculated
 by interpolating between y3 and y", towards the higher acceleration curve.
*/

static SInt32 Interpolate(  SInt32 x1, SInt32 y1,
                            SInt32 x2, SInt32 y2,
                            SInt32 x3, SInt32 y3,
                            SInt32 scale, Boolean lower )
{

    SInt32 slope;
    SInt32 intercept;
    SInt32 resultY;
    
    slope = IOFixedDivide( y2 - y1, x2 - x1 );
    intercept = y1 - IOFixedMultiply( slope, x1 );
    resultY = intercept + IOFixedMultiply( slope, x3 );
    if( lower)
        resultY = y3 - IOFixedMultiply( scale, y3 - resultY );
    else
        resultY = resultY + IOFixedMultiply( scale, y3 - resultY );

    return( resultY );
}


void IOHIPointing::setupForAcceleration( IOFixed desired )
{
    if (SetupAcceleration (copyAccelerationTable(), desired, resolution(), &_scaleSegments, &_scaleSegCount))
    {
        _acceleration = desired;
        _fractX = _fractY = 0;
    }
}

void IOHIPointing::setupScrollForAcceleration( IOFixed desired )
{
    if (SetupAcceleration (copyScrollAccelerationTable(), desired, scrollResolution(), &_scrollScaleSegments, &_scrollScaleSegCount))
    {
        _scrollAcceleration = desired;
        
        _scrollTimeDeltaIndex1	= 0;
        _scrollTimeDeltaIndex2	= 0;
        _scrollTimeDeltaIndex3	= 0;
        
        _scrollLastDeltaAxis1 = 0;
        _scrollLastDeltaAxis2 = 0;
        _scrollLastDeltaAxis3 = 0;

        for (int i=0; i<SCROLL_TIME_DELTA_COUNT; i++){
            _scrollTimeDeltas1[i] = 0;
            _scrollTimeDeltas2[i] = 0;
            _scrollTimeDeltas3[i] = 0;
        }
        
        clock_get_uptime(&_scrollLastEventTime1);
        _scrollLastEventTime3 = _scrollLastEventTime2 = _scrollLastEventTime1;
    }
}


bool IOHIPointing::resetPointer()
{
    IOLockLock( _deviceLock);

    _buttonMode = NX_RightButton;
    setupForAcceleration(0x8000);
    updateProperties();

    IOLockUnlock( _deviceLock);
    return true;
}

bool IOHIPointing::resetScroll()
{
    IOLockLock( _deviceLock);

    setupScrollForAcceleration(0x8000);

    IOLockUnlock( _deviceLock);
    return true;
}

static void ScalePressure(int *pressure, int pressureMin, int pressureMax)
{    
    *pressure = (*pressure / (pressureMax - pressureMin)) * 0xffff;
}


void IOHIPointing::dispatchAbsolutePointerEvent(Point *		newLoc,
                                                Bounds *	bounds,
                                                UInt32		buttonState,
                                                bool		proximity,
                                                int		pressure,
                                                int		pressureMin,
                                                int		pressureMax,
                                                int		stylusAngle,
                                                AbsoluteTime	ts)
{
    int buttons = 0;
    int dx, dy;
    
    IOLockLock(_deviceLock);

    if( buttonState & 1)
        buttons |= EV_LB;

    if( buttonCount() > 1) {
	if( buttonState & 2)	// any others down
            buttons |= EV_RB;
	// Other magic bit reshuffling stuff.  It seems there was space
	// left over at some point for a "middle" mouse button between EV_LB and EV_RB
	if(buttonState & 4)
            buttons |= 2;
	// Add in the rest of the buttons in a linear fasion...
	buttons |= buttonState & ~0x7;
    }

    if ((_pressureThresholdToClick < 255) && ((pressure - pressureMin) > ((pressureMax - pressureMin) * _pressureThresholdToClick / 256))) {
        buttons |= EV_LB;
    }

    if (_buttonMode == NX_OneButton) {
        if ((buttons & (EV_LB|EV_RB)) != 0) {
            buttons = EV_LB;
        }
    }

    if (_convertAbsoluteToRelative) {
        dx = newLoc->x - _previousLocation.x;
        dy = newLoc->y - _previousLocation.y;
        
        if ((_contactToMove && !_hadContact && (pressure > pressureMin)) || (abs(dx) > ((bounds->maxx - bounds->minx) / 20)) || (abs(dy) > ((bounds->maxy - bounds->miny) / 20))) {
            dx = 0;
            dy = 0;
        } else {
            scalePointer(&dx, &dy);
        }
        
        _previousLocation.x = newLoc->x;
        _previousLocation.y = newLoc->y;
    }

    IOLockUnlock(_deviceLock);

    _hadContact = (pressure > pressureMin);

    if (!_contactToMove || (pressure > pressureMin)) {
        pressure -= pressureMin;
        
        ScalePressure(&pressure, pressureMin, pressureMax);

        if (_convertAbsoluteToRelative) {
            _relativePointerEvent(  this,
                                    buttons,
                                    dx,
                                    dy,
                                    ts);
        } else {
            _absolutePointerEvent(  this,
                                    buttons,
                                    newLoc,
                                    bounds,
                                    proximity,
                                    pressure,
                                    stylusAngle,
                                    ts);
        }
    }

    return;
}

void IOHIPointing::dispatchRelativePointerEvent(int        dx,
                                                int        dy,
                                                UInt32     buttonState,
                                                AbsoluteTime ts)
{
    int buttons;

    IOLockLock( _deviceLock);

    // post the raw event to the IOHIDPointingDevice
    if (_hidPointingNub)
        _hidPointingNub->postMouseEvent(buttonState, dx, dy, 0);
        
    if (_isSeized)
    {
        IOLockUnlock( _deviceLock);
        return;
    }
    
    buttons = 0;

    if( buttonState & 1)
        buttons |= EV_LB;

    if( buttonCount() > 1) {
	if( buttonState & 2)	// any others down
            buttons |= EV_RB;
	// Other magic bit reshuffling stuff.  It seems there was space
	// left over at some point for a "middle" mouse button between EV_LB and EV_RB
	if(buttonState & 4)
            buttons |= 2;
	// Add in the rest of the buttons in a linear fasion...
	buttons |= buttonState & ~0x7;
    }

    // Perform pointer acceleration computations
    scalePointer(&dx, &dy);

    // Perform button tying and mapping.  This
    // stuff applies to relative posn devices (mice) only.
    if ( _buttonMode == NX_OneButton )
    {
	// Remap both Left and Right (but no others?) to Left.
	if ( (buttons & (EV_LB|EV_RB)) != 0 ) {
            buttons |= EV_LB;
            buttons &= ~EV_RB;
	}
    }
    else if ( (buttonCount() > 1) && (_buttonMode == NX_LeftButton) )	
    // Menus on left button. Swap!
    {
	int temp = 0;
	if ( buttons & EV_LB )
	    temp = EV_RB;
	if ( buttons & EV_RB )
	    temp |= EV_LB;
	// Swap Left and Right, preserve everything else
	buttons = (buttons & ~(EV_LB|EV_RB)) | temp;
    }
    IOLockUnlock( _deviceLock);

    _relativePointerEvent(this,
            /* buttons */ buttons,
            /* deltaX */  dx,
            /* deltaY */  dy,
            /* atTime */  ts);
}

void IOHIPointing::dispatchScrollWheelEvent(short deltaAxis1,
                                            short deltaAxis2,
                                            short deltaAxis3,
                                            AbsoluteTime ts)
{
    
    IOLockLock( _deviceLock);
     
    // Change the report descriptor for the IOHIDPointingDevice
    // to include a scroll whell
    if (_hidPointingNub && !_hidPointingNub->isScrollPresent())
    {
        DetachHIDPointingDeviceNub(this, &_hidPointingNub);
        _hidPointingNub = CreateHIDPointingDeviceNub(this, buttonCount(), resolution() >> 16, true);
    }

    // Post the raw event to IOHIDPointingDevice
    if (_hidPointingNub)
        _hidPointingNub->postMouseEvent(0, 0, 0, deltaAxis1);
        
    if (_isSeized)
    {
        IOLockUnlock( _deviceLock);
        return;
    }
        
    // scaleScrollAxes is expecting ints.  Since
    // shorts are smaller than ints, we cannot
    // cast a short to an int.
    int dAxis1 = deltaAxis1;
    int dAxis2 = deltaAxis2;
    int dAxis3 = deltaAxis3;
    
    // Perform pointer acceleration computations
    scaleScrollAxes(&dAxis1, &dAxis2, &dAxis3);
    
    IOLockUnlock( _deviceLock);
    
    _scrollWheelEvent(	this,
                        (short) dAxis1,
                        (short) dAxis2,
                        (short) dAxis3,
                        ts);
}

bool IOHIPointing::updateProperties( void )
{
    bool	ok;
    UInt32	res = resolution();

    ok = setProperty( kIOHIDPointerResolutionKey, &res, sizeof( res))
    &    setProperty( kIOHIDPointerConvertAbsoluteKey, &_convertAbsoluteToRelative,
                        sizeof( _convertAbsoluteToRelative))
    &    setProperty( kIOHIDPointerContactToMoveKey, &_contactToMove,
                        sizeof( _contactToMove));

    return( ok & super::updateProperties() );
}

IOReturn IOHIPointing::setParamProperties( OSDictionary * dict )
{
    OSData *		data;
    OSNumber * 		number;
    OSString *  	accelKey;
    IOReturn		err = kIOReturnSuccess;
    bool		updated = false;
    UInt8 *		bytes;
    UInt32		value;

    if( dict->getObject(kIOHIDResetPointerKey))
	resetPointer();

    accelKey = OSDynamicCast( OSString, getProperty(kIOHIDPointerAccelerationTypeKey));

    IOLockLock( _deviceLock);
    
    if( accelKey && 
        ((number = OSDynamicCast( OSNumber, dict->getObject(accelKey))) || 
         (data = OSDynamicCast( OSData, dict->getObject(accelKey))))) 
    {
        value = (number) ? number->unsigned32BitValue() : 
                            *((UInt32 *) (data->getBytesNoCopy()));
        setupForAcceleration( value );
        updated = true;
    } 
    else if( (number = OSDynamicCast( OSNumber,
		dict->getObject(kIOHIDPointerAccelerationKey))) ||
             (data = OSDynamicCast( OSData,
		dict->getObject(kIOHIDPointerAccelerationKey)))) {

        value = (number) ? number->unsigned32BitValue() : 
                            *((UInt32 *) (data->getBytesNoCopy()));
                            
	setupForAcceleration( value );
	updated = true;
        if( accelKey) {
            // If this is an OSData object, create an OSNumber to store in the registry
            if (!number)
            {
                number = OSNumber::withNumber(value, 32);
        	dict->setObject( accelKey, number );
                number->release();
            }
            else
                dict->setObject( accelKey, number );
        }

    }
    
    // Scroll accel setup
    if( dict->getObject(kIOHIDScrollResetKey))
	resetScroll();
        
    if ((number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDScrollAccelerationKey))) ||
        (data = OSDynamicCast( OSData, dict->getObject(kIOHIDScrollAccelerationKey))))
    {
        value = (number) ? number->unsigned32BitValue() : *((UInt32 *) (data->getBytesNoCopy()));
        setupScrollForAcceleration( value );
    }

    IOLockUnlock( _deviceLock);

    if ((number = OSDynamicCast(OSNumber,
                              dict->getObject(kIOHIDPointerConvertAbsoluteKey))) ||
        (data = OSDynamicCast(OSData,
                              dict->getObject(kIOHIDPointerConvertAbsoluteKey))))
    {
        value = (number) ? number->unsigned32BitValue() : *((UInt32 *) (data->getBytesNoCopy()));
        _convertAbsoluteToRelative = (value != 0) ? true : false;
        updated = true;
    }

    if ((number = OSDynamicCast(OSNumber,
                              dict->getObject(kIOHIDPointerContactToMoveKey))) ||
        (data = OSDynamicCast(OSData,
                              dict->getObject(kIOHIDPointerContactToMoveKey))))
    {
        value = (number) ? number->unsigned32BitValue() : *((UInt32 *) (data->getBytesNoCopy()));
        _contactToMove = (value != 0) ? true : false;
        updated = true;
    }

    if ((number = OSDynamicCast(OSNumber, dict->getObject(kIOHIDPointerButtonMode))) ||
        (data = OSDynamicCast(OSData, dict->getObject(kIOHIDPointerButtonMode))))
	{
		value = (number) ? number->unsigned32BitValue() : 
                                            *((UInt32 *) (data->getBytesNoCopy())) ;
        
		if (getProperty(kIOHIDPointerButtonCountKey))
		{
			if (value == kIOHIDButtonMode_BothLeftClicks)
				_buttonMode = NX_OneButton;
			else if (value == kIOHIDButtonMode_ReverseLeftRightClicks)
				_buttonMode = NX_LeftButton;
			else if (value == kIOHIDButtonMode_EnableRightClick)
				_buttonMode = NX_RightButton;
			else
				_buttonMode = value;

			updated = true;
		}
    }

    if( updated )
        updateProperties();

    return( err );
}

// subclasses override

IOItemCount IOHIPointing::buttonCount()
{
    return (1);
}

IOFixed IOHIPointing::resolution()
{
    return (100 << 16);
}

// RY: Added this method to obtain the resolution
// of the scroll wheel.  The default value is 0, 
// which should prevent any accel from being applied.
IOFixed	IOHIPointing::scrollResolution()
{
    IOFixed	resolution = 0;
    OSNumber * 	number = OSDynamicCast( OSNumber,
                getProperty( kIOHIDScrollResolutionKey ));
                
    if( number )
        resolution = number->unsigned32BitValue();
        
    return( resolution );
}

OSData * IOHIPointing::copyAccelerationTable()
{
    static const UInt8 accl[] = {
	0x00, 0x00, 0x80, 0x00, 
        0x40, 0x32, 0x30, 0x30, 0x00, 0x02, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 
        0x00, 0x09, 0x00, 0x00, 0x71, 0x3B, 0x00, 0x00,
        0x60, 0x00, 0x00, 0x04, 0x4E, 0xC5, 0x00, 0x10, 
        0x80, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x5F,
        0x00, 0x00, 0x00, 0x16, 0xEC, 0x4F, 0x00, 0x8B, 
        0x00, 0x00, 0x00, 0x1D, 0x3B, 0x14, 0x00, 0x94,
        0x80, 0x00, 0x00, 0x22, 0x76, 0x27, 0x00, 0x96, 
        0x00, 0x00, 0x00, 0x24, 0x62, 0x76, 0x00, 0x96,
        0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x96, 
        0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x96,
        0x00, 0x00
    };
    
    OSData * data = OSDynamicCast( OSData,
                getProperty( kIOHIDPointerAccelerationTableKey ));
    if( data)
        data->retain();
    else
        data = OSData::withBytesNoCopy( (void *) accl, sizeof( accl ) );
        
    return( data );
}

// RY: Added for scroll acceleration
// If no scroll accel table is present, this will 
// default to the pointer acceleration table
OSData * IOHIPointing::copyScrollAccelerationTable()
{
    OSData * data = OSDynamicCast( OSData,
                getProperty( kIOHIDScrollAccelerationTableKey ));
    if( data)
        data->retain();
        
    else
        data = copyAccelerationTable();
        
    return( data );
}

bool GetOSDataValue (OSData * data, UInt32 * value)
{
	bool 	validValue = false;
	
	switch (data->getLength())
	{
		case sizeof(UInt8):
			*value = *(UInt8 *) data->getBytesNoCopy();
			validValue = true;
			break;
			
		case sizeof(UInt16):
			*value = *(UInt16 *) data->getBytesNoCopy();
			validValue = true;
			break;
			
		case sizeof(UInt32):
			*value = *(UInt32 *) data->getBytesNoCopy();
			validValue = true;
			break;
	}
	
	return validValue;
}
 
// RY: This function contains the original portions of 
// setupForAcceleration.  This was separated out to 
// accomidate the acceleration of scroll axes
bool SetupAcceleration (OSData * data, IOFixed desired, IOFixed resolution, void ** scaleSegments, IOItemCount * scaleSegCount) {
    const UInt16 *	lowTable = 0;
    const UInt16 *	highTable;

    SInt32	x1, y1, x2, y2, x3, y3;
    SInt32	prevX1, prevY1;
    SInt32	upperX, upperY;
    SInt32	lowerX, lowerY;
    SInt32	lowAccl = 0, lowPoints = 0;
    SInt32	highAccl, highPoints;
    SInt32	scale;
    UInt32	count;
    Boolean	lower;

    SInt32	devScale, crsrScale;
    SInt32	scaledX1, scaledY1;
    SInt32	scaledX2, scaledY2;

    CursorDeviceSegment *	segments;
    CursorDeviceSegment *	segment;
    SInt32			segCount;

    if( !data || !resolution)
        return false;

    if( desired < (IOFixed) 0) {
        // disabling mouse scaling
        if(*scaleSegments && *scaleSegCount)
            IODelete( *scaleSegments,
                        CursorDeviceSegment, *scaleSegCount );
        *scaleSegments = NULL;
        *scaleSegCount = 0;
        data->release();
        return false;
    }
	
    highTable = (const UInt16 *) data->getBytesNoCopy();

    devScale = IOFixedDivide( resolution, FRAME_RATE );
    crsrScale = IOFixedDivide( SCREEN_RESOLUTION, FRAME_RATE );

    scaledX1 = scaledY1 = 0;

    scale = OSReadBigInt32(highTable, 0);
    highTable += 4;

    // normalize table's default (scale) to 0.5
    if( desired > 0x8000) {
        desired = IOFixedMultiply( desired - 0x8000,
                                   0x10000 - scale );
        desired <<= 1;
        desired += scale;
    } else {
        desired = IOFixedMultiply( desired, scale );
        desired <<= 1;
    }

    count = OSReadBigInt16(highTable++, 0);
    scale = (1 << 16);

    // find curves bracketing the desired value
    do {
        highAccl = OSReadBigInt32(highTable, 0);
        highTable += 2;
        highPoints = OSReadBigInt16(highTable++, 0);

        if( desired <= highAccl)
            break;

        if( 0 == --count) {
            // this much over the highest table
            scale = (highAccl) ? IOFixedDivide( desired, highAccl ) : 0;
            lowTable	= 0;
            break;
        }
            
        lowTable	= highTable;
        lowAccl		= highAccl;
        lowPoints	= highPoints;
        highTable	+= lowPoints * 4;

    } while( true );

    // scale between the two
    if( lowTable) {
        scale = (highAccl == lowAccl) ? 0 : 
                IOFixedDivide((desired - lowAccl), (highAccl - lowAccl));
                            
    }
                        
    // or take all the high one
    else {
        lowTable	= highTable;
        lowAccl		= highAccl;
        lowPoints	= 0;
    }

    if( lowPoints > highPoints)
        segCount = lowPoints;
    else
        segCount = highPoints;
    segCount *= 2;
/*    IOLog("lowPoints %ld, highPoints %ld, segCount %ld\n",
            lowPoints, highPoints, segCount); */
    segments = IONew( CursorDeviceSegment, segCount );
    assert( segments );
    segment = segments;

    x1 = prevX1 = y1 = prevY1 = 0;

    lowerX = OSReadBigInt32(lowTable, 0);
    lowTable += 2;
    lowerY = OSReadBigInt32(lowTable, 0);
    lowTable += 2;
    upperX = OSReadBigInt32(highTable, 0);
    highTable += 2;
    upperY = OSReadBigInt32(highTable, 0);
    highTable += 2;

    do {
        // consume next point from first X
        lower = (lowPoints && (!highPoints || (lowerX <= upperX)));

        if( lower) {
            /* highline */
            x2 = upperX;
            y2 = upperY;
            x3 = lowerX;
            y3 = lowerY;
            if( lowPoints && (--lowPoints)) {
                lowerX = OSReadBigInt32(lowTable, 0);
                lowTable += 2;
                lowerY = OSReadBigInt32(lowTable, 0);
                lowTable += 2;
            }
        } else  {
            /* lowline */
            x2 = lowerX;
            y2 = lowerY;
            x3 = upperX;
            y3 = upperY;
            if( highPoints && (--highPoints)) {
                upperX = OSReadBigInt32(highTable, 0);
                highTable += 2;
                upperY = OSReadBigInt32(highTable, 0);
                highTable += 2;
            }
        }
        {
        // convert to line segment
        assert( segment < (segments + segCount) );

        scaledX2 = IOFixedMultiply( devScale, /* newX */ x3 );
        scaledY2 = IOFixedMultiply( crsrScale,
                      /* newY */    Interpolate( x1, y1, x2, y2, x3, y3,
                                            scale, lower ) );
        if( lowPoints || highPoints)
            segment->devUnits = scaledX2;
        else
            segment->devUnits = 0x7fffffff;
            
        segment->slope = ((scaledX2 == scaledX1)) ? 0 : 
                IOFixedDivide((scaledY2 - scaledY1), (scaledX2 - scaledX1));

        segment->intercept = scaledY2
                            - IOFixedMultiply( segment->slope, scaledX2 );
/*        IOLog("devUnits = %08lx, slope = %08lx, intercept = %08lx\n",
                segment->devUnits, segment->slope, segment->intercept); */

        scaledX1 = scaledX2;
        scaledY1 = scaledY2;
        segment++;
        }

        // continue on from last point
        if( lowPoints && highPoints) {
            if( lowerX > upperX) {
                prevX1 = x1;
                prevY1 = y1;
            } else {
                /* swaplines */
                prevX1 = x1;
                prevY1 = y1;
                x1 = x3;
                y1 = y3;
            }
        } else {
            x2 = x1;
            y2 = y1;
            x1 = prevX1;
            y1 = prevY1;
            prevX1 = x2;
            prevY1 = y2;
        }

    } while( lowPoints || highPoints );
    
    if( *scaleSegCount && *scaleSegments)
        IODelete( *scaleSegments,
                    CursorDeviceSegment, *scaleSegCount );
    *scaleSegCount = segCount;
    *scaleSegments = (void *) segments;

    data->release();
    
    return true;
}
	
// RY: This function contains the original portions of 
// scalePointer.  This was separated out to accomidate 
// the acceleration of other axes
void ScaleAxes (void * scaleSegments, int * axis1p, IOFixed *axis1Fractp, int * axis2p, IOFixed *axis2Fractp)
{
    SInt32			dx, dy;
    SInt32			absDx, absDy;
    SInt32			mag;
    IOFixed			scale;
    CursorDeviceSegment	*	segment;

    if( !scaleSegments)
        return;

    dx = (*axis1p) << 16;
    dy = (*axis2p) << 16;
    absDx = (dx < 0) ? -dx : dx;
    absDy = (dy < 0) ? -dy : dy;

    if( absDx > absDy)
	mag = (absDx + (absDy / 2));
    else
	mag = (absDy + (absDx / 2));

    if( !mag)
        return;

    // scale
    for(
        segment = (CursorDeviceSegment *) scaleSegments;
        mag > segment->devUnits;
        segment++)	{}
    
    scale = IOFixedDivide(
            segment->intercept + IOFixedMultiply( mag, segment->slope ),
            mag );
    
    
    dx = IOFixedMultiply( dx, scale );
    dy = IOFixedMultiply( dy, scale );

    // add fract parts
    dx += *axis1Fractp;
    dy += *axis2Fractp;

    *axis1p = dx / 65536;
    *axis2p = dy / 65536;

    // get fractional part with sign extend
    if( dx >= 0)
	*axis1Fractp = dx & 0xffff;
    else
	*axis1Fractp = dx | 0xffff0000;
    if( dy >= 0)
	*axis2Fractp = dy & 0xffff;
    else
	*axis2Fractp = dy | 0xffff0000;
} 

void IOHIPointing::_relativePointerEvent( IOHIPointing * self,
				    int        buttons,
                       /* deltaX */ int        dx,
                       /* deltaY */ int        dy,
                       /* atTime */ AbsoluteTime ts)
{
    RelativePointerEventCallback rpeCallback;
    rpeCallback = (RelativePointerEventCallback)self->_relativePointerEventAction;

    if (rpeCallback)
        (*rpeCallback)(self->_relativePointerEventTarget,
                                    buttons,
                                    dx,
                                    dy,
                                    ts,
                                    self,
                                    0);
}

  /* Tablet event reporting */
void IOHIPointing::_absolutePointerEvent(IOHIPointing * self,
				    int        buttons,
                 /* at */           Point *    newLoc,
                 /* withBounds */   Bounds *   bounds,
                 /* inProximity */  bool       proximity,
                 /* withPressure */ int        pressure,
                 /* withAngle */    int        stylusAngle,
                 /* atTime */       AbsoluteTime ts)
{
    AbsolutePointerEventCallback apeCallback;
    apeCallback = (AbsolutePointerEventCallback)self->_absolutePointerEventAction;

    if (apeCallback)
        (*apeCallback)(self->_absolutePointerEventTarget,
                                buttons,
                                newLoc,
                                bounds,
                                proximity,
                                pressure,
                                stylusAngle,
                                ts,
                                self,
                                0);

}

  /* Mouse scroll wheel event reporting */
void IOHIPointing::_scrollWheelEvent(IOHIPointing *self,
                                short deltaAxis1,
                                short deltaAxis2,
                                short deltaAxis3,
                                AbsoluteTime ts)
{
    ScrollWheelEventCallback sweCallback;
    sweCallback = (ScrollWheelEventCallback)self->_scrollWheelEventAction;
    
    if (sweCallback)
        (*sweCallback)(self->_scrollWheelEventTarget,
                                (short) deltaAxis1,
                                (short) deltaAxis2,
                                (short) deltaAxis3,
                                ts,
                                self,
                                0);
}
