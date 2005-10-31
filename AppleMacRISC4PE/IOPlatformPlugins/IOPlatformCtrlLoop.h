/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _IOPLATFORMCTRLLOOP_H
#define _IOPLATFORMCTRLLOOP_H

// IOService pulls in all the headers we need
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include "IOPlatformSensor.h"
#include "IOPlatformControl.h"

#ifdef CTRLLOOP_DLOG
#undef CTRLLOOP_DLOG
#endif

// Uncomment for debug info
//#define CTRLLOOP_DEBUG 1

#ifdef CTRLLOOP_DEBUG
#define CTRLLOOP_DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define CTRLLOOP_DLOG(fmt, args...)
#endif

// Some random defines for default meta state indices, etc.
#define kIOPCtrlLoopNormalMetaState 0
#define kIOPCtrlLoopFailsafeMetaState 1

/*!
    @class IOPlatformCtrlLoop
    @abstract A class abstracting an individual thermal feedback/control loop
	@discussion Describes the sensor inputs, controller outputs, control loop meta state(s), algorithm for aggregating sensor data in loops with multiple inputs, implementation of actions to be taken for given input values (control algorithm), etc.
*/

class IOPlatformCtrlLoop : public OSObject
{

	OSDeclareDefaultStructors(IOPlatformCtrlLoop)

protected:

	bool	safeBoot;

/*! @var sensors An array of IOPlatformSensor references, indicating the sensor or sensors that provide input to this control loop */
	OSArray *sensors;

/*!	@var controls An array of IOPlatformControl references, indicating the controls that this loop drives */
	OSArray *controls;

/*! @var infoDict instance data is stored in this dictionary so that it is exposed to the registry */
	OSDictionary *infoDict;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );

	// Keep track of registration state
enum {
	kIOPCtrlLoopNotReady,
	kIOPCtrlLoopFirstAdjustment,
	kIOPCtrlLoopWillSleep,
	kIOPCtrlLoopDidWake,
	kIOPCtrlLoopAllRegistered
};

	int ctrlloopState;

	/* Each control loop is responsible for maintaining it's deadline.  If the deadline is zero, no callback is required in the future.  If the deadline is non-zero, the platform plugin will schedule a timer callback and will call the ctrl loop's ::adjustControls() routine as close to the deadline time as possible. */
	AbsoluteTime deadline;

public:

/*! @function initPlatformCtrlLoop
	@abstract Initialize a platform control loop with a dictionary from an IOPlatformThermalProfile and a pointer to the platform plugin's envInfo environmental info dictionary.  The control loop object will retain the envInfo dict. */
	virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);

	virtual OSDictionary * getInfoDict( void );

/*!
	@function getDeadline
	@abstract The platform plugin uses this to get a ctrl loops' deadline in order to set the timer. */
	virtual const AbsoluteTime getDeadline( void );

/*!
	@function deadlinePassed
	@abstract This is called when the ctrl loop's deadline has passed.  At the very least it should clear the deadline, or it should reset it to the next desired timer interval. */
	virtual void deadlinePassed( void );

/*!
	@function getMetaState
	@abstract Returns the metastate that the control loop should be operating under, given the current environmental conditions
	@discussion The concept of a control loop metastate is meant to aid in implementing control loops in complex environments, where a certain input may map to different outputs depending on some set of environmental factors.  In the macintosh computing environment, the most obvious environmental factors are the clamshell open/closed state and the energy saver user preferences (CPU speed highest/reduced, or optimize for performance/quiet operation).  Many control loops will have only a single metastate, and as such IOPlatformCtrlLoop's implementation of this method simply returns zero.  For more complex control loops, an IOPlatformCtrlLoop subclass should override this method and properly choose a metastate that corresponds to the current environmental information.
*/
	virtual OSNumber *	getMetaState( void );
	virtual void		setMetaState( const OSNumber * state );

/*!
	@function updateMetaState
	@abstract Causes the control loop to scan the environmental conditions and update its meta state. */
	virtual bool updateMetaState( void );

	virtual OSNumber *				getCtrlLoopID( void );

/*!
	@function adjustControls
	@abstract Tells the ctrl loop to check all its inputs (which can include sensors and environment), apply them to the metastate, and adjust it's control(s) accordingly.  Calling thread may block for I/O. */
	virtual void adjustControls( void );

/*!
	@function getCurrentSensorValue
	@abstract Returns the current sensor input to the control loop.
	@discussion This method returns the current sensor input value to drive the control loop.  If the control loop has multiple sensor inputs, the value returned is an aggregated sensor value.  In this case, IOPlatformCtrlLoop's inplementation returns the max of all current sensor values.  Subclasses may override this method if they need to use a different algorithm to calculate the aggregate sensor value.
*/
//	virtual SensorValue getCurrentSensorValue(void);

/*!
	@function sensorRegistered
	@abstract called by IOPlatformSensor::registerDriver when a sensor driver registers
	@param aSensor an IOPlatformSensor reference for the sensor that registered. */
	virtual void sensorRegistered( IOPlatformSensor * aSensor );

/*!
	@function controlRegistered
	@abstract called by IOPlatformControl::registerDriver when a control driver registers
	@param aControl an IOPlatformControl reference for the control that registered. */
	virtual void controlRegistered( IOPlatformControl * aControl );

/*!
	@function addSensor
	@abstract Adds a sensor to this control loop.  A sensor cannot be added more than once.
	@param aSensor An IOPlatformSensor reference to the sensor to be added. */
	virtual bool addSensor( IOPlatformSensor * aSensor );

/*!
	@function removeSensor
	@abstract Removes a sensor from this control loop.  Fails if the named sensor is not already an input to this control loop.
	@param aSensor An IOPlatformSensor reference to the sensor to be removed. */
	virtual bool removeSensor( IOPlatformSensor * aSensor );

/*!
	@function addControl
	@abstract Adds a control to this control loop.  A control cannot be added more than once.
	@param aControl An IOPlatformControl reference to the control to be added. */
	virtual bool addControl( IOPlatformControl * aControl );

/*!
	@function removeControl
	@abstract Removes a control from this control loop.  Fails if the named sensor is not already an output of this control loop.
	@param aControl An IOPlatformControl reference to the control to be removed. */
	virtual bool removeControl( IOPlatformControl * aControl );

	// control loops can use these functions to get notified when a sensor or control value
	// is changed by another entity. This can be used to avoid duplicate polling/updating.
	// the default implementation is simply to do nothing and return().
	virtual void sensorCurrentValueWasSet( IOPlatformSensor * aSensor, SensorValue newValue );
	virtual void controlCurrentValueWasSet( IOPlatformControl * aControl, ControlValue newValue );
	virtual void controlTargetValueWasSet( IOPlatformControl * aControl, ControlValue newValue );

	virtual void willSleep( void );
	virtual void didWake( void );
};


#endif
