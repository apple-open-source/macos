/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
 *
 *
 */
//		$Log: IOPlatformCtrlLoop.h,v $
//		Revision 1.9  2003/07/16 02:02:09  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.8  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
//		
//		Revision 1.7  2003/06/25 02:21:54  eem
//		Turned off debugging for submission.
//		
//		Revision 1.6  2003/06/25 02:16:24  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.5.4.1  2003/06/19 10:24:16  eem
//		Pulled common PID code into IOPlatformPIDCtrlLoop and subclassed it with
//		PowerMac7_2_CPUFanCtrlLoop and PowerMac7_2_PIDCtrlLoop.  Added history
//		length to meta-state.  No longer adjust T_err when the setpoint changes.
//		Don't crank the CPU fans for overtemp, just slew slow.
//		
//		Revision 1.5  2003/06/07 01:30:56  eem
//		Merge of EEM-PM72-ActiveFans-2 branch, with a few extra tweaks.  This
//		checkin has working PID control for PowerMac7,2 platforms, as well as
//		a first shot at localized strings.
//		
//		Revision 1.4.2.8  2003/06/06 12:16:49  eem
//		Updated strings, turned off debugging.
//		
//		Revision 1.4.2.7  2003/06/06 08:17:56  eem
//		Holy Motherfucking shit.  PID is really working.
//		
//		Revision 1.4.2.6  2003/06/04 10:21:10  eem
//		Supports forced PID meta states.
//		
//		Revision 1.4.2.5  2003/05/31 08:11:34  eem
//		Initial pass at integrating deadline-based timer callbacks for PID loops.
//		
//		Revision 1.4.2.4  2003/05/29 03:51:34  eem
//		Clean up environment dictionary access.
//		
//		Revision 1.4.2.3  2003/05/23 06:36:57  eem
//		More registration notification stuff.
//		
//		Revision 1.4.2.2  2003/05/23 05:44:40  eem
//		Cleanup, ctrlloops not get notification for sensor and control registration.
//		
//		Revision 1.4.2.1  2003/05/22 01:31:04  eem
//		Checkin of today's work (fails compilations right now).
//		
//		Revision 1.4  2003/05/21 21:58:49  eem
//		Merge from EEM-PM72-ActiveFans-1 branch with initial crack at active fan
//		control on Q37.
//		
//		Revision 1.3.2.2  2003/05/16 07:08:45  eem
//		Table-lookup active fan control working with this checkin.
//		
//		Revision 1.3.2.1  2003/05/14 22:07:48  eem
//		Implemented state-driven sensor, cleaned up "const" usage and header
//		inclusions.
//		
//		Revision 1.3  2003/05/13 02:13:51  eem
//		PowerMac7_2 Dynamic Power Step support.
//		
//		Revision 1.2.2.1  2003/05/12 11:21:10  eem
//		Support for slewing.
//		
//		Revision 1.2  2003/05/10 06:50:33  eem
//		All sensor functionality included for PowerMac7_2_PlatformPlugin.  Version
//		is 1.0.1d12.
//		
//		Revision 1.1.2.2  2003/05/10 06:32:34  eem
//		Sensor changes, should be ready to merge to trunk as 1.0.1d12.
//		
//		Revision 1.1.2.1  2003/05/01 09:28:40  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		

#ifndef _IOPLATFORMCTRLLOOP_H
#define _IOPLATFORMCTRLLOOP_H

// IOService pulls in all the headers we need
#include <IOKit/IOService.h>
#include "IOPlatformSensor.h"
#include "IOPlatformControl.h"

#ifdef CTRLLOOP_DLOG
#undef CTRLLOOP_DLOG
#endif

// Uncomment for debug info
// #define CTRLLOOP_DEBUG 1

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

	virtual void didWake( void );
};


#endif