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
 * Copyright (c) 2002-2005 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#include "IOPlatformPluginSymbols.h"
//#include <ppc/machine_routines.h>
#include "PowerMac7_2_PlatformPlugin.h"

#define super IOPlatformPlugin
OSDefineMetaClassAndStructors(PowerMac7_2_PlatformPlugin, IOPlatformPlugin)

PowerMac7_2_PlatformPlugin * PM72Plugin;
const OSSymbol * gPM72EnvSystemUncalibrated;

static const OSData * gPM72EEPROM[2] = { NULL, NULL };

bool PowerMac7_2_PlatformPlugin::init( OSDictionary * dict )
{
	if (!super::init(dict)) return(false);

	if (!gPM72EnvSystemUncalibrated)
		gPM72EnvSystemUncalibrated = OSSymbol::withCString(kPM72EnvSystemUncalibrated);

	return(true);
}

void PowerMac7_2_PlatformPlugin::free( void )
{
	if (gPM72EnvSystemUncalibrated)
	{
		gPM72EnvSystemUncalibrated->release();
		gPM72EnvSystemUncalibrated = NULL;
	}

	super::free();
}

bool PowerMac7_2_PlatformPlugin::start( IOService * provider )
{
	const OSData * cooling;
	const OSNumber * sensorID;
	IOPlatformSensor * powerSensor;

	DLOG("PowerMac7_2_PlatformPlugin::start - entered\n");

	// store the self pointer so helper classes can call readProcROM()
	PM72Plugin = this;

	if (!super::start(provider)) return(false);

	// Set flags to tell the system we do dynamic power step
	if (pmRootDomain != 0)
	{
		pmRootDomain->publishFeature("Reduce Processor Speed");
		pmRootDomain->publishFeature("Dynamic Power Step");
	}

	// set the shroud removed environment flag to false
	//setEnv(gPM72EnvShroudRemoved, kOSBooleanFalse);

	// set the platform ID
	if (gIOPPluginPlatformID)
		gIOPPluginPlatformID->release();

	gIOPPluginPlatformID = OSSymbol::withCString("PowerMac7_2");

	// The CPU Power Sensors are "fake", meaning there is really no IOHWSensor instance that
	// they correspond to.  Nothing will ever register with their sensor ID, so we have to
	// manually add them to the registry so that they show up in SensorLogger.
	//
	// They are sensorIDs 0x30 and 0x31
	sensorID = OSNumber::withNumber( 0x30, 32 );
	powerSensor = lookupSensorByID( sensorID );
	if (powerSensor)
		sensorInfoDicts->setObject( powerSensor->getInfoDict() );
	sensorID->release();

	sensorID = OSNumber::withNumber( 0x31, 32 );
	powerSensor = lookupSensorByID( sensorID );
	if (powerSensor)
		sensorInfoDicts->setObject( powerSensor->getInfoDict() );
	sensorID->release();

	// Publish a copy of sensorInfoDicts in the registry
	setSensorInfoDicts (sensorInfoDicts);

	// Check for system thermal calibration flag.
	// There are three cases:
	// 1) the system is calibrated -- no "thermal-max-cooling" property, no deviation from standard behavior
	// 2) the system is uncalibrated -- "thermal-max-cooling" = <"normal">, fans full
	// 3) MLB/MPU mismatch -- "thermal-max-cooling" = <>, fans full, CPU slow
	{
#define RESIDUAL_PATH_LEN	64

		IORegistryEntry *root;
		char residual[RESIDUAL_PATH_LEN];
		int residual_len = RESIDUAL_PATH_LEN;
		const char * coolingStr;
		int coolingLen;
    
		if ((root = IORegistryEntry::fromPath( "/device-tree", gIODTPlane, residual, &residual_len, NULL )) != NULL &&
		    (cooling = OSDynamicCast(OSData, root->getProperty("thermal-max-cooling"))) != NULL)
		{
			coolingStr = (const char *) cooling->getBytesNoCopy();
			coolingLen = cooling->getLength();
			
			if (coolingStr != NULL && coolingLen != 0 && strcmp(coolingStr, "normal") == 0)
			{
				IOLog("PowerMac7,2 Thermal Manager: detected uncalibrated system: fans to full\n");
				DLOG("PowerMac7,2 Thermal Manager: detected uncalibrated system: fans to full\n");
				setEnv( gPM72EnvSystemUncalibrated, gIOPPluginOne );
			}
			else
			{
				IOLog("PowerMac7,2 Thermal Manager: detected MLB/MPU mismatch: fans to full, CPU(s) to reduced\n");
				DLOG("PowerMac7,2 Thermal Manager: detected MLB/MPU mismatch: fans to full, CPU(s) to reduced\n");
				setEnvArray( gIOPPluginEnvExternalOvertemp, this, true );
			}
		}
			
#undef RESIDUAL_PATH_LEN
	}

	return(true);
}

