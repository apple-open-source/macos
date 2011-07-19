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
 * 17 July 	1998 	sdouglas	Initial creation
 * 01 April 	2002 	ryepez		added support for scroll acceleration
 */

#if 0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING  "IOHIPointing"
#define DEBUG_ASSERT_PRODUCTION_CODE        0
#endif

#include <AssertMacros.h>
#include <IOKit/IOLib.h>
#include <IOKit/assert.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <libkern/OSByteOrder.h>
#include "IOHIDSystem.h"
#include "IOHIDPointingDevice.h"
#include "IOHIDevicePrivateKeys.h"
#include "IOHIDParameter.h"
#include "IOFixed64.h"
#include "ev_private.h"

#ifndef abs
#define abs(_a)	((_a >= 0) ? _a : -_a)
#endif

#ifndef IOFixedSquared
#define IOFixedSquared(a) IOFixedMultiply(a, a)
#endif

#define FRAME_RATE                  (67 << 16)
#define SCREEN_RESOLUTION           (96 << 16)

#define MAX_DEVICE_THRESHOLD        0x7fffffff
    
#define SCROLL_DEFAULT_RESOLUTION       0x00090000
#define SCROLL_CONSUME_RESOLUTION       0x00640000
#define SCROLL_CONSUME_COUNT_MULTIPLIER 3
#define SCROLL_EVENT_THRESHOLD_MS       0x960000
#define SCROLL_EVENT_THRESHOLD_MS_LL    150ULL
#define SCROLL_CLEAR_THRESHOLD_MS_LL    500ULL

#define SCROLL_MULTIPLIER_RANGE         0x00018000
#define SCROLL_MULTIPLIER_A             0x00000002 /*IOFixedDivide(SCROLL_MULTIPLIER_RANGE,SCROLL_EVENT_THRESHOLD_MS*2)*/
#define SCROLL_MULTIPLIER_B             0x000003bb /*IOFixedDivide(SCROLL_MULTIPLIER_RANGE*3,(SCROLL_EVENT_THRESHOLD_MS^2)*2)*/
#define SCROLL_MULTIPLIER_C             0x00018041


#define SCROLL_WHEEL_TO_PIXEL_SCALE     0x000a0000/* IOFixedDivide(SCREEN_RESOLUTION, SCROLL_DEFAULT_RESOLUTION) */
#define SCROLL_PIXEL_TO_WHEEL_SCALE     0x0000199a/* IOFixedDivide(SCREEN_RESOLUTION, SCROLL_DEFAULT_RESOLUTION) */

#define CONVERT_SCROLL_FIXED_TO_FRACTION(fixed, fraction)   \
{                                                           \
        if( fixed >= 0)                                     \
            fraction = fixed & 0xffff;                      \
        else                                                \
            fraction = fixed | 0xffff0000;                  \
}

#define CONVERT_SCROLL_FIXED_TO_INTEGER(fixedAxis, integer) \
{                                                           \
        SInt32 tempInt = 0;                                 \
        if((fixedAxis < 0) && (fixedAxis & 0xffff))         \
            tempInt = (fixedAxis >> 16) + 1;                \
        else                                                \
            tempInt = (fixedAxis >> 16);                    \
        integer = tempInt;                                  \
}

#define CONVERT_SCROLL_FIXED_TO_COARSE(fixedAxis, coarse)   \
{                                                           \
        SInt32 tempCoarse = 0;                              \
        CONVERT_SCROLL_FIXED_TO_INTEGER(fixedAxis, tempCoarse)  \
        if (!tempCoarse && (fixedAxis & 0xffff))            \
            tempCoarse = (fixedAxis < 0) ? -1 : 1;          \
        coarse = tempCoarse;                                \
}


#define _scrollButtonMask                   _reserved->scrollButtonMask
#define _scrollType                         _reserved->scrollType
#define _scrollZoomMask                     _reserved->scrollZoomMask
#define _scrollOff                          _reserved->scrollOff
#define _lastScrollWasZoom                  _reserved->lastScrollWasZoom

#define _scrollWheelInfo                    _reserved->scrollWheelInfo
#define _scrollPointerInfo                  _reserved->scrollPointerInfo
#define _paraAccelParams                    _reserved->paraAccelParams
#define _paraAccelSecondaryParams           _reserved->paraAccelSecondaryParams

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


#define DEVICE_LOCK     IOLockLock( _deviceLock )
#define DEVICE_UNLOCK   IOLockUnlock( _deviceLock )

enum {
    kAccelTypeGlobal = -1,
    kAccelTypeY = 0, //delta axis 1
    kAccelTypeX = 1, //delta axis 2
    kAccelTypeZ = 2  //delta axis 3
};

struct CursorDeviceSegment {
    SInt32	devUnits;
    SInt32	slope;
    SInt32	intercept;
};
typedef struct CursorDeviceSegment CursorDeviceSegment;

#define SCROLL_TIME_DELTA_COUNT		8
struct ScaleDataState
{
    UInt8           deltaIndex;
    IOFixed         deltaTime[SCROLL_TIME_DELTA_COUNT];
    IOFixed         deltaAxis[SCROLL_TIME_DELTA_COUNT];
    IOFixed         fraction;
};
typedef ScaleDataState ScaleDataState;

struct ScaleConsumeState 
{
    UInt32      consumeCount;
    IOFixed     consumeAccum;
};
typedef ScaleConsumeState ScaleConsumeState;

struct ScrollAxisAccelInfo
{
    AbsoluteTime        lastEventTime;
    void *              scaleSegments;
    IOItemCount         scaleSegCount;
    ScaleDataState      state;
    ScaleConsumeState   consumeState;
    SInt32              lastValue;
    UInt32              consumeClearThreshold;
    UInt32              consumeCountThreshold;
    bool                isHighResScroll;
};
typedef ScrollAxisAccelInfo ScrollAxisAccelInfo;

struct ScrollAccelInfo 
{
    ScrollAxisAccelInfo axis[3];
    
    IOFixed             rateMultiplier;
    UInt32              zoom:1;
};
typedef ScrollAccelInfo ScrollAccelInfo;
        
struct IOHIPointing__PAParameters
{
    IOFixed64   deviceMickysDivider;
    IOFixed64   cursorSpeedMultiplier;
    IOFixed64   accelIndex;
    IOFixed64   gain[4];
    IOFixed64   tangent[2];
};

struct IOHIPointing__PASecondaryParameters
{
    int         firstTangent;
    IOFixed64   m0; // m1 == m0
    IOFixed64   b0; // no b1
    IOFixed64   y0;
    IOFixed64   y1;
    IOFixed64   m_root;
    IOFixed64   b_root;
};

