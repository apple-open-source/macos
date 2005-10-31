/*	File: $Id: IOI2CLM7x.cpp,v 1.4 2005/04/11 23:39:29 dirty Exp $
 *
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Tom Sherman
 *
 *		$Log: IOI2CLM7x.cpp,v $
 *		Revision 1.4  2005/04/11 23:39:29  dirty
 *		[4078743] Properly handle negative temperatures.
 *		
 *		Revision 1.3  2004/12/15 04:44:51  jlehrer
 *		[3867728] Support for failed hardware.
 *		
 *		Revision 1.2  2004/09/18 01:12:01  jlehrer
 *		Removed APSL header.
 *		
 *
 *
 */

#include "IOI2CLM7x.h"


#define LM7x_DEBUG 0

#if (defined(LM7x_DEBUG) && LM7x_DEBUG)
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define ERRLOG(fmt, args...)  kprintf(fmt, ## args)

#define super IOI2CDevice
OSDefineMetaClassAndStructors(IOI2CLM7x, IOI2CDevice)

bool IOI2CLM7x::start(
	IOService			*provider)
{
	IOService			*childNub;
	OSArray 			*nubArray;

	DLOG("+IOI2CLM7x::start\n");

	fRegistersAreSaved = false;

	sGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	// Start I2CDriver first...
	if ( !(super::start(provider)) )
		return false;

	if (fInitHWFailed)
	{
		ERRLOG("-IOI2CLM7x::start(0x%lx) device not responding\n", getI2CAddress());
		
		freeI2CResources();
		return false;
	}

	// Parse sensor properties and create nubs
	nubArray = parseSensorParamsAndCreateNubs(fProvider);
	if (nubArray == NULL || nubArray->getCount() == 0)
	{
		ERRLOG("-IOI2CLM7x::start(0x%lx) no thermal sensors found\n", getI2CAddress());
		if (nubArray)
			nubArray->release();
		freeI2CResources();
		return false;
	}

	// Register so others can find us with a waitForService()
	registerService();

	// tell the world my sensor nubs are here
	for (unsigned int index = 0; index < nubArray->getCount(); index++)
	{
		childNub = OSDynamicCast(IOService, nubArray->getObject(index));
		if (childNub) 
			childNub->registerService();
	}
	nubArray->release();

/*
	// Configure the DS1775
	if( initHW() != kIOReturnSuccess )
	{
		DLOG("-IOI2CLM7x::start(0x%lx) failed to initialize sensor.\n", getI2CAddress());
		return false;
	}

	// Parse sensor properties and create nubs
	nubArray = parseSensorParamsAndCreateNubs(provider);
	if (nubArray == NULL || nubArray->getCount() == 0)
	{
		DLOG("-IOI2CLM7x::start(0x%lx) no thermal sensors found\n", getI2CAddress());
		if (nubArray)
			nubArray->release();
        
		return false;
	}

	// Register so others can find us with a waitForService()
	registerService();

	// tell the world my sensor nubs are here
	for (unsigned int index = 0; index < nubArray->getCount(); index++)
	{
		childNub = OSDynamicCast(IOService, nubArray->getObject(index));
		if (childNub) 
			childNub->registerService();
	}
	nubArray->release();
*/

	DLOG("-IOI2CLM7x::start(0x%lx)\n", getI2CAddress());
	return true;
}

void IOI2CLM7x::free(void)
{
	if (sGetSensorValueSym)		{ sGetSensorValueSym->release();		sGetSensorValueSym = 0; }

	super::free();
}

IOReturn IOI2CLM7x::initHW(void)
{
	IOReturn status;
	UInt8 cfgReg;
	UInt32 key;

	DLOG("IOI2CLM7x::initHW(0x%x) entered.\n", getI2CAddress());

	if (kIOReturnSuccess == (status = lockI2CBus(&key)))
	{
		if (kIOReturnSuccess == (status = readI2C(kConfigurationReg, &cfgReg, 1, key)))
		{
			// Set R0 and R1 to get 12-bit resolution
			cfgReg |= (kCfgRegR0 | kCfgRegR1);

			// Failure of this write operation is not fatal -- just won't have extended temperature resolution
			status = writeI2C(kConfigurationReg, &cfgReg, 1, key);
		}
		unlockI2CBus(key);
	}

	if (kIOReturnSuccess != status)
		fInitHWFailed = true;

	return kIOReturnSuccess;
}

IOReturn IOI2CLM7x::getTemperature(SInt32 *temperature)
{
    IOReturn 	status;
    UInt8 	bytes[2];
    SInt16	reading;
    
    status = readI2C((UInt8)kTemperatureReg, bytes, 2);
    if(status != kIOReturnSuccess)
    {
        ERRLOG("IOI2CLM7x::getTemperature(0x%x) readI2C failed.\n", getI2CAddress());
        return status;
    }

    reading = *((SInt16 *) bytes);
    // Temperature data is represented by a 9-bit, two’s complement word with 
    // an LSB equal to 0.5C (least significant 7 bits are undefined).

	// Extra casting is required to make sure we sign-extend the temperature, to
	// handle negative temperatures.
	*temperature = ( ( ( SInt16 ) ( reading & 0xFF80 ) ) << 8 );

    return kIOReturnSuccess;
}

#pragma mark -
#pragma mark *** Platform Functions ***
#pragma mark -

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn IOI2CLM7x::callPlatformFunction(
    const OSSymbol *functionName,
    bool waitForFunction,
    void *param1, 
    void *param2,
    void *param3, 
    void *param4)
{
    UInt32 	id = (UInt32)param1;
    SInt32 	*value = (SInt32 *)param2;
    
    DLOG("IOI2CLM7x::callPlatformFunction(0x%x) %s %s %08lx %08lx %08lx %08lx\n",
        getI2CAddress(),
        functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
        (UInt32)param1,
        (UInt32)param2,
        (UInt32)param3,
        (UInt32)param4);

    if (functionName->isEqualTo(sGetSensorValueSym) == TRUE)
    {
		if (isI2COffline())
			return kIOReturnOffline;

        if (id == fHWSensorIDMap[0])
            return(getTemperature(value));
    }
    
    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

OSArray *IOI2CLM7x::parseSensorParamsAndCreateNubs(IOService *nub)
{
    IOService 	*childNub;
    OSData 	*tempOSData;
    OSArray 	*nubArray = NULL;
    unsigned 	i, n_sensors = 0;
    UInt32 	version, *id = NULL, *zone = NULL, *polling_period = NULL;
    const char 	*type = NULL, *location = NULL;
    char 	work[32];

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorParamsVersionKey));
    if (tempOSData == NULL)
    {
        ERRLOG("IOI2CLM7x::parseSensorParamsAndCreateNubs(0x%x) no param version\n", getI2CAddress());
        return(NULL);
    }

    version = *((UInt32 *)tempOSData->getBytesNoCopy());

	// I can only handle version 1
	if (version != 1)
	{
		ERRLOG("IOI2CLM7x::parseSensorParamsAndCreateNubs(0x%x) version != 1\n", getI2CAddress());
		return(NULL);
	}

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorIDKey));
    if (tempOSData == NULL)
    {
        ERRLOG("IOI2CLM7x::parseSensorParamsAndCreateNubs(0x%x) no ids\n", getI2CAddress());
        return(NULL);
    }

    n_sensors = tempOSData->getLength() / sizeof(UInt32);

    // the LM75 has only one temperature channel.  If there are more
    // sensors than this, something is wacky.
    if (n_sensors > 1)
    {
        ERRLOG("IOI2CLM7x::parseSensorParamsAndCreateNubs(0x%x) too many sensors %u\n", getI2CAddress(), n_sensors);
        return(NULL);
    }

    id = (UInt32 *)tempOSData->getBytesNoCopy();

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorZoneKey));
    if (tempOSData == NULL)
    {
        ERRLOG("IOI2CLM7x::parseSensorParamsAndCreateNubs(0x%x) no zones\n", getI2CAddress());
        return(NULL);
    }

    zone = (UInt32 *)tempOSData->getBytesNoCopy();

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorTypeKey));
    if (tempOSData == NULL)
    {
        ERRLOG("IOI2CLM7x::parseSensorParamsAndCreateNubs(0x%x) no types\n", getI2CAddress());
        return(NULL);
    }

    type = (const char *)tempOSData->getBytesNoCopy();

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorLocationKey));
    if (tempOSData == NULL)
    {
        ERRLOG("IOI2CLM7x::parseSensorParamsAndCreateNubs(0x%x) no locations\n", getI2CAddress());
        return(NULL);
    }

    location = (const char *)tempOSData->getBytesNoCopy();

    // Polling Period key is not required
    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorPollingPeriodKey));
    if (tempOSData != NULL)
    {
        polling_period = (UInt32 *)tempOSData->getBytesNoCopy();
        DLOG("IOI2CLM7x::parseSensorParamsAndCreateNubs(0x%x) polling period %lu\n", getI2CAddress(), polling_period);
    }

    // Create an OSData representation of the sensor nub name string
    strcpy(work, kHWSensorNubName);
    tempOSData = OSData::withBytes(work, strlen(work) + 1);
    if (tempOSData == NULL) 
    {
        return(0);
    }
    
    // Iterate through the sensors and create their nubs
    for (i=0; i<n_sensors; i++)
    {
        childNub = OSDynamicCast(IOService, OSMetaClass::allocClassWithName("IOService"));
    
        if (!childNub || !childNub->init())
            continue;
    
        childNub->attach(this);
    
        // Make the mapping for this sensor-id
        fHWSensorIDMap[i] = id[i];
    
        // set name, device_type and compatible to temp-sensor
        childNub->setName(kHWSensorNubName);
        childNub->setProperty("name", tempOSData);
        childNub->setProperty("compatible", tempOSData);
        childNub->setProperty("device_type", tempOSData);
    
        // set the sensor properties
        childNub->setProperty(kHWSensorParamsVersionKey, &version, sizeof(UInt32));
        childNub->setProperty(kHWSensorIDKey, &id[i], sizeof(UInt32));
        childNub->setProperty(kHWSensorZoneKey, &zone[i], sizeof(UInt32));
    
        childNub->setProperty(kHWSensorTypeKey, type);
        type += strlen(type) + 1;
    
        childNub->setProperty(kHWSensorLocationKey, location);
        location += strlen(location) + 1;
    
        if ( polling_period )
        {
            childNub->setProperty(kHWSensorPollingPeriodKey, &polling_period[i], sizeof(UInt32));
        }

        // add this nub to the array
        if (nubArray == NULL)
        {
            nubArray = OSArray::withObjects((const OSObject **) &childNub, 1);
        }
        else
        {
            nubArray->setObject( childNub );
        }
    }

    tempOSData->release();
    return(nubArray);
}

