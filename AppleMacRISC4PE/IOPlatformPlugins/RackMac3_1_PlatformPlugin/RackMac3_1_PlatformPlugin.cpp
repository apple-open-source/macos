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
 * Copyright (c) 2003-2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: RackMac3_1_PlatformPlugin.cpp,v 1.8 2004/03/18 02:18:52 eem Exp $
 */


#include "IOPlatformPluginSymbols.h"
//#include <ppc/machine_routines.h>
#include "RackMac3_1_PlatformPlugin.h"

#define super IOPlatformPlugin
OSDefineMetaClassAndStructors(RackMac3_1_PlatformPlugin, IOPlatformPlugin)

RackMac3_1_PlatformPlugin * RM31Plugin;
const OSSymbol * gRM31DIMMFanCtrlLoopTarget;
const OSSymbol * gRM31EnableSlewing;

static const OSData * gRM31EEPROM[2] = { NULL, NULL };

bool RackMac3_1_PlatformPlugin::init( OSDictionary * dict )
{
	if (!super::init(dict)) return(false);

	if (!gRM31DIMMFanCtrlLoopTarget)
		gRM31DIMMFanCtrlLoopTarget = OSSymbol::withCString(kRM31DIMMFanCtrlLoopTarget);

	if (!gRM31EnableSlewing)
		gRM31EnableSlewing = OSSymbol::withCString(kRM31EnableSlewing);

	return(true);
}

void RackMac3_1_PlatformPlugin::free( void )
{
	if (gRM31DIMMFanCtrlLoopTarget)
	{
		gRM31DIMMFanCtrlLoopTarget->release();
		gRM31DIMMFanCtrlLoopTarget = NULL;
	}

	if (gRM31EnableSlewing)
	{
		gRM31EnableSlewing->release();
		gRM31EnableSlewing = NULL;
	}

	super::free();
}

bool RackMac3_1_PlatformPlugin::start( IOService * provider )
{
    const OSNumber * sensorID;
    IOPlatformSensor * powerSensor;
	const OSArray * tempArray;

    DLOG("RackMac3_1_PlatformPlugin::start - entered\n");

    // store the self pointer so helper classes can call readProcROM()
    RM31Plugin = this;

    if (!super::start(provider)) return(false);

	platformPlugin->setEnv(gRM31EnableSlewing, (tempArray = OSArray::withCapacity(0)));
	tempArray->release();
	
#if 0
    // Set flags to tell the system we do dynamic power step
    if (pmRootDomain != 0)
    {
        pmRootDomain->publishFeature("Reduce Processor Speed");
        pmRootDomain->publishFeature("Dynamic Power Step");
    }
#endif

    // set the platform ID
    if (gIOPPluginPlatformID)
        gIOPPluginPlatformID->release();

    gIOPPluginPlatformID = OSSymbol::withCString("RackMac3_1");

    // The CPU Power Sensors are "fake", meaning there is really no IOHWSensor instance that
    // they correspond to.  Nothing will ever register with their sensor ID, so we have to
    // manually add them to the registry so that they show up in SensorLogger.
    //
    // They are sensorIDs 148 and 149
    sensorID = OSNumber::withNumber( 148, 32 );
    powerSensor = lookupSensorByID( sensorID );
    if (powerSensor)
        sensorInfoDicts->setObject( powerSensor->getInfoDict() );
    sensorID->release();

    sensorID = OSNumber::withNumber( 149, 32 );
    powerSensor = lookupSensorByID( sensorID );
    if (powerSensor)
        sensorInfoDicts->setObject( powerSensor->getInfoDict() );
    sensorID->release();

    return(true);
}

/* void RackMac3_1_PlatformPlugin::stop( IOService * provider ) {} */

UInt8 RackMac3_1_PlatformPlugin::probeConfig( void )
{
//    return (UInt8) ml_get_max_cpus();

#define RESIDUAL_PATH_LEN	64
    OSCollectionIterator 	*children;
    IORegistryEntry 		*cpus;
    char 			residual[RESIDUAL_PATH_LEN];
    int 			num_cpus, residual_len = RESIDUAL_PATH_LEN;

    // Count the children of the /cpus node in the device tree to find out
    // if this is a uni or dual
    if ((cpus = IORegistryEntry::fromPath( "/cpus", gIODTPlane, residual, &residual_len, NULL )) == NULL ||
        (children = OSDynamicCast(OSCollectionIterator, cpus->getChildIterator( gIODTPlane ))) == NULL)
        return(1);  // assume dual proc as failure case, so we don't let anything burn up inadvertantly

    cpus->release();

    num_cpus = 0;
    
    while (children->getNextObject() != 0)
        num_cpus++;
    
    children->release();

    if (num_cpus > 1)
        return(1);
    else
        return(0);
#undef RESIDUAL_PATH_LEN
}