static bool SetupAcceleration (OSData * data, IOFixed desired, IOFixed devScale, IOFixed crsrScale, void ** scaleSegments, IOItemCount * scaleSegCount);
static void ScaleAxes (void * scaleSegments, int * axis1p, IOFixed *axis1Fractp, int * axis2p, IOFixed *axis2Fractp);
static IOFixed64 OSObjectToIOFixed64(OSObject *in);
static bool PACurvesFillParamsFromDict(OSDictionary *parameters, const IOFixed64 devScale, const IOFixed64 crsrScale, IOHIPointing__PAParameters &outParams);
static bool PACurvesSetupAccelParams (OSArray *parametricCurves, IOFixed64 desired, IOFixed64 devScale, IOFixed64 crsrScale, IOHIPointing__PAParameters &primaryParams, IOHIPointing__PASecondaryParameters &secondaryParams);
static IOFixed64 PACurvesGetAccelerationMultiplier(const IOFixed64 device_speed_mickeys, const IOHIPointing__PAParameters &params, const IOHIPointing__PASecondaryParameters &secondaryParams);
static OSDictionary* PACurvesDebugDictionary(IOHIPointing__PAParameters &primaryParams, IOHIPointing__PASecondaryParameters &secondaryParams);


struct IOHIPointing::ExpansionData
{ 
    UInt32      scrollType;
    
    ScrollAccelInfo * scrollWheelInfo;
    ScrollAccelInfo * scrollPointerInfo;
    IOHIPointing__PAParameters *paraAccelParams;
    IOHIPointing__PASecondaryParameters *paraAccelSecondaryParams;
    
    IOFixed		scrollFixedDeltaAxis1;
    IOFixed		scrollFixedDeltaAxis2;
    IOFixed		scrollFixedDeltaAxis3;
    SInt32		scrollPointDeltaAxis1;
    SInt32		scrollPointDeltaAxis2;
    SInt32		scrollPointDeltaAxis3;
    UInt32      scrollButtonMask;
    
    // Added to post events to the HID Manager
    IOHIDPointingDevice	* hidPointingNub;
    IOService   * openClient;
    
