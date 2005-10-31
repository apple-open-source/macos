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



#ifdef PSENSOR_DEBUG
#define PSENSOR_DLOG(fmt, args...)  PSENSOR_DLOG(fmt, ## args)
#else
#define PSENSOR_DLOG(fmt, args...)
#endif


#include "IOPlatformPluginSymbols.h"
#include "IOPlatformPlugin.h"
#include "PowerMac11_2_CPUPowerSensor.h"

#define super IOPlatformSensor
OSDefineMetaClassAndStructors(PowerMac11_2_CPUPowerSensor, IOPlatformSensor)

IOReturn PowerMac11_2_CPUPowerSensor::registerDriver( IOService * driver, const OSDictionary * dict, bool notify )
{
	PSENSOR_DLOG("PowerMac11_2_CPUPowerSensor::registerDriver (%p, %p, %d)!!\n",driver,dict,notify);

	if (notify)
		notifyCtrlLoops();

	return(kIOReturnError);
}

/*! @function isRegistered
	@abstract Tells whether there is an IOService * associated with these controls. */
OSBoolean * PowerMac11_2_CPUPowerSensor::isRegistered( void )
{
	if ( (voltageSensor != NULL ) && ( currentSensor != NULL ) )
	{
		PSENSOR_DLOG("PowerMac11_2_CPUPowerSensor::isRegistered (%d, %d)!!\n",( voltageSensor->isRegistered() == kOSBooleanTrue ),( currentSensor->isRegistered() == kOSBooleanTrue ));
	
		if ( ( voltageSensor->isRegistered() == kOSBooleanTrue ) && ( currentSensor->isRegistered() == kOSBooleanTrue ) )
			return kOSBooleanTrue;
	}
	
	return kOSBooleanFalse;
}

void PowerMac11_2_CPUPowerSensor::free( void )
{
	if ( voltageSensor )
	{
		voltageSensor->release();
		voltageSensor = NULL;
	}

	if ( currentSensor )
	{
		currentSensor->release();
		currentSensor = NULL;
	}

	super::free();
}

IOReturn PowerMac11_2_CPUPowerSensor::initPlatformSensor( const OSDictionary * dict )
{
	OSString * type;
	OSArray * inputs;

	IOReturn status = super::initPlatformSensor( dict );
		
	PSENSOR_DLOG("PowerMac11_2_CPUPowerSensor::initPlatformSensor  starts %08lx\n", (UInt32)status);

	// look for the I_V_inputs array which gives us the IDs of the current
	// and voltage sensors fomr which we will derive the power reading
	inputs = OSDynamicCast(OSArray, dict->getObject("I_V_inputs"));
	if (!inputs)
	{
		PSENSOR_DLOG("PowerMac11_2_CPUPowerSensor::initPlatformSensor no I_V_inputs!!\n");
		return(kIOReturnError);
	}

	// The vontage sensor is the first one, the current is the second one:
	
	voltageSensor = platformPlugin->lookupSensorByID(OSDynamicCast(OSNumber, inputs->getObject(0)));
	currentSensor = platformPlugin->lookupSensorByID(OSDynamicCast(OSNumber, inputs->getObject(1)));

	if (!voltageSensor || !currentSensor )
	{
		PSENSOR_DLOG("PowerMac11_2_CPUPowerSensor::initPlatformSensor bad inputs!!\n");
		return(kIOReturnError);
	}
	
	voltageSensor->retain();
	currentSensor->retain();
	
	// TODO: should check that the zone for the above sensors is the same.

	// Get this from the dictionary:
	infoDict->setObject( kIOPPluginLocationKey, dict->getObject( kIOPPluginLocationKey ));
	infoDict->setObject( kIOPPluginTypeKey, dict->getObject( kIOPPluginTypeKey ));
	infoDict->setObject( kIOPPluginSensorIDKey, dict->getObject( kIOPPluginSensorIDKey ));
	
	// grab some properties out of the voltage sensor
	infoDict->setObject( kIOPPluginVersionKey, dict->getObject( kIOPPluginVersionKey ));
	infoDict->setObject( kIOPPluginZoneKey, dict->getObject( kIOPPluginZoneKey ));

	// flag the successful registration
	infoDict->setObject(kIOPPluginRegisteredKey, kOSBooleanTrue );

	// if there's no type override, get the type from the sensor driver
	if ((type = getSensorType()) == NULL)
	{
		infoDict->setObject( kIOPPluginTypeKey, dict->getObject( kIOPPluginTypeKey ));
	}

	// create an empty current-value property
	SensorValue zeroVal;
	zeroVal.sensValue = 0;
	setCurrentValue( zeroVal );
	
	PSENSOR_DLOG("PowerMac11_2_CPUPowerSensor::initPlatformSensor  returns %08lx\n", (UInt32)status);

	return( status );
}

SensorValue PowerMac11_2_CPUPowerSensor::fetchCurrentValue( void )
{
	SensorValue power;
	SensorValue volts = voltageSensor->fetchCurrentValue();
	SensorValue ampere = currentSensor->fetchCurrentValue();
	
	power.sensValue = ( (UInt64)volts.sensValue * (UInt64)ampere.sensValue) >> 16;

	setCurrentValue( power );

	PSENSOR_DLOG("PowerMac11_2_CPUPowerSensor::forceAndFetchCurrentValue %s (%ld) * %s (%ld) = %s (%ld)\n",
		voltageSensor->getSensorDescKey()->getCStringNoCopy(), volts.sensValue >> 16,
		currentSensor->getSensorDescKey()->getCStringNoCopy(), ampere.sensValue >> 16,
		getSensorDescKey()->getCStringNoCopy(), power.sensValue >> 16 );
		
	return power;
}

SensorValue PowerMac11_2_CPUPowerSensor::forceAndFetchCurrentValue( void )
{
	SensorValue power, volts, ampere;

	volts = voltageSensor->forceAndFetchCurrentValue();
	voltageSensor->setCurrentValue( volts );

	ampere = currentSensor->forceAndFetchCurrentValue();
	currentSensor->setCurrentValue( ampere );
	
	power.sensValue = ( (UInt64)volts.sensValue * (UInt64)ampere.sensValue) >> 16;
	setCurrentValue( power );

	PSENSOR_DLOG("PowerMac11_2_CPUPowerSensor::forceAndFetchCurrentValue %s (%ld) * %s (%ld) = %s (%ld)\n",
		voltageSensor->getSensorDescKey()->getCStringNoCopy(), volts.sensValue >> 16,
		currentSensor->getSensorDescKey()->getCStringNoCopy(), ampere.sensValue >> 16,
		getSensorDescKey()->getCStringNoCopy(), power.sensValue >> 16 );

	return power;
}

// this sends the polling period to the sensor
bool PowerMac11_2_CPUPowerSensor::sendPollingPeriod( OSNumber * period )
{
	// there's no driver, this is a fake (logical) sensor
	return(true);
}
