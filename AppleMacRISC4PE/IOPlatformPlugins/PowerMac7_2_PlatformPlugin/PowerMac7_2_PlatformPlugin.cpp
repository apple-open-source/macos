/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#include "IOPlatformPluginSymbols.h"
//#include <ppc/machine_routines.h>
#include "PowerMac7_2_PlatformPlugin.h"

#define super IOPlatformPlugin
OSDefineMetaClassAndStructors(PowerMac7_2_PlatformPlugin, IOPlatformPlugin)

PowerMac7_2_PlatformPlugin * PM72Plugin;
//const OSSymbol * gPM72EnvShroudRemoved;
//const OSSymbol * gPM72EnvAllowNapping;
const OSSymbol * gPM72EnvSystemUncalibrated;

static const OSData * gPM72EEPROM[2] = { NULL, NULL };

bool PowerMac7_2_PlatformPlugin::init( OSDictionary * dict )
{
	if (!super::init(dict)) return(false);

//	if (!gPM72EnvShroudRemoved)
//		gPM72EnvShroudRemoved = OSSymbol::withCString(kPM72EnvShroudRemoved);
//	if (!gPM72EnvAllowNapping)
//		gPM72EnvAllowNapping = OSSymbol::withCString(kPM72EnvAllowNapping);
	if (!gPM72EnvSystemUncalibrated)
		gPM72EnvSystemUncalibrated = OSSymbol::withCString(kPM72EnvSystemUncalibrated);

	return(true);
}

void PowerMac7_2_PlatformPlugin::free( void )
{
//	if (gPM72EnvShroudRemoved)
//	{
//		gPM72EnvShroudRemoved->release();
//		gPM72EnvShroudRemoved = NULL;
//	}

//	if (gPM72EnvAllowNapping)
//	{
//		gPM72EnvAllowNapping->release();
//		gPM72EnvAllowNapping = NULL;
//	}

	if (gPM72EnvSystemUncalibrated)
	{
		gPM72EnvSystemUncalibrated->release();
		gPM72EnvSystemUncalibrated = NULL;
	}

	super::free();
}