bool RackMac3_1_PlatformPlugin::readProcROM( UInt32 procID, UInt16 offset, UInt16 size, UInt8 * buf )
{
    int i;
    const UInt8 * eeprom;

    if (procID > 1 || size == 0 || buf == NULL) return(false);

    // if we don't already have a pointer to this CPU's rom data, get one
    if (gRM31EEPROM[procID] == NULL)
    {
#define RESIDUAL_PATH_LEN	64
        IORegistryEntry 	*deviceNode;
        char 			residual[RESIDUAL_PATH_LEN];
        int 			residual_len = RESIDUAL_PATH_LEN;
        const char 		*devicePrefix = "/u3/i2c/cpuid@";
        char 			devicePath[64];

        strcpy(devicePath, devicePrefix);
        
        if (procID == 0)
            strcat(devicePath, "a0");
        else // procID == 1
            strcat(devicePath, "a2");

        DLOG("RackMac3_1_PlatformPlugin::readProcROM looking for %s\n", devicePath);

        if ((deviceNode = IORegistryEntry::fromPath( devicePath, gIODTPlane, residual, &residual_len, NULL )) == NULL)
        {
            DLOG("RackMac3_1_PlatformPlugin::readProcROM unable to find cpuid node (proc %u)\n", procID);
            return(false);
        }

        if ((gRM31EEPROM[procID] = OSDynamicCast(OSData, deviceNode->getProperty("cpuid"))) == NULL)
        {
            DLOG("RackMac3_1_PlatformPlugin::readProcROM unable to fetch ROM image from device tree (proc %u)\n", procID);
            deviceNode->release();
            return(false);
        }

        deviceNode->release();
#undef RESIDUAL_PATH_LEN
    }

    // fetch the desired data
    eeprom = (const UInt8 *) gRM31EEPROM[procID]->getBytesNoCopy();

    for (i=0; i<size; i++)
    {
        buf[i] = eeprom[offset + i];
    }

    return(true);
}

IOReturn RackMac3_1_PlatformPlugin::wakeHandler(void)
{
	DLOG("RackMac3_1_PlatformPlugin::wakeHandler called\n");

	super::wakeHandler();
	
	// Max brightness for Power LED
	togglePowerLED(true);

	return IOPMAckImplied;
}

typedef struct
{
    int				command;
    IOByteCount		sLength;
    UInt8			*sBuffer;
    IOByteCount		*rLength;
    UInt8			*rBuffer;
} SendMiscCommandParameterBlock;

IOReturn RackMac3_1_PlatformPlugin::togglePowerLED(bool state)
{
	UInt8							sBuffer[3];
	IOByteCount 					rLength;
	UInt8 							rBuffer;
	SendMiscCommandParameterBlock 	sendMiscCommandParameterBlock;
    static IOService				*pmu = NULL;
	IOService						*service = NULL;
	
	DLOG("RackMac3_1_PlatformPlugin::togglePowerLED called, turn power LED %s (0x%x)\n", state ? "on" : "off", state);

	if (pmu == NULL)
	{
		mach_timespec_t waitTimeout;

		waitTimeout.tv_sec = 30;
		waitTimeout.tv_nsec = 0;

		service = waitForService(resourceMatching("IOPMU"), &waitTimeout);
		if (service == NULL)
		{
			IOLog("RackMac3_1_PlatformPlugin::togglePowerLED IOPMU resource not found.\n");
			return false;
		}
		
		pmu = OSDynamicCast(IOService, service->getProperty("IOPMU")); 
		if (pmu == NULL) 
			return false;
	}

	if(state) // on
	{
		// Fill parameter block
		sendMiscCommandParameterBlock.command = 0xdf; // op code for set LED command
		sendMiscCommandParameterBlock.sLength = 0x03; // length of the set LED command
		sendMiscCommandParameterBlock.sBuffer = sBuffer;
		sendMiscCommandParameterBlock.rLength = &rLength;
		sendMiscCommandParameterBlock.rBuffer = &rBuffer;

		sBuffer[0] = 0x00; // 1st byte -- 0x00
		sBuffer[1] = 0x01; // 2nd byte -- 0x00 OFF, 0x01 ON
		sBuffer[2] = 0x00; // 3rd byte -- 0x00 sleepLED, 0x02 runLED
		rBuffer = 0x0;
		rLength = sizeof (rBuffer);		
		pmu->callPlatformFunction("sendMiscCommand", false, (void *)&sendMiscCommandParameterBlock, 0, 0, 0);

		sBuffer[0] = 0x00; // 1st byte -- 0x00
		sBuffer[1] = 0x01; // 2nd byte -- 0x00 OFF, 0x01 ON
		sBuffer[2] = 0x02; // 3rd byte -- 0x00 sleepLED, 0x02 runLED
		rBuffer = 0x0;
		rLength = sizeof (rBuffer);		
		pmu->callPlatformFunction("sendMiscCommand", false, (void *)&sendMiscCommandParameterBlock, 0, 0, 0);
	}
	else // off
	{

	}

	return 0;
}
