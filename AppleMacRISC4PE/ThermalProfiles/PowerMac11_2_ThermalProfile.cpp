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
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: PowerMac11_2_ThermalProfile.cpp,v 1.10 2005/09/11 22:40:11 raddog Exp $
 */


#include "IOPlatformPluginSymbols.h"
#include "PowerMac11_2_ThermalProfile.h"
#include "PowerMac11_2_CPUPowerSensor.h"

#define kThermalProfileIsDisabled	"isDisabled"

#define super IOPlatformPluginThermalProfile
OSDefineMetaClassAndStructors( PowerMac11_2_ThermalProfile, IOPlatformPluginThermalProfile )

bool PowerMac11_2_ThermalProfile::start( IOService * provider )
{
	bool superSetup = super::start( provider );
	
	IOLog("PowerMac11_2_ThermalProfile::start %d\n", superSetup);
	
	// See if we run disabled:
	OSObject* tmpObject;
	OSNumber* isDisabled;
	
	if ( ( tmpObject = getProperty(kThermalProfileIsDisabled) ) != NULL )
	{
		if ( (isDisabled = OSDynamicCast(OSNumber, tmpObject)) != NULL )
		{
			if ( isDisabled->unsigned8BitValue() != 0 )
			{
				IOLog("PowerMac11_2_ThermalProfile::start profile is disabled.\n");
				return false;
			}
		}
		else
			IOLog("PowerMac11_2_ThermalProfile::start is disabled is not a number.\n");
	}

	if ( superSetup == false )
		return superSetup;

	IOLog("PowerMac11_2_ThermalProfile::end %d\n", superSetup);
	
	return superSetup;
}


void PowerMac11_2_ThermalProfile::adjustThermalProfile( void )
{
	OSArray*							sensorInfoDicts;
	OSArray*							sensorsArray;
	OSDictionary*						thermalProfile;
	unsigned int i;
	
	IOLog("PowerMac11_2_ThermalProfile::adjustThermalProfile start\n");
	
	if ( (thermalProfile = OSDynamicCast(OSDictionary, platformPlugin->getProperty(kIOPPluginThermalProfileKey))) == NULL )
	{
		IOLog("PowerMac11_2_ThermalProfile::adjustThermalProfile failure getting the thermal profile\n");
		return;
	}
	
	if ( (sensorsArray = OSDynamicCast(OSArray, thermalProfile->getObject(kIOPPluginThermalSensorsKey))) == NULL )
	{
		IOLog("PowerMac11_2_ThermalProfile::adjustThermalProfile failure while parsing sensor array\n");
		return;
	}
	
	sensorInfoDicts = platformPlugin->getSensorInfoDicts();
	if ( sensorInfoDicts == NULL )
	{
		IOLog("PowerMac11_2_ThermalProfile::adjustThermalProfile failure while getting the gIOPPluginSensorDataKey\n");
		return;
	}
	
	if ( sensorsArray->getCount() == 0 )
	{
		IOLog("PowerMac11_2_ThermalProfile::adjustThermalProfile failure the sensor array is empty !\n");
		return;
	}
	
	for ( i = 0 ; i < sensorsArray->getCount() ; i++ )
	{
		OSDictionary *candidate = OSDynamicCast(OSDictionary, sensorsArray->getObject(i));
		
		if ( candidate )
		{
			OSNumber *sensorID = OSDynamicCast(OSNumber, candidate->getObject( kIOPPluginSensorIDKey ));
			
			if ( sensorID )
			{
				IOPlatformSensor*					powerSensor;
			
				if ( ( powerSensor = platformPlugin->lookupSensorByID( sensorID ) ) != NULL )
				{
					if ( OSDynamicCast(PowerMac11_2_CPUPowerSensor, powerSensor) )
					{
						sensorInfoDicts->setObject( powerSensor->getInfoDict() );
					}
				}
				else
				{
					IOLog("PowerMac11_2_ThermalProfile::adjustThermalProfile Sensor entry %d of %d not a sensor with that id\n", i , sensorsArray->getCount());				
				}
			}
			else
			{
				IOLog("PowerMac11_2_ThermalProfile::adjustThermalProfile Sensor entry %d of %d not a number with sensor id\n", i , sensorsArray->getCount());			
			}
		}
		else
		{
			IOLog("PowerMac11_2_ThermalProfile::adjustThermalProfile Sensor entry %d of %d not a dictionary\n", i , sensorsArray->getCount());
		}
	}
}
