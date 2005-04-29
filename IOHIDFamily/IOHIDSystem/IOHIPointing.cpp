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
#include "IOHIDevicePrivateKeys.h"

#ifndef abs
#define abs(_a)	((_a >= 0) ? _a : -_a)
#endif

#ifndef IOFixedSquared
#define IOFixedSquared(a) IOFixedMultiply(a, a)
#endif

#define FRAME_RATE                  (67 << 16)
#define SCREEN_RESOLUTION           (96 << 16)

#define MAX_DEVICE_THRESHOLD        0x7fffffff
    
#define SCROLL_DEFUALT_RESOLUTION   (7 << 16)
#define SCROLL_CLEAR_THRESHOLD_MS   (500 << 16)
#define SCROLL_EVENT_THRESHOLD_MS   (150 << 16)
#define SCROLL_MULTIPLIER_RANGE     0x00030000
#define SCROLL_MULTIPLIER_MULTIPLIER 0x0000008 /*IOFixedDivide(SCROLL_MULTIPLIER_RANGE, IOFixedSquared(SCROLL_EVENT_THRESHOLD_MS+(1<<16)))*/

#define SCROLL_POINTER_COALESCE_CLEAR_THRESHOLD         8
#define SCROLL_POINTER_COALESCE_COUNTER_THRESHOLD       24

#define CONVERT_SCROLL_WHEEL_SCALE  IOFixedDivide(SCREEN_RESOLUTION, SCROLL_DEFUALT_RESOLUTION)

#define CONVERT_SCROLL_FIXED_TO_COARSE(fixedAxis, coarse)   \
        if((fixedAxis < 0) && (fixedAxis & 0xffff))         \
            coarse = (fixedAxis >> 16) + 1;                 \
        else                                                \
            coarse = (fixedAxis >> 16);                     \
        if (!coarse && (fixedAxis & 0xffff))                \
            coarse = (fixedAxis < 0) ? -1 : 1;
            
#define CONVERT_SCROLL_FIXED_TO_FRACTION(fixed, fraction)   \
        if( fixed >= 0)                                     \
            fraction = fixed & 0xffff;                      \
        else                                                \
            fraction = fixed | 0xffff0000;                  


#define _scrollScaleSegments                _reserved->scrollScaleSegments
#define _scrollScaleSegCount                _reserved->scrollScaleSegCount
#define _scrollDeltaTime                    _reserved->scrollDeltaTime
#define _scrollDeltaAxis                    _reserved->scrollDeltaAxis
#define _scrollDeltaIndex                   _reserved->scrollDeltaIndex
#define _scrollLastEventTime                _reserved->scrollLastEventTime
#define _scrollPixelScaleSegments           _reserved->scrollPixelScaleSegments
#define _scrollPixelScaleSegCount           _reserved->scrollPixelScaleSegCount
#define _scrollPixelDeltaTime               _reserved->scrollPixelDeltaTime
#define _scrollPixelDeltaAxis               _reserved->scrollPixelDeltaAxis
#define _scrollPixelDeltaIndex              _reserved->scrollPixelDeltaIndex
#define _scrollPixelLastEventTime           _reserved->scrollPixelLastEventTime
#define _scrollPointerScaleSegments         _reserved->scrollPointerScaleSegments
#define _scrollPointerScaleSegCount         _reserved->scrollPointerScaleSegCount
#define _scrollPointerDeltaTime             _reserved->scrollPointerDeltaTime
#define _scrollPointerDeltaAxis             _reserved->scrollPointerDeltaAxis
#define _scrollPointerDeltaIndex            _reserved->scrollPointerDeltaIndex
#define _scrollPointerLastEventTime         _reserved->scrollPointerLastEventTime
#define _scrollPointerPixelScaleSegments    _reserved->scrollPointerPixelScaleSegments
#define _scrollPointerPixelScaleSegCount    _reserved->scrollPointerPixelScaleSegCount
#define _scrollPointerPixelDeltaTime        _reserved->scrollPointerPixelDeltaTime
#define _scrollPointerPixelDeltaAxis        _reserved->scrollPointerPixelDeltaAxis
#define _scrollPointerPixelDeltaIndex       _reserved->scrollPointerPixelDeltaIndex
#define _scrollPointerPixelLastEventTime    _reserved->scrollPointerPixelLastEventTime
#define _scrollFixedDeltaAxis1              _reserved->scrollFixedDeltaAxis1
#define _scrollFixedDeltaAxis2              _reserved->scrollFixedDeltaAxis2
#define _scrollFixedDeltaAxis3              _reserved->scrollFixedDeltaAxis3
#define _scrollPointDeltaAxis1              _reserved->scrollPointDeltaAxis1
#define _scrollPointDeltaAxis2              _reserved->scrollPointDeltaAxis2
#define _scrollPointDeltaAxis3              _reserved->scrollPointDeltaAxis3
#define _hidPointingNub                     _reserved->hidPointingNub
#define _isSeized                           _reserved->isSeized
#define _openClient                         _reserved->openClient
#define _accelerateMode                     _reserved->accelerateMode
#define _scrollButtonMask                   _reserved->scrollButtonMask
#define _scrollPointerCoalesceXCount        _reserved->scrollPointerCoalesceXCount
#define _scrollPointerCoalesceYCount        _reserved->scrollPointerCoalesceYCount
#define _scrollPointerCoalesceLastDy        _reserved->scrollPointerCoalesceLastDy
#define _scrollPointerCoalesceLastDx        _reserved->scrollPointerCoalesceLastDx
#define _scrollPointerPixelFractionAxis1    _reserved->scrollPointerPixelFractionAxis1
#define _scrollPointerPixelFractionAxis2    _reserved->scrollPointerPixelFractionAxis2
#define _scrollPixelFraction1               _reserved->scrollPixelFraction1
#define _scrollPixelFraction2               _reserved->scrollPixelFraction2
#define _scrollPixelFraction3               _reserved->scrollPixelFraction3
#define _scrollLastDA1                      _reserved->scrollLastDA1
#define _scrollLastDA2                      _reserved->scrollLastDA2
#define _scrollLastDA3                      _reserved->scrollLastDA3
#define _scrollType                         _reserved->scrollType

