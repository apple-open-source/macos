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


#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "PowerMac7_2_CPUPowerSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(PowerMac7_2_CPUPowerSensor, IOPlatformSensor)

IOReturn PowerMac7_2_CPUPowerSensor::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
	// this is a fake (logical) sensor, there is no IOHWSensor instance
	// associated with it so no driver should ever register
	return(kIOReturnError);
}

IOReturn PowerMac7_2_CPUPowerSensor::initPlatformSensor( const OSDictionary * dict )
{
	OSString * type;
	OSArray * inputs;

	IOReturn status = super::initPlatformSensor( dict );

	// look for the I_V_inputs array which gives us the IDs of the current
	// and voltage sensors fomr which we will derive the power reading
	inputs = OSDynamicCast(OSArray, dict->getObject("I_V_inputs"));
	if (!inputs)
	{
		SENSOR_DLOG("PowerMac7_2_CPUPowerSensor::initPlatformSensor no I_V_inputs!!\n");
		return(kIOReturnError);
	}

	sourceSensors[0] = platformPlugin->lookupSensorByID(OSDynamicCast(OSNumber, inputs->getObject(0)));
	sourceSensors[1] = platformPlugin->lookupSensorByID(OSDynamicCast(OSNumber, inputs->getObject(1)));

	if (!sourceSensors[0] || !sourceSensors[1])
	{
		SENSOR_DLOG("PowerMac7_2_CPUPowerSensor::initPlatformSensor bad inputs!!\n");
		return(kIOReturnError);
	}

	// this routine needs to take care of a lot of the stuff that is normally
	// done by registerDriver(), since registerDriver() will never be called
	// on this instance.

	// grab some properties out of the IOHWSensor node
	infoDict->setObject( kIOPPluginVersionKey, dict->getObject( kIOPPluginVersionKey ));
	infoDict->setObject( kIOPPluginLocationKey, dict->getObject( kIOPPluginLocationKey ));
	infoDict->setObject( kIOPPluginZoneKey, dict->getObject( kIOPPluginZoneKey ));

	// if there's no type override, get the type from the sensor driver
	if ((type = getSensorType()) == NULL)
	{
		infoDict->setObject( kIOPPluginTypeKey, dict->getObject( kIOPPluginTypeKey ));
	}

	// flag the successful registration
	infoDict->setObject(kIOPPluginRegisteredKey, kOSBooleanTrue );

	// create an empty current-value property
	SensorValue zeroVal;
	zeroVal.sensValue = 0;
	setCurrentValue( zeroVal );

	return( status );
}

SensorValue PowerMac7_2_CPUPowerSensor::fetchCurrentValue( void )
{
	UInt64 buf64;
	SensorValue val1, val2, result;

	val1 = sourceSensors[0]->getCurrentValue();
	val2 = sourceSensors[1]->getCurrentValue();

	// accumulate into a 64 bit buffer
	buf64 = ((UInt64)val1.sensValue) * ((UInt64)val2.sensValue);

	// shift right by 16 to convert back to 16.16 fixed point
	result.sensValue = (SInt32)((buf64 >> 16) & 0xFFFFFFFF);

	return result;
}

// this sends the polling period to the sensor
bool PowerMac7_2_CPUPowerSensor::sendPollingPeriod( OSNumber * period )
{
	// there's no driver, this is a fake (logical) sensor
	return(true);
}