#pragma mark -
#pragma mark *** Power Management ***
#pragma mark -

/*******************************************************************************
 * Power Management callbacks and handlers
 *******************************************************************************/

void IOI2CLM7x::processPowerEvent(UInt32 eventType)
{
	switch (eventType)
	{
		case kI2CPowerEvent_OFF:
		case kI2CPowerEvent_SLEEP:
			if ( saveRegisters() != kIOReturnSuccess )
				ERRLOG("IOI2CLM7x::processPowerEvent(0x%x) failed to save registers.\n", getI2CAddress());
			else
				fRegistersAreSaved = true;
			break;

		case kI2CPowerEvent_ON:
		case kI2CPowerEvent_WAKE:
			if (fRegistersAreSaved == true)
			{
				// Full Power State
				if ( restoreRegisters() != kIOReturnSuccess )
					ERRLOG("IOI2CLM7x::processPowerEvent(0x%x) failed to restore registers.\n", getI2CAddress());
				fRegistersAreSaved = false;
			}
			break;

		case kI2CPowerEvent_STARTUP:
			// Configure the DS1775
			if( initHW() != kIOReturnSuccess )
				ERRLOG("-IOI2CLM7x::processPowerEvent(0x%lx) failed to initialize sensor.\n", getI2CAddress());
			break;
    }
}


