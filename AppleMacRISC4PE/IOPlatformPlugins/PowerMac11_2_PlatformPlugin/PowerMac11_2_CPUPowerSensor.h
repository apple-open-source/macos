/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _POWERMAC11_2_CPUPOWERSENSOR_H
#define _POWERMAC11_2_CPUPOWERSENSOR_H

#include "IOPlatformSensor.h"

class PowerMac11_2_CPUPowerSensor : public IOPlatformSensor
{

	OSDeclareDefaultStructors(PowerMac11_2_CPUPowerSensor)

protected:

	IOPlatformSensor *currentSensor;
	IOPlatformSensor *voltageSensor;

public:
	virtual void free( void );

	/*!	@function registerDriver
		@abstract Used to associated an IOService with this sensor, responsible for extracting needed data from the sensor driver instance and sending the polling period to the sensor driver
		@param driver An IOService reference to the sender of the registration request
		@param dict The registration dictionary send by the driver
		@param notify A boolean that can be used by IOPlatformSensor subclasses that can be used to properly synchronize notification to IOPlatformCtrlLoops that this sensor has registered.  See IOPlatformStateSensor for example usage.
	*/
	virtual IOReturn	registerDriver(IOService *driver, const OSDictionary * dict, bool notify = true);

	/*! @function initPlatformSensor
		@abstract Initialize an IOPlatformSensor object from a thermal profile entry
		@param dict An element from the SensorArray array of the thermal profile
	*/
	virtual IOReturn	initPlatformSensor( const OSDictionary * dict );

	/*! @function fetchCurrentValue
		@abstract Read the "current-value" currently stored in the IOHWSensor instance that corresponds to this sensor and return the transformed (if applicable) fresh value.  This does NOT force IOHWSensor to poll the hardware before utilizing the current-value it's published.
	*/
	virtual SensorValue	fetchCurrentValue( void );	// blocks for I/O
	
	/*! @function forceAndFetchCurrentValue
		@abstract Same as fetchCurrentValue(), except calls sendForceUpdate() before fetching, transforming and storing the current-value.
	*/
	virtual SensorValue				forceAndFetchCurrentValue( void );	// blocks for I/O
	
	/*! @function isRegistered
		@abstract Tells whether there is an IOService * associated with this sensor.
	*/
	virtual OSBoolean *	isRegistered(void);

	// this sends the polling period to the sensor
	virtual bool	sendPollingPeriod( OSNumber * period );
};

#endif // _POWERMAC11_2_CPUPOWERSENSOR_H