struct CursorDeviceSegment {
    SInt32	devUnits;
    SInt32	slope;
    SInt32	intercept;
};
typedef struct CursorDeviceSegment CursorDeviceSegment;

//static bool GetOSDataValue (OSData * data, UInt32 * value);
static bool SetupAcceleration (OSData * data, IOFixed desired, IOFixed devScale, IOFixed crsrScale, void ** scaleSegments, IOItemCount * scaleSegCount);
static void ScaleAxes (void * scaleSegments, int * axis1p, IOFixed *axis1Fractp, int * axis2p, IOFixed *axis2Fractp);

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
    
    bzero(_reserved, sizeof(ExpansionData));
    
    // Initialize pointer accel items
    _scaleSegments 		= 0;
    _scaleSegCount 		= 0;
    _fractX			= 0;
    _fractY     		= 0;
    
    _acceleration	= -1;
    _accelerateMode = ( kAccelScroll | kAccelMouse );
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

  if (!getProperty(kIOHIDScrollAccelerationTypeKey))
    setProperty(kIOHIDScrollAccelerationTypeKey, kIOHIDMouseScrollAccelerationKey);

  /*
   * RY: Publish a property containing the button Count.  This will
   * will be used to determine whether or not the button
   * behaviors can be modified.
   */
  if (buttonCount() > 1)
  {
     setProperty(kIOHIDPointerButtonCountKey, buttonCount(), 32);
  }

  OSNumber * number;

    if (number = OSDynamicCast(OSNumber, getProperty(kIOHIDScrollMouseButtonKey)))
	{
		UInt32 value = number->unsigned32BitValue();
        
        if (!value)
            _scrollButtonMask = 0;
        else
            _scrollButtonMask = (1 << (value-1));
    }

  // create a IOHIDPointingDevice to post events to the HID Manager
  _hidPointingNub = IOHIDPointingDevice::newPointingDeviceAndStart(this, buttonCount(), resolution() >> 16); 
        
  /*
   * IOHIPointing serves both as a service and a nub (we lead a double
   * life).  Register ourselves as a nub to kick off matching.
   */

  registerService();

  return true;
}