IOReturn IOI2CLM7x::saveRegisters(void)
{
    IOReturn status;

    DLOG("+IOI2CLM7x::saveRegisters(0x%x) entered.\n", getI2CAddress());

    status = readI2C(kConfigurationReg, (UInt8 *)&savedRegisters.Configuration, 1);
    if(status != kIOReturnSuccess)
    {
        ERRLOG("-IOI2CLM7x::saveRegisters(0x%x) readI2C failed.\n", getI2CAddress());
        return status;
    }

    status = readI2C(kT_hystReg, (UInt8 *)&savedRegisters.Thyst, 2);
    if(status != kIOReturnSuccess)
    {
        ERRLOG("-IOI2CLM7x::saveRegisters(0x%x) readI2C failed.\n", getI2CAddress());
        return status;
    }

    status = readI2C(kT_osReg, (UInt8 *)&savedRegisters.Tos, 2);
    if(status != kIOReturnSuccess)
    {
        ERRLOG("-IOI2CLM7x::saveRegisters(0x%x) readI2C failed.\n", getI2CAddress());
        return status;
    }

    DLOG("-IOI2CLM7x::saveRegisters(0x%x) done.\n", getI2CAddress());
    return status;
}

IOReturn IOI2CLM7x::restoreRegisters(void)
{
    IOReturn status;

    DLOG("+IOI2CLM7x::restoreRegisters(0x%x) entered.\n", getI2CAddress());

    status = writeI2C(kConfigurationReg, (UInt8 *)&savedRegisters.Configuration, 1);
    if(status != kIOReturnSuccess)
    {
        ERRLOG("-IOI2CLM7x::restoreRegisters(0x%x) writeI2C failed.\n", getI2CAddress());
        return status;
    }

    status = writeI2C(kT_hystReg, (UInt8 *)&savedRegisters.Thyst, 2);
    if(status != kIOReturnSuccess)
    {
        ERRLOG("-IOI2CLM7x::restoreRegisters(0x%x) writeI2C failed.\n", getI2CAddress());
        return status;
    }

    status = writeI2C(kT_osReg, (UInt8 *)&savedRegisters.Tos, 2);
    if(status != kIOReturnSuccess)
    {
        ERRLOG("-IOI2CLM7x::restoreRegisters(0x%x) writeI2C failed.\n", getI2CAddress());
        return status;
    }

    DLOG("-IOI2CLM7x::restoreRegisters(0x%x) done.\n", getI2CAddress());
    return status;
}
