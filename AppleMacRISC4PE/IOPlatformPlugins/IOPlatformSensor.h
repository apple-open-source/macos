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
 *  DRI: Dave Radcliffe
 *
 */
//		$Log: IOPlatformSensor.h,v $
//		Revision 1.7  2003/07/08 04:32:49  eem
//		3288891, 3279902, 3291553, 3154014
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
//		Revision 1.4.2.3  2003/06/06 12:16:49  eem
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
//		Revision 1.3.2.1  2003/05/14 22:07:49  eem
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
//		Revision 1.1.1.1.2.2  2003/05/03 01:11:38  eem
//		*** empty log message ***
//		
//		Revision 1.1.1.1.2.1  2003/05/01 09:28:40  eem
//		Initial check-in in progress toward first Q37 checkpoint.
//		
//		Revision 1.1.1.1  2003/02/04 00:36:43  raddog
//		initial import into CVS
//		

#ifndef _IOPLATFORMSENSOR_H
#define _IOPLATFORMSENSOR_H

// IOService pulls in all the headers we need
#include <IOKit/IOService.h>

#ifdef SENSOR_DLOG
#undef SENSOR_DLOG
#endif

// Uncomment for debug info
// #define SENSOR_DEBUG 1

#ifdef SENSOR_DEBUG
#define SENSOR_DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define SENSOR_DLOG(fmt, args...)
#endif

enum {
	kIOPSensorTypeUnknown,
	kIOPSensorTypeTemp,
	kIOPSensorTypeVoltage,
	kIOPSensorTypeCurrent,
	kIOPSensorTypePower,
	kIOPSensorTypeADC
};

class IOPlatformCtrlLoop;

/*!
    @class IOPlatformSensor
    @abstract A class abstracting hardware sensors such as for power and thermal */
class IOPlatformSensor : public OSObject
{
    OSDeclareDefaultStructors(IOPlatformSensor)	

protected:
	/*! @var infoDict A dictionary containing up-to-date info for publishing in the registry.  At the least this should contain sensor-id, version, location, type, zone and current-value keys */
	OSDictionary *	infoDict;

	/*! @var sensorDriver A pointer to the device driver that implements this controller.  Should be an IOHWSensor object. */
	IOService *		sensorDriver;

	/*! @var ctrlLoops An array of IOPlatformCtrlLoops that this sensor acts as an input for.  */
	OSArray *		ctrlLoops;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual bool initSymbols( void );
	virtual void free( void );

	// sometimes we need to apply a transform to convert a value read from the sensor into something meaningful.
	// Subclasses should implement this method to perform their transforms.
	virtual const OSNumber *	applyValueTransform( const OSNumber * hwReading ) const;

	// notify the control loops that we've got a registered driver
	virtual void notifyCtrlLoops( void );

public:
	// initialize a sensor from it's SensorArray dict in the IOPlatformThermalProfile
	virtual IOReturn	initPlatformSensor( const OSDictionary * dict );

	// initialize an IOPlatformSensor from an unknown (i.e. not listed in thermal profile) sensor
	// that is trying to register.
	virtual IOReturn	initPlatformSensor( IOService * unknownSensor );

	/*! @function isRegistered Tells whether there is an IOService * associated with this sensor. */
	virtual OSBoolean *	isRegistered(void);

	/*!	@function registerDriver Used to associated an IOService with this sensor. */
	virtual IOReturn	registerDriver(IOService *driver, const OSDictionary * dict, bool notify = true);

	/*! @function joinedCtrlLoop
		@abstract Called by IOPlatformCtrlLoop when a sensor is added.
		@param aCtrlLoop The IOPlatformCtrlLoop* referencing the control loop. */
	virtual bool		joinedCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop );

	/*! @function leftCtrlLoop
		@abstract Called by IOPlatformCtrlLoop when a sensor is removed from a loop.
		@param aCtrlLoop The control loop that this sensor is leaving. */
	virtual bool		leftCtrlLoop( IOPlatformCtrlLoop * aCtrlLoop );

	/*!	@function memberOfCtrlLoops
		@abstract Accessor for ctrlLoops array, so that the plugin can get a list of control loops affected by a change in this sensor's value/state. */
	virtual OSArray *				memberOfCtrlLoops( void );

	/*!	@function sendMessage
		@abstract Sends a dictionary to the sensor.  The caller is responsible for setting up the dictionary to represent the desired command.  This call can block, as the sensor may use the calling thread to perform I/O. */
	virtual IOReturn				sendMessage( OSDictionary * msg );

	// get a reference to the info dictionary
	virtual OSDictionary *			getInfoDict( void );

	// accessors - this just grabs attributes from the info dict
	// version
	virtual OSNumber *				getVersion( void );

	// sensor-id
	virtual OSNumber *				getSensorID( void );

	// type
	virtual OSString *				getSensorType( void );
	virtual UInt32					getSensorTypeID( void );

	// zone
	virtual OSData *				getSensorZone( void );

	// location
	virtual OSString *				getSensorLocation( void );

	// Desc-Key
	virtual OSString *				getSensorDescKey( void );

	// sensor-flags
	virtual const OSNumber *		getSensorFlags( void );

	// current-value
	virtual const OSNumber *		getCurrentValue( void );
	virtual void					setCurrentValue( const OSNumber * sensorValue );

	// this sends a force-update message to the sensor and then reads its current-value property
	virtual const OSNumber *		fetchCurrentValue( void );	// blocks for I/O

	// polling period
	virtual const OSNumber *		getPollingPeriod( void );
	virtual void					setPollingPeriod( const OSNumber * period );

	// this sends the polling period to the sensor
	virtual bool					sendPollingPeriod( const OSNumber * period );

/*	
public:

	// First variant takes a preallocated array of thresholds - useful when compiled in
	virtual IOReturn initPlatformSensor (OSDictionary *dict, IOService *sensor, UInt32 maxSensorStates, 
		ThresholdInfo *thresholds);
	// Second variant takes a dictionary - convenient when read from a property
	virtual IOReturn initPlatformSensor (OSDictionary *dict, IOService *sensor, UInt32 maxSensorStates, 
		OSDictionary *thresholdDict);
	// Handle messages from sensor
	virtual IOReturn handleSensorEvent (UInt32 eventType, IOService *fromService, OSDictionary *dict);
*/
};


#endif 	// _IOPLATFORMSENSOR_H