/* void PowerMac7_2_PlatformPlugin::stop( IOService * provider ) {} */

UInt8 PowerMac7_2_PlatformPlugin::probeConfig( void )
{

//	return (UInt8) ml_get_max_cpus();

#define RESIDUAL_PATH_LEN	64

	OSData * regData;
	UInt32 reg;
	const char *chName;
	OSCollectionIterator *children;
	IORegistryEntry *cpus, *fcu, *channel;
	char residual[RESIDUAL_PATH_LEN];
	int num_cpus, residual_len = RESIDUAL_PATH_LEN;

	// Count the children of the /cpus node in the device tree to find out
	// if this is a uni or dual
	if ((cpus = IORegistryEntry::fromPath( "/cpus", gIODTPlane, residual, &residual_len, NULL )) == NULL ||
		(children = OSDynamicCast(OSCollectionIterator, cpus->getChildIterator( gIODTPlane ))) == NULL)
	{

		return(2);
	}

	cpus->release();

	num_cpus = 0;
	while (children->getNextObject() != 0) num_cpus++;
	children->release();
	if (num_cpus > 1)
	{

		if ((fcu = IORegistryEntry::fromPath( "/u3/i2c/fan", gIODTPlane, residual, &residual_len, NULL )) == NULL ||
			(children = OSDynamicCast(OSCollectionIterator, fcu->getChildIterator( gIODTPlane ))) == NULL)
		{
			return(2);
		}

		fcu->release();
	
		while ((channel = OSDynamicCast(IORegistryEntry, children->getNextObject())) != 0)
		{
			if ((regData = OSDynamicCast(OSData, channel->getProperty("reg"))) == NULL)
				continue;

			reg = *(UInt32 *)regData->getBytesNoCopy();
			chName = channel->getName( gIODTPlane );

			//kprintf("PowerMac7_2_PlatformPlugin::probeConfig found fcu channel %s@%02lX\n", chName, reg);

			if (strcmp(chName, "rpm1") == 0 && reg == 0x12)
			{

				children->release();
				return(2);
			}
		}

		children->release();
		
                //check for U3 Heavy
                if (!(uniN = waitForService(serviceMatching("AppleU3")))) return false;
                uniN->callPlatformFunction ("readUniNReg", false, (void *)(UInt32)kUniNVersion, (void *)(UInt32)&uniNVersion, (void *)0, (void *)0);
                if (IS_U3_HEAVY(uniNVersion))
                    return(3);	//dual (for Q77 better only)
                return(1); // dual (Q37 better and best and Q77 good)
	}
	else
	{
		return(0);	// uni
	}

#undef RESIDUAL_PATH_LEN
}


bool PowerMac7_2_PlatformPlugin::readProcROM( UInt32 procID, UInt16 offset, UInt16 size, UInt8 * buf )
{
	int i;
	const UInt8 * eeprom;

	if (procID > 1 || size == 0 || buf == NULL) return(false);

	// if we don't already have a pointer to this CPU's rom data, get one
	if (gPM72EEPROM[procID] == NULL)
	{
#define RESIDUAL_PATH_LEN	64

		IORegistryEntry * deviceNode;
		char residual[RESIDUAL_PATH_LEN];
		int residual_len = RESIDUAL_PATH_LEN;
		const char * devicePrefix = "/u3/i2c/cpuid@";
		char devicePath[64];

		strcpy(devicePath, devicePrefix);
		if (procID == 0)
			strcat(devicePath, "a0");
		else // procID == 1
			strcat(devicePath, "a2");

		DLOG("PowerMac7_2_PlatformPlugin::readProcROM looking for %s\n", devicePath);

		if ((deviceNode = IORegistryEntry::fromPath( devicePath, gIODTPlane, residual, &residual_len, NULL )) == NULL)
		{
			DLOG("PowerMac7_2_PlatformPlugin::readProcROM unable to find cpuid node (proc %u)\n", procID);
			return(false);
		}

		if ((gPM72EEPROM[procID] = OSDynamicCast(OSData, deviceNode->getProperty("cpuid"))) == NULL)
		{
			DLOG("PowerMac7_2_PlatformPlugin::readProcROM unable to fetch ROM image from device tree (proc %u)\n", procID);
			deviceNode->release();
			return(false);
		}

		deviceNode->release();
#undef RESIDUAL_PATH_LEN
	}

	// fetch the desired data
	eeprom = (const UInt8 *) gPM72EEPROM[procID]->getBytesNoCopy();

	for (i=0; i<size; i++)
	{
		buf[i] = eeprom[offset + i];
	}

	return(true);
}

