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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: SMU_Neo2_PowerSensor.cpp,v 1.7 2004/06/18 23:08:18 dirty Exp $
 */


#include "IOPlatformPlugin.h"
#include "IOPlatformPluginSymbols.h"
#include "SMU_Neo2_PlatformPlugin.h"

#include "SMU_Neo2_PowerSensor.h"


OSDefineMetaClassAndStructors( SMU_Neo2_PowerSensor, IOPlatformSensor )


IOReturn SMU_Neo2_PowerSensor::registerDriver( IOService* driver, const OSDictionary* dict, bool notify )
	{
	// This is a fake (virtual) sensor.  There is no IOHWSensor instance associated with it, so no driver
	// should ever register.

	return( kIOReturnError );
	}


IOReturn SMU_Neo2_PowerSensor::initPlatformSensor( const OSDictionary* dict )
{

	sdb_partition_header_t		partHeader;
	UInt8						dvtState;
	OSString*					type;
	OSArray*					inputs;
	IOReturn					result;

	result = IOPlatformSensor::initPlatformSensor( dict );

	// Look for the I_V_inputs array which gives us the sensor-ids pf the voltage and
	// current sensors.  From those two sensors we can derive the power reading.

	if ( ( inputs = OSDynamicCast( OSArray, dict->getObject( "I_V_inputs" ) ) ) == NULL )
		{
		SENSOR_DLOG( "SMU_Neo2_PowerSensor::initPlatformSensor: No I_V_inputs!\n" );

		return( kIOReturnError );
		}

	currentSensor = platformPlugin->lookupSensorByID( OSDynamicCast( OSNumber, inputs->getObject( 0 ) ) );
	voltageSensor = platformPlugin->lookupSensorByID( OSDynamicCast( OSNumber, inputs->getObject( 1 ) ) );

	if ( !voltageSensor || !currentSensor )
		{
		SENSOR_DLOG( "SMU_Neo2_PowerSensor::initPlatformSensor: No voltage or current sensors!\n" );

		return( kIOReturnError );
		}

	// Since registerDriver() is never called, this routine needs to take care
	// of the stuff that is normally done by registerDriver().

	// Copy some properties out of the IOHWSensor node.

	infoDict->setObject( kIOPPluginVersionKey, dict->getObject( kIOPPluginVersionKey ) );
	infoDict->setObject( kIOPPluginLocationKey, dict->getObject( kIOPPluginLocationKey ) );
	infoDict->setObject( kIOPPluginZoneKey, dict->getObject( kIOPPluginZoneKey ) );

	// If there is no type override, copy the type from the sensor driver.

	if ( ( type = getSensorType() ) == NULL )
		{
		infoDict->setObject( kIOPPluginTypeKey, dict->getObject( kIOPPluginTypeKey ) );
		}

	// Prior to DVT Q78 and Q45, the current and voltage sensors were in the wrong place, so we ended up hard-coding
	// the voltage part of the calculation. If we have a debug partition present that says we're on an EVT or earlier,
	// use the dumb algorithm. This can go away soon.

	if ( ( gPlatformPlugin->getSDBPartitionData( kDebugSwitchesPartID, 4, 1, &dvtState ) ) && ( dvtState & 0x80 ) )
	{
		fOnEVTOrEarlier = true;
	}
	else
	{
		fOnEVTOrEarlier = false;
	
		// The thermal profile tells us if we should use the quadratic transform so that the power sensor can be used
		// by both CPU sensors that need the transform, and generic "I*V" sensors that don't. fUseQuadraticTransform
		// may get overridden if the thermal positioning constants partition is not present or too old.
	
		fUseQuadraticTransform = (OSDynamicCast(OSBoolean, dict->getObject("cpu-power-quadratic-transform")) == kOSBooleanTrue);

		SENSOR_DLOG("SMU_Neo2_PowerSensor::initPlatformSensor - fUseQuadraticTransform %d\n", fUseQuadraticTransform);

		if (fUseQuadraticTransform)
		{
			fUseQuadraticTransform = false;							// assume this will fail as it makes the logic below much simpler

			// If we can't read the power conversion constants partition it's not fatal. Also make sure the version is 2 or later.
		
			if (gPlatformPlugin->getSDBPartitionData(kThermalPositioningADCConstantsPartID, 0, sizeof(sdb_partition_header_t), (UInt8 *) &partHeader)
					&& (partHeader.pVER >= 2))
			{
				// The version is new, so read the entire thing.
			
				if (gPlatformPlugin->getSDBPartitionData(kThermalPositioningADCConstantsPartID, 0, sizeof(sdb_thermal_pos_adc_constants_2_part_t), (UInt8 *) &fThermalPosConstantsPart))
				{
					fUseQuadraticTransform = true;
				}
			}
		}
	}

	// Flag our successful registration.

	infoDict->setObject( kIOPPluginRegisteredKey, kOSBooleanTrue );

	// Create an empty "current-value" property.

	SensorValue					zeroValue;

	zeroValue.sensValue = 0;
	setCurrentValue( zeroValue );

	return( result );
	}