void IOHIPointing::stop(IOService * provider)
{
    if ( _hidPointingNub ) 
    {
        _hidPointingNub->stop(this);
        _hidPointingNub->detach(this);
        
        _hidPointingNub->release();
        _hidPointingNub = 0;
    }

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
    
    if(_scaleSegments && _scaleSegCount)
        IODelete( _scaleSegments, CursorDeviceSegment, _scaleSegCount );

    if(_scrollScaleSegments && _scrollScaleSegCount)
        IODelete( _scrollScaleSegments, CursorDeviceSegment, _scrollScaleSegCount );

    if(_scrollPixelScaleSegments && _scrollPixelScaleSegCount)
        IODelete( _scrollPixelScaleSegments, CursorDeviceSegment, _scrollPixelScaleSegCount );

    if(_scrollPointerScaleSegments && _scrollPointerScaleSegCount)
        IODelete( _scrollPointerScaleSegments, CursorDeviceSegment, _scrollPointerScaleSegCount );

    if(_scrollPointerPixelScaleSegments && _scrollPointerPixelScaleSegCount)
        IODelete( _scrollPointerPixelScaleSegments, CursorDeviceSegment, _scrollPointerPixelScaleSegCount );
    
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

static void AccelerateScrollAxis(   IOFixed * 		axis1p, 
                                    IOFixed * 		axis2p,
                                    IOFixed * 		axis3p,
                                    IOFixed *       prevScrollDeltaAxis,
                                    IOFixed *		prevScrollDeltaTime,
                                    UInt8 *         prevScrollDeltaIndex,
                                    AbsoluteTime 	*scrollLastEventTime,
                                    void *          scaleSegments,
                                    AbsoluteTime    timeStamp,
                                    bool            directionChange = false)
{
    IOFixed absAxis1            = 0;
    IOFixed absAxis2            = 0;
    IOFixed absAxis3            = 0;
    IOFixed mag                 = 0;
    IOFixed	avgIndex            = 0;
    IOFixed	avgCount            = 0;
    IOFixed avgAxis             = 0;
    IOFixed	timeDeltaMS         = 0;
    IOFixed	avgTimeDeltaMS      = 0;
    IOFixed highTime            = 0;
    IOFixed lowTime             = 0;
    IOFixed highAxis            = 0;
    IOFixed lowAxis             = 0;
    IOFixed	scrollMultiplier    = 0;
    UInt64	currentTimeNS       = 0;
    UInt64	lastEventTimeNS     = 0;

    if (!scaleSegments)
        return;
    
    absolutetime_to_nanoseconds(timeStamp, &currentTimeNS);
    absolutetime_to_nanoseconds(*scrollLastEventTime, &lastEventTimeNS);

    *scrollLastEventTime = timeStamp;

    timeDeltaMS = ((currentTimeNS - lastEventTimeNS) / 1000000) << 16;
    
    // RY: To compensate for non continual motion, we have added a second
    // threshold.  This whill allow a user with a standard scroll wheel
    // to continue with acceleration when lifting the finger within a 
    // predetermined time.  We should also clear out the last time deltas
    // if the direction has changed.
    if ((timeDeltaMS > SCROLL_CLEAR_THRESHOLD_MS) || directionChange)
    {
        bzero(prevScrollDeltaTime, sizeof(prevScrollDeltaTime) * SCROLL_TIME_DELTA_COUNT);
        bzero(prevScrollDeltaAxis, sizeof(prevScrollDeltaAxis) * SCROLL_TIME_DELTA_COUNT);
        *prevScrollDeltaIndex = 0;
    }

    absAxis1 = abs(*axis1p);
    absAxis2 = abs(*axis2p);
    absAxis3 = abs(*axis3p);

    if( absAxis1 > absAxis2)
        mag = (absAxis1 + (absAxis2 / 2));
    else
        mag = (absAxis2 + (absAxis1 / 2));
    
    if (mag > absAxis3)
        mag = (mag + (absAxis3 / 2));
    else
        mag = (absAxis3 + (mag / 2));
        
    if( mag == 0 )
        return;
    
    timeDeltaMS = ((timeDeltaMS > SCROLL_EVENT_THRESHOLD_MS) || directionChange) ? 
                    SCROLL_EVENT_THRESHOLD_MS : timeDeltaMS;
    prevScrollDeltaTime[*prevScrollDeltaIndex] = timeDeltaMS;
    prevScrollDeltaAxis[*prevScrollDeltaIndex] = mag;
    
    // Bump the next index
    *prevScrollDeltaIndex = (*prevScrollDeltaIndex + 1) % SCROLL_TIME_DELTA_COUNT;

    // RY: To eliminate jerkyness associated with the scroll acceleration,
    // we scroll based on the average of the last n events.  This has the
    // effect of make acceleration smoother with accel and decel.
    lowAxis     = MAX_DEVICE_THRESHOLD;
    highAxis    = 0;
    lowTime     = SCROLL_EVENT_THRESHOLD_MS;
    highTime    = 0;
    
    for (avgIndex=0; avgIndex < SCROLL_TIME_DELTA_COUNT; avgIndex++) 
    {
        if (prevScrollDeltaTime[avgIndex] == 0)
            continue;
            
        mag             = abs(prevScrollDeltaAxis[avgIndex]);
        avgAxis         += mag;
        avgTimeDeltaMS  += prevScrollDeltaTime[avgIndex];
        avgCount ++;

    /*
        if (mag > highAxis)
            highAxis = mag;
        if (mag < lowAxis)
            lowAxis = mag;
            
        if (prevScrollDeltaTime[avgIndex] > highTime)
            highTime = prevScrollDeltaTime[avgIndex];
        if (prevScrollDeltaTime[avgIndex] < lowTime)
            lowTime = prevScrollDeltaTime[avgIndex];
    */
    }
    
    // RY: Another step to minimize jumpiness is to
    // remove both the high/low axis and time from
    // the averages
/*
    if ( avgCount > 2 )
    {
        avgAxis         -= (highAxis+lowAxis);
        avgTimeDeltaMS  -= (highTime+lowTime);
        avgCount        -= 2;
    
    }
*/    
    avgAxis         = IOFixedDivide(avgAxis, (avgCount<<16));
    avgTimeDeltaMS  = IOFixedDivide(avgTimeDeltaMS, (avgCount<<16));

          
    // RY: Since we want scroll acceleration to work with the
    // time delta and the accel curves, we have come up with
    // this approach:
    //
    // scrollMultiplier = (SCROLL_MULTIPLIER_RANGE * (avgTimeDeltaMS - (SCROLL_EVENT_THRESHOLD_MS + (1<<16))^2) 
    //                      / ((SCROLL_EVENT_THRESHOLD_MS+(1<<16))^2)
    //
    // scrollMultiplier *= avgDeviceDelta
    //
    // The boost curve follows a parabolic curve which results in
    // a smoother boost.
    //
    // The resulting multipler is applied to the average axis 
    // magnitude and then compared against the accleration curve.
    //
    // The value acquired from the graph will then be multiplied
    // to the current axis delta.    
    scrollMultiplier    = IOFixedMultiply(SCROLL_MULTIPLIER_MULTIPLIER, IOFixedSquared(avgTimeDeltaMS - (SCROLL_EVENT_THRESHOLD_MS + (1<<16))));    
    scrollMultiplier    = IOFixedMultiply(scrollMultiplier, avgAxis);
                           
    CursorDeviceSegment	*segment;

    // scale
    for(
        segment = (CursorDeviceSegment *) scaleSegments;
        scrollMultiplier > segment->devUnits;
        segment++)	{}
    
    scrollMultiplier = IOFixedDivide(
            segment->intercept + IOFixedMultiply( scrollMultiplier, segment->slope ),
            scrollMultiplier );
        
    if (scaleSegments)
    {
        *axis1p = IOFixedMultiply(*axis1p, scrollMultiplier);
        *axis2p = IOFixedMultiply(*axis2p, scrollMultiplier);
        *axis3p = IOFixedMultiply(*axis3p, scrollMultiplier);
    }
}


void IOHIPointing::setPointingMode(UInt32 accelerateMode)
{
    _accelerateMode = accelerateMode;
    
    _convertAbsoluteToRelative = ((accelerateMode & kAbsoluteConvertMouse) != 0);
}

UInt32 IOHIPointing::getPointingMode()
{
    return _accelerateMode;
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
    IOFixed     devScale    = IOFixedDivide( resolution(), FRAME_RATE );
    IOFixed     crsrScale   = IOFixedDivide( SCREEN_RESOLUTION, FRAME_RATE );
    OSData *  table         = copyAccelerationTable();

    if (SetupAcceleration (table, desired, devScale, crsrScale, &_scaleSegments, &_scaleSegCount))
    {
        _acceleration = desired;
        _fractX = _fractY = 0;
        
        if (table) table->release();
    }
}

void IOHIPointing::setupScrollForAcceleration( IOFixed desired )
{
    IOFixed     resolution = scrollResolution();
    
    if ( resolution )
    {
        OSData *    accelTable = copyScrollAccelerationTable();
        
        // Setup line scroll wheel acceleration table
        IOFixed     devScale   = IOFixedDivide( resolution, FRAME_RATE );
        IOFixed     scrScale   = IOFixedDivide( SCROLL_DEFUALT_RESOLUTION, FRAME_RATE );
        
        if (SetupAcceleration (accelTable, desired, devScale, scrScale, &_scrollScaleSegments, &_scrollScaleSegCount))
        {
            _scrollDeltaIndex	= 0;
            
            bzero(_scrollDeltaTime, sizeof(_scrollDeltaTime));
            bzero(_scrollDeltaAxis, sizeof(_scrollDeltaAxis));
            
            clock_get_uptime(&_scrollLastEventTime);
        }

        // Setup pixel scroll wheel acceleration table
        devScale   = IOFixedDivide( resolution, FRAME_RATE );
        scrScale   = IOFixedDivide( SCREEN_RESOLUTION, FRAME_RATE );
        
        if (SetupAcceleration (accelTable, desired, devScale, scrScale, &_scrollPixelScaleSegments, &_scrollPixelScaleSegCount))
        {
            _scrollPixelDeltaIndex	= 0;
            
            bzero(_scrollPixelDeltaTime, sizeof(_scrollPixelDeltaTime));
            bzero(_scrollPixelDeltaAxis, sizeof(_scrollPixelDeltaAxis));
            
            clock_get_uptime(&_scrollPixelLastEventTime);
        }

        // Setup line pointer drag/scroll acceleration table
        devScale   = IOFixedDivide( this->resolution(), FRAME_RATE );
        scrScale   = IOFixedDivide( SCROLL_DEFUALT_RESOLUTION, FRAME_RATE );

        if (accelTable) 
            accelTable->retain();

        if (SetupAcceleration (accelTable, desired, devScale, scrScale, &_scrollPointerScaleSegments, &_scrollPointerScaleSegCount))
        {            
            _scrollPointerDeltaIndex = 0;
            
            bzero(_scrollPointerDeltaTime, sizeof(_scrollPointerDeltaTime));
            bzero(_scrollPointerDeltaAxis, sizeof(_scrollPointerDeltaAxis));
            
            clock_get_uptime(&_scrollPointerLastEventTime);
        }
        
        // Setup pixel pointer drag/scroll acceleration table
        devScale   = IOFixedDivide( this->resolution(), FRAME_RATE );
        scrScale   = IOFixedDivide( SCREEN_RESOLUTION, FRAME_RATE );

        if (SetupAcceleration (accelTable, desired, devScale, scrScale, &_scrollPointerPixelScaleSegments, &_scrollPointerPixelScaleSegCount))
        {            
            _scrollPointerPixelDeltaIndex = 0;
            
            bzero(_scrollPointerPixelDeltaTime, sizeof(_scrollPointerPixelDeltaTime));
            bzero(_scrollPointerPixelDeltaAxis, sizeof(_scrollPointerPixelDeltaAxis));
            
            clock_get_uptime(&_scrollPointerPixelLastEventTime);
        }
        
        if (accelTable)
            accelTable->release();

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
    // scaled pressure value; MAX=(2^16)-1, MIN=0    
    *pressure = ((pressureMin != pressureMax)) ? 
                (((unsigned)(*pressure - pressureMin) * 65535LL) / 
                (unsigned)( pressureMax - pressureMin)) : 0;                
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

    //if( buttonCount() > 1) {
	if( buttonState & 2)	// any others down
            buttons |= EV_RB;
	// Other magic bit reshuffling stuff.  It seems there was space
	// left over at some point for a "middle" mouse button between EV_LB and EV_RB
	if(buttonState & 4)
            buttons |= 2;
	// Add in the rest of the buttons in a linear fasion...
	buttons |= buttonState & ~0x7;
   // }

    /* 	There should not be a threshold applied to pressure for generating a button event.
        As soon as the pen hits the tablet, a mouse down should occur
        
    if ((_pressureThresholdToClick < 255) && ((pressure - pressureMin) > ((pressureMax - pressureMin) * _pressureThresholdToClick / 256))) {
        buttons |= EV_LB;
    }
    */
    if ( pressure > pressureMin )
    {
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

    //if( buttonCount() > 1) {
	if( buttonState & 2)	// any others down
            buttons |= EV_RB;
	// Other magic bit reshuffling stuff.  It seems there was space
	// left over at some point for a "middle" mouse button between EV_LB and EV_RB
	if(buttonState & 4)
            buttons |= 2;
	// Add in the rest of the buttons in a linear fasion...
	buttons |= buttonState & ~0x7;
    //}

    if ( _scrollButtonMask & buttonState )
    {
        if ((dx == 0) && (dy == 0))
        {
            IOLockUnlock( _deviceLock );
            return;
        }

        IOFixed     fixedDeltaAxis1 = 0;
        IOFixed     fixedDeltaAxis2 = 0;
        IOFixed     fixedDeltaAxis3 = 0;
        bool        directionChange = false;
        
        fixedDeltaAxis1 = -(dy << 16);
        fixedDeltaAxis2 = -(dx << 16);

        directionChange = ((((_scrollPointerCoalesceLastDy < 0) && (fixedDeltaAxis1 >= 0)) || 
                            ((_scrollPointerCoalesceLastDy >= 0) && (fixedDeltaAxis1 < 0)))
                            || (((_scrollPointerCoalesceLastDx < 0) && (fixedDeltaAxis2 >= 0)) || 
                            ((_scrollPointerCoalesceLastDx >= 0) && (fixedDeltaAxis2 < 0))));
                                            
        _scrollPointerCoalesceLastDy   = fixedDeltaAxis1;
        _scrollPointerCoalesceLastDx   = fixedDeltaAxis2;

        AccelerateScrollAxis(&fixedDeltaAxis1,
                             &fixedDeltaAxis2,
                             &fixedDeltaAxis3,
                             _scrollPointerPixelDeltaAxis,
                             _scrollPointerPixelDeltaTime,
                             &_scrollPointerPixelDeltaIndex,
                             &_scrollPointerPixelLastEventTime,
                             _scrollPointerPixelScaleSegments,
                             ts,
                             directionChange);
                             
        fixedDeltaAxis1 += _scrollPointerPixelFractionAxis1;
        fixedDeltaAxis2 += _scrollPointerPixelFractionAxis2;
        
        CONVERT_SCROLL_FIXED_TO_FRACTION(fixedDeltaAxis1, _scrollPointerPixelFractionAxis1);
        CONVERT_SCROLL_FIXED_TO_FRACTION(fixedDeltaAxis2, _scrollPointerPixelFractionAxis2);
                                         
        _scrollPointDeltaAxis1 = fixedDeltaAxis1 / 65536;
        _scrollPointDeltaAxis2 = fixedDeltaAxis2 / 65536;
        _scrollPointDeltaAxis3 = 0;
        
        // RY: Begin conversion to from continous scroll to normal wheel event
        _scrollFixedDeltaAxis1 = 0;
        _scrollFixedDeltaAxis2 = 0;
        _scrollFixedDeltaAxis3 = 0;

        if ( directionChange )
        {
            _scrollPointerCoalesceYCount = _scrollPointerCoalesceXCount = 0;
        }

        _scrollPointerCoalesceYCount += abs(dy);
        _scrollPointerCoalesceXCount += abs(dx);

        // RY: Not enought movement to generate a scroll
        if ( (_scrollPointDeltaAxis1 == 0) && (_scrollPointDeltaAxis2 == 0) )
        {
            IOLockUnlock( _deviceLock );
            return;        
        }
        
        // RY: throttle the conversion of dy to deltaAxis1
        if ((abs(dy) > SCROLL_POINTER_COALESCE_CLEAR_THRESHOLD) || 
            (_scrollPointerCoalesceYCount >= SCROLL_POINTER_COALESCE_COUNTER_THRESHOLD))
        {                                        
            _scrollFixedDeltaAxis1          = -(dy << 16);
            _scrollPointerCoalesceYCount    = 0;
        }
        else if ( dy != 0)
        {
            
            _scrollFixedDeltaAxis1 = 0;
            dy = 0;            
        }

        // RY: throttle the conversion of dx to deltaAxis2
        if ((abs(dx) > SCROLL_POINTER_COALESCE_CLEAR_THRESHOLD) || 
            (_scrollPointerCoalesceXCount >= SCROLL_POINTER_COALESCE_COUNTER_THRESHOLD))
        {                                        
            _scrollFixedDeltaAxis2          = -(dx << 16);
            _scrollPointerCoalesceXCount    = 0;
        }
        else if ( dx != 0 )
        {
            _scrollFixedDeltaAxis2 = 0;
            dx = 0;            
        }
        
        // RY: got passed the throttling
        if ( dx || dy )
        {
            fixedDeltaAxis1 = -(dy << 16);
            fixedDeltaAxis2 = -(dx << 16);
            
            AccelerateScrollAxis(&fixedDeltaAxis1,
                                 &fixedDeltaAxis2,
                                 &fixedDeltaAxis3,
                                 _scrollPointerDeltaAxis,
                                 _scrollPointerDeltaTime,
                                 &_scrollPointerDeltaIndex,
                                 &_scrollPointerLastEventTime,
                                 _scrollPointerScaleSegments,
                                 ts,
                                 directionChange);
            
            // RY: convert only if the axis has not been zero'd out
            if ( dy )
            {
                _scrollFixedDeltaAxis1 = fixedDeltaAxis1;
                CONVERT_SCROLL_FIXED_TO_COARSE(_scrollFixedDeltaAxis1, dy);
            }
            
            if ( dx )
            {
                _scrollFixedDeltaAxis2 = fixedDeltaAxis2;
                CONVERT_SCROLL_FIXED_TO_COARSE(_scrollFixedDeltaAxis2, dx);
            }
        }

        IOLockUnlock( _deviceLock );

        _scrollType = 1;
        _scrollWheelEvent( this, dy, dx, 0, ts);
        _scrollType = 0;
        
        return;
    }

    // Perform pointer acceleration computations
    if ( _accelerateMode & kAccelMouse )
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
        _hidPointingNub->stop(this);
        _hidPointingNub->detach(this);
        
        _hidPointingNub->release();
        _hidPointingNub = IOHIDPointingDevice::newPointingDeviceAndStart(this, buttonCount(), resolution() >> 16, true);
    }

    // Post the raw event to IOHIDPointingDevice
    if (_hidPointingNub)
        _hidPointingNub->postMouseEvent(0, 0, 0, deltaAxis1);
        
    if (_isSeized)
    {
        IOLockUnlock( _deviceLock);
        return;
    }
        
    _scrollFixedDeltaAxis1 = deltaAxis1 << 16;
    _scrollFixedDeltaAxis2 = deltaAxis2 << 16;
    _scrollFixedDeltaAxis3 = deltaAxis3 << 16;

    _scrollPointDeltaAxis1 = IOFixedMultiply(_scrollFixedDeltaAxis1, CONVERT_SCROLL_WHEEL_SCALE) / 65536;
    _scrollPointDeltaAxis2 = IOFixedMultiply(_scrollFixedDeltaAxis2, CONVERT_SCROLL_WHEEL_SCALE) / 65536;
    _scrollPointDeltaAxis3 = IOFixedMultiply(_scrollFixedDeltaAxis3, CONVERT_SCROLL_WHEEL_SCALE) / 65536;
    
    // Perform pointer acceleration computations
    if ( _accelerateMode & kAccelScroll )
    {
        bool directionChange = ((((_scrollLastDA1 < 0) && (deltaAxis1 >= 0)) || 
                                ((_scrollLastDA1 >= 0) && (deltaAxis1 < 0)))
                                || (((_scrollLastDA2 < 0) && (deltaAxis2 >= 0)) || 
                                ((_scrollLastDA2 >= 0) && (deltaAxis2 < 0)))
                                || (((_scrollLastDA3 < 0) && (deltaAxis3 >= 0)) || 
                                ((_scrollLastDA3 >= 0) && (deltaAxis3 < 0))));
                                
        _scrollLastDA1 = deltaAxis1;
        _scrollLastDA2 = deltaAxis2;
        _scrollLastDA3 = deltaAxis3;

        // RY: Generate fixed point and course scroll deltas.
        AccelerateScrollAxis(&_scrollFixedDeltaAxis1,
                             &_scrollFixedDeltaAxis2,
                             &_scrollFixedDeltaAxis3,
                             _scrollDeltaAxis,
                             _scrollDeltaTime,
                             &_scrollDeltaIndex,
                             &_scrollLastEventTime,
                             _scrollScaleSegments,
                             ts,
                             directionChange);

        CONVERT_SCROLL_FIXED_TO_COARSE(_scrollFixedDeltaAxis1, deltaAxis1);
        CONVERT_SCROLL_FIXED_TO_COARSE(_scrollFixedDeltaAxis2, deltaAxis2);
        CONVERT_SCROLL_FIXED_TO_COARSE(_scrollFixedDeltaAxis3, deltaAxis3);
                                     
        if ( _scrollPixelScaleSegments )
        {
            _scrollPointDeltaAxis1 = _scrollLastDA1 << 16;
            _scrollPointDeltaAxis2 = _scrollLastDA2 << 16;
            _scrollPointDeltaAxis3 = _scrollLastDA3 << 16;

            // RY: Convert scroll wheel value to pixel movement.
            AccelerateScrollAxis(&_scrollPointDeltaAxis1,
                                 &_scrollPointDeltaAxis2,
                                 &_scrollPointDeltaAxis3,
                                 _scrollPixelDeltaAxis,
                                 _scrollPixelDeltaTime,
                                 &_scrollPixelDeltaIndex,
                                 &_scrollPixelLastEventTime,
                                 _scrollPixelScaleSegments,
                                 ts,
                                 directionChange);
                                 
            _scrollPointDeltaAxis1 += _scrollPixelFraction1;
            _scrollPointDeltaAxis2 += _scrollPixelFraction2;
            _scrollPointDeltaAxis3 += _scrollPixelFraction3;
            
            CONVERT_SCROLL_FIXED_TO_FRACTION(_scrollPointDeltaAxis1, _scrollPixelFraction1);
            CONVERT_SCROLL_FIXED_TO_FRACTION(_scrollPointDeltaAxis2, _scrollPixelFraction2);
            CONVERT_SCROLL_FIXED_TO_FRACTION(_scrollPointDeltaAxis3, _scrollPixelFraction3);
            
            _scrollPointDeltaAxis1 /= 65536;
            _scrollPointDeltaAxis2 /= 65536;
            _scrollPointDeltaAxis3 /= 65536;
        }
    }
    
    IOLockUnlock( _deviceLock);
    
    _scrollWheelEvent(	this,
                        deltaAxis1,
                        deltaAxis2,
                        deltaAxis3,
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
    OSString *  	pointerAccelKey;
    OSString *      scrollAccelKey;
    IOReturn		err = kIOReturnSuccess;
    bool		updated = false;
    UInt32		value;

    if( dict->getObject(kIOHIDResetPointerKey))
	resetPointer();

    pointerAccelKey = OSDynamicCast( OSString, getProperty(kIOHIDPointerAccelerationTypeKey));
    scrollAccelKey = OSDynamicCast( OSString, getProperty(kIOHIDScrollAccelerationTypeKey));

    IOLockLock( _deviceLock);
    
    if( pointerAccelKey && 
        ((number = OSDynamicCast( OSNumber, dict->getObject(pointerAccelKey))) || 
         (data = OSDynamicCast( OSData, dict->getObject(pointerAccelKey))))) 
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
        if( pointerAccelKey) {
            // If this is an OSData object, create an OSNumber to store in the registry
            if (!number)
            {
                number = OSNumber::withNumber(value, 32);
                dict->setObject( pointerAccelKey, number );
                number->release();
            }
            else
                dict->setObject( pointerAccelKey, number );
        }

    }
    
    // Scroll accel setup
    // use same mechanism as pointer accel setup
    
    if( dict->getObject(kIOHIDScrollResetKey))
        resetScroll();
    
    if( scrollAccelKey && 
        ((number = OSDynamicCast( OSNumber, dict->getObject(scrollAccelKey))) || 
         (data = OSDynamicCast( OSData, dict->getObject(scrollAccelKey))))) 
    {
        value = (number) ? number->unsigned32BitValue() : 
                            *((UInt32 *) (data->getBytesNoCopy()));
        setupScrollForAcceleration( value );
        updated = true;
    } 
    else if( (number = OSDynamicCast( OSNumber,
		dict->getObject(kIOHIDScrollAccelerationKey))) ||
             (data = OSDynamicCast( OSData,
		dict->getObject(kIOHIDScrollAccelerationKey)))) {

        value = (number) ? number->unsigned32BitValue() : 
                            *((UInt32 *) (data->getBytesNoCopy()));
                            
        setupScrollForAcceleration( value );
        updated = true;
        if( scrollAccelKey) {
            // If this is an OSData object, create an OSNumber to store in the registry
            if (!number)
            {
                number = OSNumber::withNumber(value, 32);
                dict->setObject( scrollAccelKey, number );
                number->release();
            }
            else
                dict->setObject( scrollAccelKey, number );
        }

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

    if ((number = OSDynamicCast(OSNumber, dict->getObject(kIOHIDScrollMouseButtonKey))) ||
        (data = OSDynamicCast(OSData, dict->getObject(kIOHIDScrollMouseButtonKey))))
	{
		value = (number) ? number->unsigned32BitValue() : 
                                            *((UInt32 *) (data->getBytesNoCopy())) ;
        
        if (!value)
            _scrollButtonMask = 0;
        else
            _scrollButtonMask = (1 << (value-1));
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
    OSNumber * number = OSDynamicCast(OSNumber, getProperty(kIOHIDPointerResolutionKey));
    
    if ( number )
        return number->unsigned32BitValue();
        
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

/*bool GetOSDataValue (OSData * data, UInt32 * value)
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
}*/
 
// RY: This function contains the original portions of 
// setupForAcceleration.  This was separated out to 
// accomidate the acceleration of scroll axes
bool SetupAcceleration (OSData * data, IOFixed desired, IOFixed devScale, IOFixed crsrScale, void ** scaleSegments, IOItemCount * scaleSegCount) {
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

    SInt32	scaledX1, scaledY1;
    SInt32	scaledX2, scaledY2;

    CursorDeviceSegment *	segments;
    CursorDeviceSegment *	segment;
    SInt32			segCount;

    if( !data || !devScale || !crsrScale)
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

    scaledX1 = scaledY1 = 0;

    scale = OSReadBigInt32((volatile void *)highTable, 0);
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

    count = OSReadBigInt16((volatile void *)(highTable++), 0);
    scale = (1 << 16);

    // find curves bracketing the desired value
    do {
        highAccl = OSReadBigInt32((volatile void *)highTable, 0);
        highTable += 2;
        highPoints = OSReadBigInt16((volatile void *)(highTable++), 0);

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

    lowerX = OSReadBigInt32((volatile void *)lowTable, 0);
    lowTable += 2;
    lowerY = OSReadBigInt32((volatile void *)lowTable, 0);
    lowTable += 2;
    upperX = OSReadBigInt32((volatile void *)highTable, 0);
    highTable += 2;
    upperY = OSReadBigInt32((volatile void *)highTable, 0);
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
                lowerX = OSReadBigInt32((volatile void *)lowTable, 0);
                lowTable += 2;
                lowerY = OSReadBigInt32((volatile void *)lowTable, 0);
                lowTable += 2;
            }
        } else  {
            /* lowline */
            x2 = lowerX;
            y2 = lowerY;
            x3 = upperX;
            y3 = upperY;
            if( highPoints && (--highPoints)) {
                upperX = OSReadBigInt32((volatile void *)highTable, 0);
                highTable += 2;
                upperY = OSReadBigInt32((volatile void *)highTable, 0);
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
            segment->devUnits = MAX_DEVICE_THRESHOLD;
            
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
    absDx = abs(dx);
    absDy = abs(dy);

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
                                self->_scrollFixedDeltaAxis1,
                                self->_scrollFixedDeltaAxis2,
                                self->_scrollFixedDeltaAxis3,
                                self->_scrollPointDeltaAxis1,
                                self->_scrollPointDeltaAxis2,
                                self->_scrollPointDeltaAxis3,
                                self->_scrollType,
                                ts,
                                self,
                                0);
}