    UInt32      accelerateMode;
    UInt32      scrollZoomMask;
    bool		isSeized;
    bool        lastScrollWasZoom;
    bool        scrollOff;
};

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
    
    _scrollWheelInfo = (ScrollAccelInfo *) IOMalloc(sizeof(ScrollAccelInfo));
    if (!_scrollWheelInfo) return false;
    
    bzero(_scrollWheelInfo, sizeof(ScrollAccelInfo));
    
    _scrollPointerInfo = (ScrollAccelInfo *) IOMalloc(sizeof(ScrollAccelInfo));
    if (!_scrollPointerInfo) return false;

    bzero(_scrollPointerInfo, sizeof(ScrollAccelInfo));
    
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
	
	if (!getProperty(kIOHIDDisallowRemappingOfPrimaryClickKey))
		if (provider->getProperty(kIOHIDDisallowRemappingOfPrimaryClickKey))
			setProperty(kIOHIDDisallowRemappingOfPrimaryClickKey, provider->getProperty(kIOHIDDisallowRemappingOfPrimaryClickKey));

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

    if ((number = OSDynamicCast(OSNumber, getProperty(kIOHIDScrollMouseButtonKey))))
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

	registerService(kIOServiceSynchronous);

	return true;
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

    if ( _scrollWheelInfo )
    {
        UInt32 type;
        for (type=kAccelTypeY; type<=kAccelTypeZ; type++) {
            if(_scrollWheelInfo->axis[type].scaleSegments && _scrollWheelInfo->axis[type].scaleSegCount)
                IODelete( _scrollWheelInfo->axis[type].scaleSegments, CursorDeviceSegment, _scrollWheelInfo->axis[type].scaleSegCount );
        }
        
        IOFree(_scrollWheelInfo, sizeof(ScrollAccelInfo));
        _scrollWheelInfo = 0;
    }
    
    if ( _paraAccelParams )
    {
        IOFree(_paraAccelParams, sizeof(IOHIPointing__PAParameters));
        _paraAccelParams = 0;
    }
    
    if ( _paraAccelSecondaryParams )
    {
        IOFree(_paraAccelSecondaryParams, sizeof(IOHIPointing__PASecondaryParameters));
        _paraAccelSecondaryParams = 0;
    }
    
    if ( _scrollPointerInfo )
    {
        UInt32 type;
        for (type=kAccelTypeY; type<=kAccelTypeZ; type++) {
            if(_scrollPointerInfo->axis[type].scaleSegments && _scrollPointerInfo->axis[type].scaleSegCount)
                IODelete( _scrollPointerInfo->axis[type].scaleSegments, CursorDeviceSegment, _scrollPointerInfo->axis[type].scaleSegCount );
        }

        IOFree(_scrollPointerInfo, sizeof(ScrollAccelInfo));
        _scrollPointerInfo = 0;
    }
    
    if ( _hidPointingNub )
    {
        _hidPointingNub->release();
        _hidPointingNub = 0;
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
                      void *				refcon __unused,
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

static void AccelerateScrollAxis(   IOFixed *               axisp, 
                                    ScrollAxisAccelInfo *   scaleInfo,
                                    AbsoluteTime            timeStamp,
                                    IOFixed                 rateMultiplier,
                                    bool                    clear = false)
{
    IOFixed absAxis             = 0;
    IOFixed	avgIndex            = 0;
    IOFixed	avgCount            = 0;
    IOFixed avgAxis             = 0;
    IOFixed	timeDeltaMS         = 0;
    IOFixed	avgTimeDeltaMS      = 0;
    IOFixed	scrollMultiplier    = 0;
    UInt64	currentTimeNSLL     = 0;
    UInt64	lastEventTimeNSLL   = 0;
    UInt64  timeDeltaMSLL       = 0;

    if (!scaleInfo || !scaleInfo->scaleSegments)
        return;
        
    absAxis = abs(*axisp);

    if( absAxis == 0 )
        return;

    absolutetime_to_nanoseconds(timeStamp, &currentTimeNSLL);
    absolutetime_to_nanoseconds(scaleInfo->lastEventTime, &lastEventTimeNSLL);

    scaleInfo->lastEventTime = timeStamp;

    timeDeltaMSLL = (currentTimeNSLL - lastEventTimeNSLL) / 1000000;
    
    // RY: To compensate for non continual motion, we have added a second
    // threshold.  This whill allow a user with a standard scroll wheel
    // to continue with acceleration when lifting the finger within a 
    // predetermined time.  We should also clear out the last time deltas
    // if the direction has changed.
    if ((timeDeltaMSLL > SCROLL_CLEAR_THRESHOLD_MS_LL) || clear)
    {
        bzero(&(scaleInfo->state), sizeof(ScaleDataState));        
    }
    
    timeDeltaMSLL = ((timeDeltaMSLL > SCROLL_EVENT_THRESHOLD_MS_LL) || clear) ? 
                    SCROLL_EVENT_THRESHOLD_MS_LL : timeDeltaMSLL;
                    
    timeDeltaMS = ((UInt32) timeDeltaMSLL) << 16;
    
    scaleInfo->state.deltaTime[scaleInfo->state.deltaIndex] = timeDeltaMS;
    scaleInfo->state.deltaAxis[scaleInfo->state.deltaIndex] = absAxis;
        
    // Bump the next index
    scaleInfo->state.deltaIndex = (scaleInfo->state.deltaIndex + 1) % SCROLL_TIME_DELTA_COUNT;

    // RY: To eliminate jerkyness associated with the scroll acceleration,
    // we scroll based on the average of the last n events.  This has the
    // effect of make acceleration smoother with accel and decel.
    for (avgIndex=0; avgIndex < SCROLL_TIME_DELTA_COUNT; avgIndex++) 
    {
        if (scaleInfo->state.deltaTime[avgIndex] == 0)
            continue;
            
        avgAxis         += abs(scaleInfo->state.deltaAxis[avgIndex]);
        avgTimeDeltaMS  += scaleInfo->state.deltaTime[avgIndex];
        avgCount ++;

    }
    
    avgAxis         = (avgCount) ? IOFixedDivide(avgAxis, (avgCount<<16)) : 0;
    avgTimeDeltaMS  = (avgCount) ? IOFixedDivide(avgTimeDeltaMS, (avgCount<<16)) : 0;
    avgTimeDeltaMS  = IOFixedMultiply(avgTimeDeltaMS, rateMultiplier);
    avgTimeDeltaMS  = (avgTimeDeltaMS > SCROLL_EVENT_THRESHOLD_MS) ? SCROLL_EVENT_THRESHOLD_MS : avgTimeDeltaMS;

          
    // RY: Since we want scroll acceleration to work with the
    // time delta and the accel curves, we have come up with
    // this approach:
    //
    // scrollMultiplier = (SCROLL_MULTIPLIER_A * (avgTimeDeltaMS^2)) + 
    //                      (SCROLL_MULTIPLIER_B * avgTimeDeltaMS) + 
    //                          SCROLL_MULTIPLIER_C
    //
    // scrollMultiplier *= avgDeviceDelta
    //
    // The boost curve follows a quadratic/parabolic curve which
    // results in a smoother boost.
    //
    // The resulting multipler is applied to the average axis 
    // magnitude and then compared against the accleration curve.
    //
    // The value acquired from the graph will then be multiplied
    // to the current axis delta.    
    scrollMultiplier    = IOFixedMultiply(SCROLL_MULTIPLIER_A, IOFixedSquared(avgTimeDeltaMS)) - 
                            IOFixedMultiply(SCROLL_MULTIPLIER_B, avgTimeDeltaMS) + 
                                SCROLL_MULTIPLIER_C;
    scrollMultiplier    = IOFixedMultiply(scrollMultiplier, rateMultiplier);
    scrollMultiplier    = IOFixedMultiply(scrollMultiplier, avgAxis);
                           
    CursorDeviceSegment	*segment;

    // scale
    for(
        segment = (CursorDeviceSegment *) scaleInfo->scaleSegments;
        scrollMultiplier > segment->devUnits;
        segment++)	{}
    
    scrollMultiplier = IOFixedDivide(
            segment->intercept + IOFixedMultiply( scrollMultiplier, segment->slope ),
            absAxis );

    *axisp = IOFixedMultiply(*axisp, scrollMultiplier);
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

void IOHIPointing::setScrollType(UInt32 scrollType)
{
    _scrollType = scrollType;
}

UInt32 IOHIPointing::getScrollType()
{
    return _scrollType;
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
    if (_paraAccelParams && _paraAccelSecondaryParams) {
        IOFixed64 deltaX;
        IOFixed64 deltaY;
        IOFixed64 fractX;
        IOFixed64 fractY;
        IOFixed64 mag;
        deltaX.fromInt(*dxp);
        deltaY.fromInt(*dyp);
        fractX.fromFixed(_fractX);
        fractY.fromFixed(_fractY);
        mag.fromInt(llsqrt((deltaX * deltaX + deltaY * deltaY).as64()));
        
        IOFixed64 mult = PACurvesGetAccelerationMultiplier(mag, *_paraAccelParams, *_paraAccelSecondaryParams);
        deltaX *= mult;
        deltaY *= mult;
        deltaX += fractX;
        deltaY += fractY;
        
        *dxp = deltaX.as32();
        *dyp = deltaY.as32();
        
        _fractX = deltaX.asFixed();
        _fractY = deltaY.asFixed();
        
        // sign extend fractional part
        if( deltaX < 0LL )
            _fractX |= 0xffff0000;
        else
            _fractX &= 0x0000ffff;
        
        if( deltaY < 0LL)
            _fractY |= 0xffff0000;
        else
            _fractY &= 0x0000ffff;
    }
    else {
        ScaleAxes(_scaleSegments, dxp, &_fractX, dyp, &_fractY);
    }
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
    
    slope = (x2 == x1) ? 0 : IOFixedDivide( y2 - y1, x2 - x1 );
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
    OSArray         *parametricAccelerationCurves = OSDynamicCast( OSArray, getProperty(kHIDTrackingAccelParametricCurvesKey, gIOServicePlane) );
    IOFixed         devScale    = IOFixedDivide( resolution(), FRAME_RATE );
    IOFixed         crsrScale   = IOFixedDivide( SCREEN_RESOLUTION, FRAME_RATE );
    bool            useParametric = false;
    
//  IOLog("%s %d: got %08x and %p\n", __PRETTY_FUNCTION__, __LINE__, desired, parametricAccelerationCurves); 
    if (!parametricAccelerationCurves)
        parametricAccelerationCurves = OSDynamicCast( OSArray, getProperty(kHIDAccelParametricCurvesKey, gIOServicePlane) );
    // Try to set up the parametric acceleration data
    if (parametricAccelerationCurves) {
        if ( !_paraAccelParams )
        {
            _paraAccelParams = (IOHIPointing__PAParameters*)IOMalloc(sizeof(IOHIPointing__PAParameters));
        }
        if ( !_paraAccelSecondaryParams )
        {
            _paraAccelSecondaryParams = (IOHIPointing__PASecondaryParameters*)IOMalloc(sizeof(IOHIPointing__PASecondaryParameters));
        }

//      IOLog("%s %d: have %p and %p\n", __PRETTY_FUNCTION__, __LINE__, _paraAccelParams, _paraAccelSecondaryParams); 
        
        if (_paraAccelParams && _paraAccelSecondaryParams) {
            IOFixed64 desired64;
            IOFixed64 devScale64;
            IOFixed64 crsrScale64;
            
        //  IOLog("%s: Calling PACurvesSetupAccelParams with %08x, %08x, %08x\n", __PRETTY_FUNCTION__, desired, devScale, crsrScale);
            
            useParametric = PACurvesSetupAccelParams(parametricAccelerationCurves, 
                                                      desired64.fromFixed(desired), 
                                                      devScale64.fromFixed(devScale), 
                                                      crsrScale64.fromFixed(crsrScale),
                                                      *_paraAccelParams,
                                                      *_paraAccelSecondaryParams);
            if (useParametric && getProperty(kHIDAccelParametricCurvesDebugKey, gIOServicePlane)) {
                OSDictionary *debugInfo = PACurvesDebugDictionary(*_paraAccelParams, *_paraAccelSecondaryParams);
                if (debugInfo) {
                    setProperty(kHIDAccelParametricCurvesDebugKey, debugInfo);
                    debugInfo->release();
                }
            }
        }
    }
    
//  IOLog("%s %d: %s parametric\n", __PRETTY_FUNCTION__, __LINE__, useParametric ? "using" : "NOT using"); 

    // If that fails, fall back to classic acceleration
    if (!useParametric) {
        OSData *  table         = copyAccelerationTable();
        
        if (_paraAccelParams)
            IOFree(_paraAccelParams, sizeof(IOHIPointing__PAParameters));
        if (_paraAccelSecondaryParams)
            IOFree(_paraAccelSecondaryParams, sizeof(IOHIPointing__PASecondaryParameters));
        _paraAccelParams = NULL;
        _paraAccelSecondaryParams = NULL;
        
        if (SetupAcceleration (table, desired, devScale, crsrScale, &_scaleSegments, &_scaleSegCount))
        {
            _acceleration = desired;
            _fractX = _fractY = 0;
        }
        if (table) table->release();
    }
}

void IOHIPointing::setupScrollForAcceleration( IOFixed desired )
{
    IOFixed     devScale    = 0;
    IOFixed     scrScale    = 0;
    IOFixed     res         = 0;
    IOFixed     reportRate  = scrollReportRate();
    OSData *    accelTable  = NULL;  
    UInt32      type        = 0;
      
    _scrollWheelInfo->rateMultiplier    = IOFixedDivide(reportRate, FRAME_RATE);
    _scrollPointerInfo->rateMultiplier  = IOFixedDivide(reportRate, FRAME_RATE);

    for ( type=kAccelTypeY; type<=kAccelTypeZ; type++) {
        res = scrollResolutionForType(type);
        if ( res ) {
            accelTable = copyScrollAccelerationTableForType(type);

            // Setup pixel scroll wheel acceleration table
            devScale = IOFixedDivide( res, reportRate );
            scrScale = IOFixedDivide( SCREEN_RESOLUTION, FRAME_RATE );
            
            if (SetupAcceleration (accelTable, desired, devScale, scrScale, &(_scrollWheelInfo->axis[type].scaleSegments), &(_scrollWheelInfo->axis[type].scaleSegCount)))
            {
                bzero(&(_scrollWheelInfo->axis[type].state), sizeof(ScaleDataState));
                clock_get_uptime(&(_scrollWheelInfo->axis[type].lastEventTime));
            }
            
            _scrollWheelInfo->axis[type].isHighResScroll       = res > (SCROLL_DEFAULT_RESOLUTION * 2);
            _scrollWheelInfo->axis[type].consumeClearThreshold = (IOFixedDivide(res, SCROLL_CONSUME_RESOLUTION) >> 16) * 2;
            _scrollWheelInfo->axis[type].consumeCountThreshold = _scrollWheelInfo->axis[type].consumeClearThreshold * SCROLL_CONSUME_COUNT_MULTIPLIER;
            
            bzero(&(_scrollWheelInfo->axis[type].consumeState), sizeof(ScaleConsumeState));
            
            // Grab the pointer resolution
            res = this->resolution();
            reportRate = FRAME_RATE;
            
            // Setup pixel pointer drag/scroll acceleration table
            devScale = IOFixedDivide( res, reportRate );
            scrScale = IOFixedDivide( SCREEN_RESOLUTION, FRAME_RATE );

            if (SetupAcceleration (accelTable, desired, devScale, scrScale, &(_scrollPointerInfo->axis[type].scaleSegments), &(_scrollPointerInfo->axis[type].scaleSegCount)))
            {            
                bzero(&(_scrollPointerInfo->axis[type].state), sizeof(ScaleDataState));
                clock_get_uptime(&(_scrollPointerInfo->axis[type].lastEventTime));
            }
                    
            _scrollPointerInfo->axis[type].isHighResScroll       = res > (SCROLL_DEFAULT_RESOLUTION * 2);
            _scrollPointerInfo->axis[type].consumeClearThreshold = (IOFixedDivide(res, SCROLL_CONSUME_RESOLUTION) >> 16) * 2;
            _scrollPointerInfo->axis[type].consumeCountThreshold = _scrollPointerInfo->axis[type].consumeClearThreshold * SCROLL_CONSUME_COUNT_MULTIPLIER;

            bzero(&(_scrollPointerInfo->axis[type].consumeState), sizeof(ScaleConsumeState));

            if (accelTable)
                accelTable->release();
        }
    }
}


bool IOHIPointing::resetPointer()
{
    DEVICE_LOCK;

    _buttonMode = NX_RightButton;
    setupForAcceleration(EV_DEFAULTPOINTERACCELLEVEL);
    updateProperties();

    DEVICE_UNLOCK;
    return true;
}

bool IOHIPointing::resetScroll()
{
    DEVICE_LOCK;

    setupScrollForAcceleration(EV_DEFAULTSCROLLACCELLEVEL);

    DEVICE_UNLOCK;
    return true;
}

static void ScalePressure(int *pressure, int pressureMin, int pressureMax)
{    
    // scaled pressure value; MAX=(2^16)-1, MIN=0    
    *pressure = ((pressureMin != pressureMax)) ? 
                (((unsigned)(*pressure - pressureMin) * 65535LL) / 
                (unsigned)( pressureMax - pressureMin)) : 0;                
}


void IOHIPointing::dispatchAbsolutePointerEvent(IOGPoint *  newLoc,
                                                IOGBounds *	bounds,
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
    
    DEVICE_LOCK;

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

    DEVICE_UNLOCK;

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

    DEVICE_LOCK;

    // post the raw event to the IOHIDPointingDevice
    if (_hidPointingNub)
        _hidPointingNub->postMouseEvent(buttonState, dx, dy, 0);
        
    if (_isSeized)
    {
        DEVICE_UNLOCK;
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

        DEVICE_UNLOCK;

        dispatchScrollWheelEventWithAccelInfo(-dy, -dx, 0, _scrollPointerInfo, ts);
        
        return;
    }

    // Perform pointer acceleration computations
    if ( _accelerateMode & kAccelMouse ) {        
        int oldDx = dx;
        int oldDy = dy;

        scalePointer(&dx, &dy);
        
        if (((oldDx < 0) && (dx > 0)) || ((oldDx > 0) && (dx < 0))) {
            IOLog("IOHIPointing::dispatchRelativePointerEvent: Unwanted Direction Change X: oldDx=%d dx=%d\n", oldDy, dy);
        }
        
        
        if (((oldDy < 0) && (dy > 0)) || ((oldDy > 0) && (dy < 0))) {
            IOLog("IOHIPointing::dispatchRelativePointerEvent: Unwanted Direction Change Y: oldDy=%d dy=%d\n", oldDy, dy);
        }
    }

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
    DEVICE_UNLOCK;

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
    dispatchScrollWheelEventWithAccelInfo(deltaAxis1, deltaAxis2, deltaAxis3, _scrollWheelInfo, ts);
}
    
void IOHIPointing::dispatchScrollWheelEventWithAccelInfo(
                                SInt32              deltaAxis1,
                                SInt32              deltaAxis2,
                                SInt32              deltaAxis3,
                                ScrollAccelInfo *   info,
                                AbsoluteTime        ts)
{
    Boolean         isHighResScroll = FALSE;
    IOHIDSystem *   hidSystem       = IOHIDSystem::instance();
    UInt32          eventFlags      = (hidSystem ? hidSystem->eventFlags() : 0);

    DEVICE_LOCK;

    // Change the report descriptor for the IOHIDPointingDevice
    // to include a scroll whell
    if (_hidPointingNub && !_hidPointingNub->isScrollPresent())
    {
        IOHIDPointingDevice * nub = _hidPointingNub;
        
        _hidPointingNub = 0;
        
        DEVICE_UNLOCK;
        
    //  nub->terminate(kIOServiceSynchronous);
        nub->terminate(kIOServiceAsynchronous); // rdar://8810574
        nub->release();
        
        nub = IOHIDPointingDevice::newPointingDeviceAndStart(this, buttonCount(), resolution() >> 16, true);
        
        DEVICE_LOCK;
        
        _hidPointingNub = nub;
    }

    // Post the raw event to IOHIDPointingDevice
    if (_hidPointingNub)
        _hidPointingNub->postMouseEvent(0, 0, 0, deltaAxis1);
    
    if (_isSeized) {
        DEVICE_UNLOCK;
        return;
    }
    
    if (_scrollZoomMask) {
        bool isModifiedToZoom = ((SPECIALKEYS_MODIFIER_MASK & eventFlags) == _scrollZoomMask);
        bool isMomentum = (0 != (_scrollType & kScrollTypeMomentumAny));
        if ((isMomentum && _lastScrollWasZoom) || (isModifiedToZoom && !isMomentum)) {
            _lastScrollWasZoom = true;
            _scrollType |= kScrollTypeZoom;
        }
        else {
            _lastScrollWasZoom = false;
        }
    }
    else {
        _lastScrollWasZoom = false;
    }
    
    if (!(_scrollType & kScrollTypeZoom) && _scrollOff ) {
        DEVICE_UNLOCK;
        return;
    }
     
    _scrollFixedDeltaAxis1 = deltaAxis1 << 16;
    _scrollFixedDeltaAxis2 = deltaAxis2 << 16;
    _scrollFixedDeltaAxis3 = deltaAxis3 << 16;

    CONVERT_SCROLL_FIXED_TO_COARSE(IOFixedMultiply(_scrollFixedDeltaAxis1, SCROLL_WHEEL_TO_PIXEL_SCALE), _scrollPointDeltaAxis1);
    CONVERT_SCROLL_FIXED_TO_COARSE(IOFixedMultiply(_scrollFixedDeltaAxis2, SCROLL_WHEEL_TO_PIXEL_SCALE), _scrollPointDeltaAxis2);
    CONVERT_SCROLL_FIXED_TO_COARSE(IOFixedMultiply(_scrollFixedDeltaAxis3, SCROLL_WHEEL_TO_PIXEL_SCALE), _scrollPointDeltaAxis3);
    
    // Perform pointer acceleration computations
    if ( _accelerateMode & kAccelScroll )
    {
        bool            directionChange[3]          = {0,0,0};
        bool            typeChange                  = FALSE;
        SInt32*         pDeltaAxis[3]               = {&deltaAxis1, &deltaAxis2, &deltaAxis3};
        SInt32*         pScrollFixedDeltaAxis[3]    = {&_scrollFixedDeltaAxis1, &_scrollFixedDeltaAxis2, &_scrollFixedDeltaAxis3};
        IOFixed*        pScrollPointDeltaAxis[3]    = {&_scrollPointDeltaAxis1, &_scrollPointDeltaAxis2, &_scrollPointDeltaAxis3};
        
        if ( info->zoom != (_scrollType == kScrollTypeZoom)) 
        {
            info->zoom = (_scrollType == kScrollTypeZoom);
            typeChange = TRUE;
        }
        
        for (UInt32 type=kAccelTypeY; type<=kAccelTypeZ; type++ ) {
            directionChange[type]       = ((info->axis[type].lastValue == 0) ||
                                           ((info->axis[type].lastValue < 0) && (*(pDeltaAxis[type]) > 0)) || 
                                           ((info->axis[type].lastValue > 0) && (*(pDeltaAxis[type]) < 0)));
            info->axis[type].lastValue  = *(pDeltaAxis[type]);
            
            if ( info->axis[type].scaleSegments )
            {
                isHighResScroll         |= info->axis[type].isHighResScroll;
                *(pScrollPointDeltaAxis[type])  = info->axis[type].lastValue << 16;

                // RY: Convert scroll wheel value to pixel movement.
                AccelerateScrollAxis(pScrollPointDeltaAxis[type],
                                     &(info->axis[type]),
                                     ts,
                                     info->rateMultiplier,
                                     directionChange[type] || typeChange);

                if ( info->axis[type].consumeCountThreshold )
                {
                    *(pScrollPointDeltaAxis[type]) += info->axis[type].state.fraction;
                    CONVERT_SCROLL_FIXED_TO_FRACTION(*(pScrollPointDeltaAxis[type]), info->axis[type].state.fraction);
                    *(pScrollPointDeltaAxis[type]) /= 65536;
                }
                else 
                {
                    CONVERT_SCROLL_FIXED_TO_COARSE(*(pScrollPointDeltaAxis[type]), *(pScrollPointDeltaAxis[type]));
                }

                // RY: Convert pixel value to points
                *(pScrollFixedDeltaAxis[type]) = *(pScrollPointDeltaAxis[type]) << 16;
                
                if ( directionChange[type] )
                    bzero(&(info->axis[type].consumeState), sizeof(ScaleConsumeState));

                // RY: throttle the tranlation of scroll based on the resolution threshold.
                // This allows us to not generated traditional scroll whell (line) events 
                // for high res devices at really low (fine granularity) speeds.  This
                // prevents a succession of single scroll events that can make scrolling
                // slowly actually seem faster.
                if ( info->axis[type].consumeCountThreshold ) 
                {
                    info->axis[type].consumeState.consumeAccum += *(pScrollFixedDeltaAxis[type]) + ((*(pScrollFixedDeltaAxis[type])) ? info->axis[type].state.fraction : 0);
                    info->axis[type].consumeState.consumeCount += abs(info->axis[type].lastValue);

                    if (*(pScrollFixedDeltaAxis[type]) &&
                       ((abs(info->axis[type].lastValue) >= (SInt32)info->axis[type].consumeClearThreshold) ||
                        (info->axis[type].consumeState.consumeCount >= info->axis[type].consumeCountThreshold)))
                    {                                        
                        *(pScrollFixedDeltaAxis[type]) = info->axis[type].consumeState.consumeAccum;
                        info->axis[type].consumeState.consumeAccum = 0;
                        info->axis[type].consumeState.consumeCount = 0;
                    }
                    else
                    {
                        *(pScrollFixedDeltaAxis[type]) = 0;            
                    }
                }
                
                *(pScrollFixedDeltaAxis[type]) = IOFixedMultiply(*(pScrollFixedDeltaAxis[type]), SCROLL_PIXEL_TO_WHEEL_SCALE);

                // RY: Generate fixed point and course scroll deltas.
                CONVERT_SCROLL_FIXED_TO_COARSE(*(pScrollFixedDeltaAxis[type]), *(pDeltaAxis[type]));
            }
        }
    }
    
    DEVICE_UNLOCK;
    
    _scrollType |= (isHighResScroll) ? kScrollTypeContinuous : 0;
    
    _scrollWheelEvent(  this,
                        deltaAxis1,
                        deltaAxis2,
                        deltaAxis3,
                        ts);
    _scrollType = 0;
}

bool IOHIPointing::updateProperties( void )
{
    bool	ok;
    UInt32	res = resolution();

    ok = setProperty( kIOHIDPointerResolutionKey, res, 32)
    &    setProperty( kIOHIDPointerConvertAbsoluteKey, &_convertAbsoluteToRelative,
                        sizeof( _convertAbsoluteToRelative))
    &    setProperty( kIOHIDPointerContactToMoveKey, &_contactToMove,
                        sizeof( _contactToMove));

    return( ok & super::updateProperties() );
}

IOReturn IOHIPointing::setParamProperties( OSDictionary * dict )
{
    OSData			*data;
    OSNumber		*number;
    OSBoolean		*booleanValue;
    OSString		*pointerAccelKey;
    OSString		*scrollAccelKey;
    IOReturn		err = kIOReturnSuccess;
    bool		updated = false;
    UInt32		value;

    // The resetPointer and resetScroll methods attempt to grab the DEVICE_LOCK
    // We should make these calls outside of DEVICE_LOCK as it is not recursive
    if( dict->getObject(kIOHIDResetPointerKey))
        resetPointer();

    if( dict->getObject(kIOHIDScrollResetKey))
        resetScroll();

    pointerAccelKey = OSDynamicCast( OSString, getProperty(kIOHIDPointerAccelerationTypeKey));
    scrollAccelKey = OSDynamicCast( OSString, getProperty(kIOHIDScrollAccelerationTypeKey));

    DEVICE_LOCK;

    if( (number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDScrollZoomModifierMaskKey)))) 
    {
        _scrollZoomMask = number->unsigned32BitValue() & SPECIALKEYS_MODIFIER_MASK;
    }
    
    number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDDeviceScrollWithTrackpadKey));
    if((number) && scrollAccelKey && scrollAccelKey->isEqualTo(kIOHIDTrackpadScrollAccelerationKey)) 
    {   
        _scrollOff = number->unsigned32BitValue() == 0;
    }
    
    number = OSDynamicCast(OSNumber, dict->getObject(kIOHIDDeviceScrollDisableKey));
    if (number) {
        _scrollOff = number->unsigned32BitValue() != 0;
    }
    
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
    if( scrollAccelKey && 
        ((number = OSDynamicCast( OSNumber, dict->getObject(scrollAccelKey))) || 
         (data = OSDynamicCast( OSData, dict->getObject(scrollAccelKey))))) {
        value = (number) ? number->unsigned32BitValue() : 
                            *((UInt32 *) (data->getBytesNoCopy()));
        setupScrollForAcceleration( value );
        updated = true;
    } 
    else if((number = OSDynamicCast( OSNumber, dict->getObject(kIOHIDScrollAccelerationKey))) ||
            (data = OSDynamicCast( OSData, dict->getObject(kIOHIDScrollAccelerationKey)))) {

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

    DEVICE_UNLOCK;

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
                                            *((UInt32 *) (data->getBytesNoCopy()));
		
		if (getProperty(kIOHIDPointerButtonCountKey))
		{
			// vtn3: rdar://problem/5816671
			booleanValue = OSDynamicCast(OSBoolean, getProperty(kIOHIDDisallowRemappingOfPrimaryClickKey));
			if (NULL == booleanValue) {
				booleanValue = kOSBooleanFalse;
			}
			
			switch (value) {
				case kIOHIDButtonMode_BothLeftClicks:
					_buttonMode = NX_OneButton;
					break;
					
				case kIOHIDButtonMode_EnableRightClick:
					_buttonMode = NX_RightButton;
					break;
				
				case kIOHIDButtonMode_ReverseLeftRightClicks:
					// vtn3: rdar://problem/5816671
					_buttonMode = booleanValue->isTrue() ? _buttonMode : NX_LeftButton;
					break;
					
				default:
					// vtn3: rdar://problem/5816671
					_buttonMode = booleanValue->isTrue() ? _buttonMode : value;
			}
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

    return( err == kIOReturnSuccess ) ? super::setParamProperties(dict) : err;
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
IOFixed	IOHIPointing::scrollResolutionForType(SInt32 type)
{
    IOFixed     res         = 0;
    OSNumber * 	number      = NULL;
    const char *key         = NULL;
    
    switch ( type ) {
        case kAccelTypeY:
            key = kIOHIDScrollResolutionYKey;
            break;
        case kAccelTypeX:
            key = kIOHIDScrollResolutionXKey;
            break;
        case kAccelTypeZ:
            key = kIOHIDScrollResolutionZKey;
            break;
        default:
            key = kIOHIDScrollResolutionKey;
            break;
    
    }
    
    number = OSDynamicCast( OSNumber, getProperty(key) );
    if( !number )
		number = OSDynamicCast( OSNumber, getProperty(kIOHIDScrollResolutionKey) );

	if( number )
		res = number->unsigned32BitValue();
        
    return( res );
}

// RY: Added this method to obtain the report rate
// of the scroll wheel.  The default value is 67 << 16.
IOFixed	IOHIPointing::scrollReportRate()
{
    OSNumber * 	number = OSDynamicCast( OSNumber,
                getProperty( kIOHIDScrollReportRateKey ));
                        
    return number ? (number->unsigned32BitValue() ? number->unsigned32BitValue() : FRAME_RATE ) : FRAME_RATE;
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
    return( copyScrollAccelerationTableForType() );
}

OSData * IOHIPointing::copyScrollAccelerationTableForType(SInt32 type)
{
    OSData *    data    = NULL;
    const char *key     = NULL;
    
    switch ( type ) {
        case kAccelTypeY:
            key = kIOHIDScrollAccelerationTableYKey;
            break;
        case kAccelTypeX:
            key = kIOHIDScrollAccelerationTableXKey;
            break;
        case kAccelTypeZ:
            key = kIOHIDScrollAccelerationTableZKey;
            break;
    }
    
    if ( key )
        data = OSDynamicCast( OSData, getProperty( key ));

    if ( !data )
		data = OSDynamicCast( OSData, getProperty( kIOHIDScrollAccelerationTableKey ));

	if ( data )
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

IOFixed64 OSObjectToIOFixed64(OSObject *in)
{
    OSNumber *num = OSDynamicCast(OSNumber, in);
    IOFixed64 result;
    if (num) {
        result.fromFixed(num->unsigned32BitValue());
    }
    return result;
}

bool
PACurvesFillParamsFromDict(OSDictionary *parameters, 
                           const IOFixed64 devScale, 
                           const IOFixed64 crsrScale, 
                           IOHIPointing__PAParameters &outParams)
{
    require(parameters, exit_early);
    
    outParams.deviceMickysDivider = devScale;
    outParams.cursorSpeedMultiplier = crsrScale;

    outParams.accelIndex = OSObjectToIOFixed64(parameters->getObject(kHIDAccelIndexKey));

    outParams.gain[0] = OSObjectToIOFixed64(parameters->getObject(kHIDAccelGainLinearKey));
    outParams.gain[1] = OSObjectToIOFixed64(parameters->getObject(kHIDAccelGainParabolicKey));
    outParams.gain[2] = OSObjectToIOFixed64(parameters->getObject(kHIDAccelGainCubicKey));
    outParams.gain[3] = OSObjectToIOFixed64(parameters->getObject(kHIDAccelGainQuarticKey));
    
    outParams.tangent[0] = OSObjectToIOFixed64(parameters->getObject(kHIDAccelTangentSpeedLinearKey));
    outParams.tangent[1] = OSObjectToIOFixed64(parameters->getObject(kHIDAccelTangentSpeedParabolicRootKey));
//    outParams.tangent[2] = OSObjectToIOFixed64(parameters->getObject(kHIDAccelTangentSpeedCubicRootKey));
//    outParams.tangent[3] = OSObjectToIOFixed64(parameters->getObject(kHIDAccelTangentSpeedQuarticRootKey));
    
    return ((outParams.gain[0] != 0LL) || 
            (outParams.gain[1] != 0LL) || 
            (outParams.gain[2] != 0LL) || 
            (outParams.gain[3] != 0LL));
    
exit_early:
    return false;
}

bool 
PACurvesSetupAccelParams (OSArray *parametricCurves, 
                          IOFixed64 desired, 
                          IOFixed64 devScale, 
                          IOFixed64 crsrScale, 
                          IOHIPointing__PAParameters &primaryParams,
                          IOHIPointing__PASecondaryParameters &secondaryParams)
{
    bool                    success = false;
    OSCollectionIterator    *itr = NULL;
    OSDictionary            *dict = NULL;
    
    IOHIPointing__PAParameters high_curve_params;
    IOHIPointing__PAParameters low_curve_params;

//  IOLog("%s %d: Called with %08x, %08x, %08x\n", __PRETTY_FUNCTION__, __LINE__, desired.asFixed(), devScale.asFixed(), crsrScale.asFixed());

    require(parametricCurves, exit_early);
    require(crsrScale > 0LL, exit_early);
    require(devScale > 0LL, exit_early);
    require(desired > 0LL, exit_early);

    itr = OSCollectionIterator::withCollection(parametricCurves);
    require(itr, exit_early);
    
    while (!success) {
        itr->reset();
        dict = OSDynamicCast(OSDictionary, itr->getNextObject());
        require(PACurvesFillParamsFromDict(dict, devScale, crsrScale, low_curve_params),
                exit_early);
       
        while (!success && (NULL != dict)) {
            if (!PACurvesFillParamsFromDict(dict, devScale, crsrScale, high_curve_params)) {
                break;
            }
            if (desired <= high_curve_params.accelIndex) {
                success = true;
            }
            else {
                low_curve_params = high_curve_params;
            }
            dict = OSDynamicCast(OSDictionary, itr->getNextObject());
        }
        
        require(success || !itr->isValid(), exit_early);
    };
    
    if ( high_curve_params.accelIndex > low_curve_params.accelIndex ) {
        IOFixed64   ratio = (desired - low_curve_params.accelIndex) / (high_curve_params.accelIndex - low_curve_params.accelIndex);
        int         index;
        
//      IOLog("%s %d: Using %08x, %08x, %08x\n", __PRETTY_FUNCTION__, __LINE__, high_curve_params.accelIndex.asFixed(), low_curve_params.accelIndex.asFixed(), ratio.asFixed());
              
        primaryParams.deviceMickysDivider   = high_curve_params.deviceMickysDivider;
        primaryParams.cursorSpeedMultiplier = high_curve_params.cursorSpeedMultiplier;
        primaryParams.accelIndex            = desired;
        
        for (index = 0; index < 4; index++) {
            primaryParams.gain[index] = low_curve_params.gain[index] + (high_curve_params.gain[index] - low_curve_params.gain[index]) * ratio;
            if (primaryParams.gain[index] < 0LL)
                primaryParams.gain[index].fromFixed(0);
        }
        for (index = 0; index < 2; index++) {
            primaryParams.tangent[index] = low_curve_params.tangent[index] + (high_curve_params.tangent[index] - low_curve_params.tangent[index]) * ratio;
            if (primaryParams.tangent[index] < 0LL)
                primaryParams.tangent[index].fromFixed(0);
        }
    }
    else {
        primaryParams = high_curve_params;
    }
    
    success = ((primaryParams.gain[0] != 0LL) || 
               (primaryParams.gain[1] != 0LL) || 
               (primaryParams.gain[2] != 0LL) || 
               (primaryParams.gain[3] != 0LL));
    
    // calculate secondary values
    bzero(&secondaryParams, sizeof(secondaryParams));
    if ((primaryParams.tangent[1] > 0LL) && (primaryParams.tangent[1] < primaryParams.tangent[0]))
        secondaryParams.firstTangent = 1;
    
    if (secondaryParams.firstTangent == 0) {
        secondaryParams.y0 = IOQuarticFunction(primaryParams.tangent[0], primaryParams.gain);
        secondaryParams.m0 = IOQuarticDerivative(primaryParams.tangent[0], primaryParams.gain);
        secondaryParams.b0 = secondaryParams.y0 - secondaryParams.m0 * primaryParams.tangent[0];
        secondaryParams.y1 = secondaryParams.m0 * primaryParams.tangent[1] + secondaryParams.b0;
    }
    else {
		secondaryParams.y1 = IOQuarticFunction( primaryParams.tangent[1], primaryParams.gain ); 
		secondaryParams.m0 = IOQuarticDerivative( primaryParams.tangent[1], primaryParams.gain ); 
    }
    
    secondaryParams.m_root = secondaryParams.m0 * secondaryParams.y1 * 2LL;
    secondaryParams.b_root = exponent(secondaryParams.y1, 2) - secondaryParams.m_root * primaryParams.tangent[1];

exit_early:
    if (itr) {
        itr->release();
    }
    
    return success;
}

OSDictionary*
PACurvesDebugDictionary(IOHIPointing__PAParameters &primaryParams,
                        IOHIPointing__PASecondaryParameters &secondaryParams)
{
    OSDictionary    *result = OSDictionary::withCapacity(20);

    require(result, exit_early);
    
#define ADD_NUMBER_FOR(X) \
    do { \
        OSNumber *value = OSNumber::withNumber(X.as64(), 64); \
        if (value) { \
            result->setObject(#X, value); \
            value->release(); \
        } \
    } \
    while (0)
    
    ADD_NUMBER_FOR(primaryParams.deviceMickysDivider);
    ADD_NUMBER_FOR(primaryParams.cursorSpeedMultiplier);
    ADD_NUMBER_FOR(primaryParams.accelIndex);
    ADD_NUMBER_FOR(primaryParams.gain[0]);
    ADD_NUMBER_FOR(primaryParams.gain[1]);
    ADD_NUMBER_FOR(primaryParams.gain[2]);
    ADD_NUMBER_FOR(primaryParams.gain[3]);
    ADD_NUMBER_FOR(primaryParams.tangent[0]);
    ADD_NUMBER_FOR(primaryParams.tangent[1]);
    
    ADD_NUMBER_FOR(secondaryParams.m0);    
    ADD_NUMBER_FOR(secondaryParams.b0);
    ADD_NUMBER_FOR(secondaryParams.y0);
    ADD_NUMBER_FOR(secondaryParams.y1);
    ADD_NUMBER_FOR(secondaryParams.m_root);
    ADD_NUMBER_FOR(secondaryParams.b_root);

#undef ADD_NUMBER_FOR
    
exit_early:    
    return result;
}

IOFixed64 
PACurvesGetAccelerationMultiplier(const IOFixed64 device_speed_mickeys, 
                                  const IOHIPointing__PAParameters &params, 
                                  const IOHIPointing__PASecondaryParameters &secondaryParams)
{
    IOFixed64 result; // defaults to zero
    
    if ((device_speed_mickeys > result) && (params.deviceMickysDivider != result)) {
		IOFixed64 standardized_speed = device_speed_mickeys / params.deviceMickysDivider;
		IOFixed64 accelerated_speed;
        if ((params.tangent[secondaryParams.firstTangent] != 0LL) && (standardized_speed <= params.tangent[secondaryParams.firstTangent])) {
            accelerated_speed = IOQuarticFunction(standardized_speed, params.gain);
        }
        else {
            if ((secondaryParams.firstTangent == 0) && (params.tangent[1] != 0LL) && (standardized_speed <= params.tangent[1])) {
                accelerated_speed = secondaryParams.m0 * standardized_speed + secondaryParams.b0;
            }
            else {
                accelerated_speed.fromInt(llsqrt(((secondaryParams.m_root * device_speed_mickeys) + secondaryParams.b_root).as64()));
            }
        }
		IOFixed64 accelerated_pixels = accelerated_speed * params.cursorSpeedMultiplier;
		result = accelerated_pixels / device_speed_mickeys;
    }
    else {
        result.fromFixed(1);
    }
    
    return result;
}
	
// RY: This function contains the original portions of 
// scalePointer.  This was separated out to accomidate 
// the acceleration of other axes
void ScaleAxes (void * scaleSegments, int * axis1p, IOFixed *axis1Fractp, int * axis2p, IOFixed *axis2Fractp)
{
    SInt32			dx, dy;
    SInt32			mag;
    IOFixed			scale;
    CursorDeviceSegment	*	segment;

    if( !scaleSegments)
        return;

    dx = (*axis1p) << 16;
    dy = (*axis2p) << 16;

    // mag is (x^2 + y^2)^0.5 and converted to fixed point
    mag = (lsqrt(*axis1p * *axis1p + *axis2p * *axis2p)) << 16;
    if (mag == 0)
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
                 /* at */           IOGPoint * newLoc,
                 /* withBounds */   IOGBounds *bounds,
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
