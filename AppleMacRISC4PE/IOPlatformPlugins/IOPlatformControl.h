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
//		$Log: IOPlatformControl.h,v $
//		Revision 1.7  2003/07/16 02:02:09  eem
//		3288772, 3321372, 3328661
//		
//		Revision 1.6  2003/06/25 02:16:24  eem
//		Merged 101.0.21 to TOT, fixed PM72 lproj, included new fan settings, bumped
//		version to 101.0.22.
//		
//		Revision 1.5.4.2  2003/06/20 09:07:33  eem
//		Added rising/falling slew limiters, integral clipping, etc.
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
//		Revision 1.4.2.3  2003/06/06 12:16:48  eem
//		Updated strings, turned off debugging.
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
//		Revision 1.3.2.3  2003/05/17 11:08:22  eem
//		All active fan data present, table event-driven.  PCI power sensors are
//		not working yet so PCI fan is just set to 67% PWM and forgotten about.
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
//		Revision 1.2.2.1  2003/05/12 11:21:09  eem
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

#ifndef _IOPLATFORMCONTROL_H
#define _IOPLATFORMCONTROL_H

#include <IOKit/IOService.h>

#ifdef CONTROL_DLOG
#undef CONTROL_DLOG
#endif

// Uncomment for debug info
// #define CONTROL_DEBUG 1

#ifdef CONTROL_DEBUG
#define CONTROL_DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define CONTROL_DLOG(fmt, args...)
#endif

enum {
	kIOPControlTypeUnknown,
	kIOPControlTypeSlew,
	kIOPControlTypeFanRPM,
	kIOPControlTypeFanPWM
};

enum {

	/* some controls are not managed by the plugin, but we still need to
	   know about them so we can register for their failure events.  Setting
	   this flag tells the plugin not to attempt to send commands to this
	   control */
	IOPControlFlagExternallyManaged		= 0x1
};

class IOPlatformCtrlLoop;

/*!
	@class IOPlatformControl
	@abstract An internal abstraction of a device controller entity
	@discussion The platform plugin uses this class (and derived classes) internally to represent a device controller entity.  Basic functionality includes registering for messages from the controller object (used for failure notification) and sending commands to the controller. */

class IOPlatformControl : public OSObject
{

	OSDeclareDefaultStructors(IOPlatformControl)

protected:
	/*! @var infoDict A dictionary containing up-to-date info for publishing in the registry.  At the least this should contain control-id, version, location, type, zone, current-value and target-value keys */
	OSDictionary *	infoDict;

	/*! @var ctrlDriver A pointer to the device driver that implements this controller */
	IOService *		controlDriver;

	/*! @var ctrlLoops An array of IOPlatformCtrlLoops that this control acts as an output for.  */
	OSArray *		ctrlLoops;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual bool initSymbols( void );
	virtual void free( void );

	/* translate between plugin and HW value representations.  Note that current-value and target-value might not represent the same thing.  For example, FCU PWM channels accept/report their target value as a PWM tick value in the range [0-255] but report their current value as an integer RPM.  Here we have a method to convert current-value from HW->plugin, and to convert target from HW->plugin and from plugin->HW. */
	virtual const OSNumber * 		applyCurrentValueTransform( const OSNumber * hwReading );
	virtual const OSNumber *		applyTargetValueTransform( const OSNumber * hwReading );
	virtual const OSNumber *		applyTargetHWTransform( const OSNumber * value );

	// notify the control loops that we've got a registered driver
	virtual void notifyCtrlLoops( void );

public:
	/*!	@function initPlatformControl 
		@abstract Initialize an IOPlatformControl object from its ControlArray dict as included in the IOPlatformThermalProfile. */
	virtual IOReturn				initPlatformControl( const OSDictionary *dict );

	/*! @function isManaged
		@abstract Boolean to tell whether the platform plugin is managing this control.  Some controls are only represented so that they can report errors/failures. */
	virtual bool					isManaged( void );

	/*! @function isRegistered
		@abstract Tells whether there is an IOService * associated with this control. */
	virtual OSBoolean *				isRegistered( void );

	/*!	@function registerDriver
		@abstract Used to associated an IOService with this control. */
	virtual IOReturn				registerDriver( IOService * driver, const OSDictionary * dict, bool notify = true );

	/*! @function joinedCtrlLoop
		@abstract Called by IOPlatformCtrlLoop when a control is added.
		@param aCtrlLoop The IOPlatformCtrlLoop* referencing the control loop. */
	virtual bool					joinedCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop );

	/*! @function leftCtrlLoop
		@abstract Called by IOPlatformCtrlLoop when a control is removed from a loop.
		@param aCtrlLoop The control loop that this control is leaving. */
	virtual bool					leftCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop );

	/*!	@function memberOfCtrlLoops
		@abstract Accessor for ctrlLoops array, so that the plugin can get a list of control loops affected by a change in this control's value/state. */
	virtual OSArray *				memberOfCtrlLoops( void );

	/*!	@function sendMessage
		@abstract Sends a dictionary to the control.  The caller is responsible for setting up the dictionary to represent the desired command.  This call can block, as the control may use the calling thread to perform I/O. */
	virtual IOReturn				sendMessage( OSDictionary * msg );

	// get a reference to the info dictionary
	virtual OSDictionary *			getInfoDict( void );

	// accessors - this just grabs attributes from the info dict
	// version
	virtual OSNumber *				getVersion( void );

	// sensor-id
	virtual OSNumber *				getControlID( void );

	// type
	virtual OSString *				getControlType( void );
	virtual UInt32					getControlTypeID( void );

	// zone
	virtual OSData *				getControlZone( void );

	// location
	virtual OSString *				getControlLocation( void );

	// Desc-Key
	virtual OSString *				getControlDescKey( void );

	// control-flags
	virtual OSNumber *				getControlFlags( void );

	// min-value, max-value -- these may not be used for all controls
	virtual UInt32					getControlMinValueUInt32( void );
	virtual OSNumber *				getControlMinValue( void );
	virtual UInt32					getControlMaxValueUInt32( void );
	virtual OSNumber *				getControlMaxValue( void );
	virtual	void					setControlMinValue( OSNumber * min );
	virtual	void					setControlMaxValue( OSNumber * max );

	// current-value
	virtual const OSNumber *		getCurrentValue( void ); // reads current-value from infoDict
	virtual void					setCurrentValue( const OSNumber * sensorValue ); // sets current-value in infoDict
	virtual const OSNumber *		fetchCurrentValue( void );	// possibly blocking -- reads from controller driver

	// target-value
	virtual const OSNumber *		getTargetValue(void);
	virtual void					setTargetValue( UInt32 value );
	virtual void					setTargetValue( const OSNumber * value );

	// this send the target-value stored in infoDict to the control
	// if forced == true, this is a forced value from setProperties
	virtual bool					sendTargetValue( const OSNumber * value, bool forced = false ); // this can block !!
};

#endif