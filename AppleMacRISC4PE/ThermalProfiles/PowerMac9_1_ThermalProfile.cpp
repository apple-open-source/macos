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
 *  File: $Id: PowerMac9_1_ThermalProfile.cpp,v 1.2 2004/04/23 01:29:36 murph Exp $
 */


#include "IOPlatformPluginSymbols.h"
#include "PowerMac9_1_ThermalProfile.h"

OSDefineMetaClassAndStructors( PowerMac9_1_ThermalProfile, IOPlatformPluginThermalProfile )


UInt8 PowerMac9_1_ThermalProfile::getThermalConfig( void )
	{
	return( 0 );
	}


void PowerMac9_1_ThermalProfile::adjustThermalProfile( void )
	{
	const OSNumber*						sensorID;
	OSArray*							sensorInfoDicts;
	IOPlatformSensor*					powerSensor;

	// The Power Sensor is "virtual", meaning there is really no IOHWSensor instance that
	// they correspond to.  Nothing will ever register with their sensor ID, so we have to
	// manually add them to the registry so that they show up in PlatformConsole.

	sensorInfoDicts = OSDynamicCast( OSArray, platformPlugin->getProperty( gIOPPluginSensorDataKey ) );

	sensorID = OSNumber::withNumber( kCPUPowerSensorID, 32 );

	if ( ( powerSensor = platformPlugin->lookupSensorByID( sensorID ) ) != NULL )
	{
		sensorInfoDicts->setObject( powerSensor->getInfoDict());
	}

	sensorID->release();

	sensorInfoDicts->release();
	}