bool PowerMac7_2_PlatformPlugin::start( IOService * provider )
{
	//OSArray * tmpArray;
	//mach_timespec_t waitTimeout;
	//IOService *rsc;
	const OSData * cooling;
	const OSNumber * sensorID;
	IOPlatformSensor * powerSensor;

	DLOG("PowerMac7_2_PlatformPlugin::start - entered\n");

	// store the self pointer so helper classes can call readProcROM()
	PM72Plugin = this;

/*
	// get a pointer to the UniN I2C driver
	waitTimeout.tv_sec = 5;
	waitTimeout.tv_nsec = 0;

	rsc = waitForService(resourceMatching("PPCI2CInterface.i2c-uni-n"), &waitTimeout);
	if (!rsc)
	{
		DLOG("PowerMac7_2_PlatformPlugin::start i2c-uni-n resource not found\n");
		return(false);
	}

	fI2C_iface = OSDynamicCast(IOService, rsc->getProperty("PPCI2CInterface.i2c-uni-n"));
	if (!fI2C_iface)
	{
		DLOG("PowerMac7_2_PlatformPlugin::start failed to get I2C driver\n");
		return(false);
	}

	DLOG("PowerMac7_2_PlatformPlugin::start got I2C driver %s\n", fI2C_iface->getName());
*/
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

/*	tmpArray = OSArray::withCapacity(0);
	setEnv(gPM72EnvAllowNapping, tmpArray);
	tmpArray->release();
*/
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
		return(1);	// dual
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

/*
bool PowerMac7_2_PlatformPlugin::readProcROM( UInt32 procID, UInt16 offset, UInt16 size, UInt8 * buf )
{
	UInt8 romBus, romAddr;
	bool success = false;

	DLOG("PowerMac7_2_PlatformPlugin::readProcROM procID 0x%02lX offset 0x%04X size %u\n",
			procID, offset, size);

	if (procID > 1) return(false);

	// the ROM address depends on which processor card we're interested in
	romBus = 0x0;	// uni-n master 0
	romAddr = (!procID) ? 0xA0 : 0xA2;

	if (!openI2C(romBus, romAddr))
	{
		DLOG("PowerMac7_2_PlatformPlugin::readProcROM failed to open I2C\n");
		return(false);
	}
	else
	{
		// The IIC ROM requires two-byte subaddresses, so we use standard mode to address it.
		// Each read is preceeded by a two-byte standard mode write which serves to set the
		// address pointer register in the part.  The read is also performed in standard mode.

		do
		{
			DLOG("PowerMac7_2_PlatformPlugin::readProcROM set standard mode\n");
			if (!setI2CStandardMode())
			{
				DLOG("PowerMac7_2_PlatformPlugin::readProcROM failed to set bus mode\n");
				break;
			}

			// set the address that we'll read from - the subAddr parameter is ignored in standard
			// mode
			DLOG("PowerMac7_2_PlatformPlugin::readProcROM write offset 0x%02X 0x%02X\n",
					((UInt8 *) &offset)[0], ((UInt8 *) &offset)[1]);
			if (!writeI2C( 0x00, (UInt8 *) &offset, 2 ))
			{
				DLOG("PowerMac7_2_PlatformPlugin::readProcROM failed to set address\n");
				break;
			}
	
			// read the data
			DLOG("PowerMac7_2_PlatformPlugin::readProcROM read\n");
			if (!readI2C( 0x00, buf, size ))
			{
				DLOG("PowerMac7_2_PlatformPlugin::readProcROM failed to read data\n");
				break;
			}

			success = true;
		} while (false);

		closeI2C();
	}

	return(success);
}
*/

/*******************************************************************************
 * I2C Helpers
 *******************************************************************************/
/*
bool PowerMac7_2_PlatformPlugin::openI2C( UInt8 busNo, UInt8 addr )
{
	UInt32 passBus;
	IOReturn status;

	DLOG("PowerMac7_2_PlatformPlugin::openI2C bus 0x%02X addr 0x%02X\n", busNo, addr);

	if (fI2C_iface == NULL)
		return false;

	fI2CBus = busNo;
	fI2CAddress = addr;

	// Open the interface
	passBus = (UInt32) fI2CBus;	// cast from 8-bit to machine long word length
	if ((status = (fI2C_iface->callPlatformFunction(kOpenI2Cbus, false, (void *) passBus, NULL, NULL, NULL)))
			!= kIOReturnSuccess)
	{
		IOLog("PowerMac7_2_PlatformPlugin::openI2C failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

void PowerMac7_2_PlatformPlugin::closeI2C( void )
{
	IOReturn status;

	DLOG("PowerMac7_2_PlatformPlugin::closeI2C - entered\n");

	if (fI2C_iface == NULL) return;

	if ((status = (fI2C_iface->callPlatformFunction(kCloseI2Cbus, false, NULL, NULL, NULL, NULL)))
			!= kIOReturnSuccess)
	{
		IOLog("PowerMac7_2_PlatformPlugin::closeI2C failed, status = %08lx\n", (UInt32) status);
	}
}

bool PowerMac7_2_PlatformPlugin::setI2CDumbMode( void )
{
	IOReturn status;

	DLOG("PowerMac7_2_PlatformPlugin::setI2CDumbMode - entered\n");

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetDumbMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("PowerMac7_2_PlatformPlugin::setI2CDumbMode failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

bool PowerMac7_2_PlatformPlugin::setI2CStandardMode( void )
{
	IOReturn status;

	DLOG("PowerMac7_2_PlatformPlugin::setI2CStandardMode - entered\n");

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetStandardMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("PowerMac7_2_PlatformPlugin::setI2CStandardMode failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

bool PowerMac7_2_PlatformPlugin::setI2CStandardSubMode( void )
{
	IOReturn status;

	DLOG("PowerMac7_2_PlatformPlugin::setI2CStandardSubMode - entered\n");

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetStandardSubMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("PowerMac7_2_PlatformPlugin::setI2CStandardSubMode failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

bool PowerMac7_2_PlatformPlugin::setI2CCombinedMode( void )
{
	IOReturn status;

	DLOG("PowerMac7_2_PlatformPlugin::setI2CCombinedMode - entered\n");

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetCombinedMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("PowerMac7_2_PlatformPlugin::setI2CCombinedMode failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

bool PowerMac7_2_PlatformPlugin::writeI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
	UInt32 passAddr, passSubAddr, passSize;
	unsigned int retries = 0;
	IOReturn status;

	DLOG("PowerMac7_2_PlatformPlugin::writeI2C - entered\n");

	if (fI2C_iface == NULL || data == NULL || size == 0)
		return false;

	passAddr = (UInt32) (fI2CAddress >> 1);
	passSubAddr = (UInt32) subAddr;
	passSize = (UInt32) size;

	while ((retries < kNumRetries) &&
	       ((status = (fI2C_iface->callPlatformFunction(kWriteI2Cbus, false, (void *) passAddr,
		    (void *) passSubAddr, (void *) data, (void *) passSize))) != kIOReturnSuccess))
	{
		IOLog("PowerMac7_2_PlatformPlugin::writeI2C transaction failed, status = %08lx\n", (UInt32) status);
		retries++;
	}

	if (retries >= kNumRetries)
		return(false);
	else
		return(true);
}

bool PowerMac7_2_PlatformPlugin::readI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
	UInt32 passAddr, passSubAddr, passSize;
	unsigned int retries = 0;
	IOReturn status;

	DLOG("PowerMac7_2_PlatformPlugin::readI2C \n");

	if (fI2C_iface == NULL || data == NULL || size == 0)
		return false;

	passAddr = (UInt32) (fI2CAddress >> 1);
	passSubAddr = (UInt32) subAddr;
	passSize = (UInt32) size;

	while ((retries < kNumRetries) &&
	       ((status = (fI2C_iface->callPlatformFunction(kReadI2Cbus, false, (void *) passAddr,
		    (void *) passSubAddr, (void *) data, (void *) passSize))) != kIOReturnSuccess))
	{
		IOLog("PowerMac7_2_PlatformPlugin::readI2C transaction failed, status = %08lx\n", (UInt32) status);
		retries++;
	}

	if (retries >= kNumRetries)
		return(false);
	else
		return(true);
}
*/
