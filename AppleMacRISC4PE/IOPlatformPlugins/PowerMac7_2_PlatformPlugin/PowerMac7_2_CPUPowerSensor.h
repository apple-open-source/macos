/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
 *
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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _POWERMAC7_2_CPUPOWERSENSOR_H
#define _POWERMAC7_2_CPUPOWERSENSOR_H

#include "IOPlatformSensor.h"

class PowerMac7_2_CPUPowerSensor : public IOPlatformSensor
{

	OSDeclareDefaultStructors(PowerMac7_2_CPUPowerSensor)

protected:

	IOPlatformSensor *sourceSensors[2];

	/*!	@function registerDriver Used to associated an IOService with this sensor. */
	virtual IOReturn	registerDriver(IOService *driver, const OSDictionary * dict, bool notify = true);

	// initialize a sensor from it's SensorArray dict in the IOPlatformThermalProfile
	virtual IOReturn		initPlatformSensor( const OSDictionary * dict );

	// this reads the sensor's current-value property
	virtual SensorValue		fetchCurrentValue( void );

	// this sends the polling period to the sensor
	virtual bool			sendPollingPeriod( OSNumber * period );

};

#endif // _POWERMAC7_2_CPUPOWERSENSOR_H