SensorValue SMU_Neo2_PowerSensor::fetchCurrentValue( void )
{
	UInt64						power;
	SInt64						powerSquared, a_Component, b_Component;
	SensorValue					voltageValue;
	SensorValue					currentValue;
	SensorValue					result;

	// The voltage and current sensors don't automatically fetch the current value.  We need
	// to force them to update.

	voltageValue = voltageSensor->forceAndFetchCurrentValue();
	voltageSensor->setCurrentValue( voltageValue );

	currentValue = currentSensor->forceAndFetchCurrentValue();
	currentSensor->setCurrentValue( currentValue );

	// Can remove the below conditional when we don't need to support preDVT machines...

	if (fOnEVTOrEarlier)
	{
		// EVT or earlier

		// Just cheat and use ( currentValue * 12 ) - 3 to determine power.

		result.sensValue = ( SInt32 ) ( ( currentValue.sensValue * 12 ) - ( 3 << 16 ) );
	}
	else
	{
		// DVT or later

		// Accumulate into a 64-bit buffer.

		SENSOR_DLOG("SMU_Neo2_PowerSensor::fetchCurrentValue - voltageValue 0x%08X  currentValue 0x%08X\n", (unsigned int) voltageValue.sensValue, (unsigned int) currentValue.sensValue);

		power = ( ( UInt64 ) voltageValue.sensValue ) * ( ( UInt64 ) currentValue.sensValue );

		// Shift right by 16 to convert back to 16.16 fixed point format.

		power = power >> 16;

		// If we didn't get the needed version of the thermal constants partition, or we aren't a CPU power sensor, just return the raw I*V value.
		
		if (fUseQuadraticTransform == false)
		{
			result.sensValue = (SInt32) power;

			SENSOR_DLOG("SMU_Neo2_PowerSensor::fetchCurrentValue - NOT applying quadratic transform\n");
		}
		else
		{
			// The equation below is a*x^2 + b*x + c (a quadratic equation). This equation, in concert with the input values for A, B, and C from the data block
			// describes a curve for the effeciency of the power supply. Since it's non-linear, the I * V power value doesn't correctly represent
			// the actual power consumed by the processor.
			
			SENSOR_DLOG("SMU_Neo2_PowerSensor::fetchCurrentValue - a_Value 0x%08X  b_Value 0x%08X  c_Value 0x%08X\n", (unsigned int) fThermalPosConstantsPart.a_Value, (unsigned int) fThermalPosConstantsPart.b_Value, (unsigned int) fThermalPosConstantsPart.c_Value);

			powerSquared = (power * power) >> 16;
	
			a_Component = (fThermalPosConstantsPart.a_Value * powerSquared) >> 28;
			b_Component = (fThermalPosConstantsPart.b_Value * power) >> 28;
	
			result.sensValue = ( SInt32 ) (a_Component + b_Component + (fThermalPosConstantsPart.c_Value >> 12));
	
			SENSOR_DLOG("SMU_Neo2_PowerSensor::fetchCurrentValue - power = 0x%016LX  powerSquared = 0x%016LX\n", power, powerSquared);
			SENSOR_DLOG("SMU_Neo2_PowerSensor::fetchCurrentValue - a_Component = 0x%016LX  b_Component = 0x%016LX\n", a_Component, b_Component);
		}
	}

	SENSOR_DLOG("SMU_Neo2_PowerSensor::fetchCurrentValue - result.sensValue = 0x%08X (%d)\n", (unsigned int) result.sensValue, (int) result.sensValue >> 16);

	return result;
}


bool SMU_Neo2_PowerSensor::sendPollingPeriod( OSNumber* period )
	{
	// There is no driver.  This is a fake (virtual) sensor.

	return( true );
	}
