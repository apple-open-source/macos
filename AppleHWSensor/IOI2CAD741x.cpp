/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CAD741x.cpp,v 1.3 2004/12/15 04:44:50 jlehrer Exp $
 *
 *		$Log: IOI2CAD741x.cpp,v $
 *		Revision 1.3  2004/12/15 04:44:50  jlehrer
 *		[3867728] Support for failed hardware.
 *		
 *		Revision 1.2  2004/09/18 01:15:50  jlehrer
 *		Removed APSL header.
 *		Cleanup softReset.
 *		
 *		Revision 1.1  2004/06/14 20:26:28  jlehrer
 *		Initial Checkin
 *		
 *		Revision 1.7  2004/01/30 23:52:00  eem
 *		[3542678] IOHWSensor/IOHWControl should use "reg" with version 2 thermal parameters
 *		Remove AppleSMUSensor/AppleSMUFan since that code will be in AppleSMUDevice class.
 *		Fix IOHWMonitor, AppleMaxim6690, AppleAD741x to use setPowerState() API instead of
 *		unsynchronized powerStateWIllChangeTo() API.
 *		
 *		Revision 1.6  2003/08/15 23:34:55  eem
 *		Merged performFunction-leak branch for 3379113, 3379339 and bumped
 *		version to 1.0.4b1.
 *		
 *		Revision 1.5.4.1  2003/08/15 02:26:50  eem
 *		3379113, 3379339: leaking objects in performFunction()
 *		
 *		Revision 1.5  2003/07/02 22:54:47  dirty
 *		Add CVS log and id keywords.
 *		
 */

#include "IOI2CAD741x.h"

#define super IOI2CDevice
OSDefineMetaClassAndStructors(IOI2CAD741x, IOI2CDevice)

void IOI2CAD741x::free( void )
{
	super::free();
}

bool IOI2CAD741x::start( IOService *nub )
{
	IOService		*childNub;
	OSArray *		nubArray;
//	IOReturn		retval;

	DLOG("+IOI2CAD741x::start - entered\n");

	if (!fGetSensorValueSym)
		fGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	if (!super::start(nub))
		return(false);

	// soft-reset the device -- failure could mean that the device is not present or not
	// responding
	if (softReset() != kIOReturnSuccess)
	{
		DLOG("-IOI2CAD741x@%lx::start failed to soft-reset device\n", getI2CAddress());
		freeI2CResources();
		return(false);
	}

	// parse thermal sensor properties and create nubs
	nubArray = parseSensorParamsAndCreateNubs( nub );
	if (nubArray == NULL || nubArray->getCount() == 0)
	{
		DLOG("-IOI2CAD741x@%lx::start no thermal sensors found\n", getI2CAddress());
		if (nubArray)
			nubArray->release();
		freeI2CResources();
		return(false);
	}

	// tell the world my sensor nubs are here
	for (unsigned int index = 0; index < nubArray->getCount(); index++)
	{
		childNub = OSDynamicCast(IOService, nubArray->getObject(index));
		if (childNub) childNub->registerService();
	}
	nubArray->release();

	// tell the world i'm here
	registerService();

	DLOG("-IOI2CAD741x@%lx::start\n", getI2CAddress());

	return(true);
}

/*
void IOI2CAD741x::processPowerEvent(UInt32 eventType)
{
	switch (eventType)
	{
		case kI2CPowerEvent_STARTUP:
			// soft-reset the device -- failure could mean that the device is not present or not
			// responding
			if (softReset() != kIOReturnSuccess)
			{
				DLOG("IOI2CAD741x@%lx::start failed to soft-reset device\n", getI2CAddress());
				return;
			}
		break;
	}
}
*/

void IOI2CAD741x::stop( IOService *nub )
{
	if (fGetSensorValueSym) { fGetSensorValueSym->release(); fGetSensorValueSym = NULL; }

	super::stop( nub );
}

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn IOI2CAD741x::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 id = (UInt32)param1;
	UInt32 *value_buf = (UInt32 *)param2;
	SInt32 *temp_buf = (SInt32 *)param2;
	bool found = false;
	UInt8 i;

//	DLOG("IOI2CAD741x::callPlatformFunction %s %s %08lx %08lx %08lx %08lx\n",
//			functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
//			(UInt32) param1, (UInt32) param2, (UInt32) param3, (UInt32) param4);

	if (functionName->isEqualTo(fGetSensorValueSym) == true)
	{
		if (isI2COffline() == true)
		{
			DLOG("IOI2CAD741x@%lx::callPlatformFunction I2C OFFLINE\n", getI2CAddress());
			return(kIOReturnOffline);
		}

		if (temp_buf == NULL)
		{
			DLOG("IOI2CAD741x@%lx::callPlatformFunction BAD ARGS\n", getI2CAddress());
			return(kIOReturnBadArgument);
		}

		for (i=0; i<5; i++)
		{
			if (id == fHWSensorIDMap[i])
			{
				found = true;
				break;
			}
		}

		if (found)
		{
			if (i == 0)
				return(getTemperature(temp_buf));
			else
				return(getADC(i, value_buf));
		}
	}

	return(super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4));
}

/*******************************************************************************
 * Create and set up sensor nubs - parseSensorParamsAndCreateNubs()
 *******************************************************************************/

OSArray *IOI2CAD741x::parseSensorParamsAndCreateNubs(IOService *nub)
{
	IOService *childNub;
	OSData *tmp_osdata, *tempNubName, *adcNubName;
	OSArray *nubArray = NULL;
	unsigned i, n_sensors = 0;
	UInt32 version, *id = NULL, *zone = NULL, *polling_period = NULL;
	const char *type = NULL, *location = NULL;
	char work[32];

	// Get the version
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorParamsVersionKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CAD741x::parseSensorParamsAndCreateNubs no param version\n");
		return(NULL);
	}

	version = *((UInt32 *)tmp_osdata->getBytesNoCopy());

	// Get pointers inside the libkern containers for all properties
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorIDKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CAD741x::parseSensorParamsAndCreateNubs no ids\n");
		return(NULL);
	}

	n_sensors = tmp_osdata->getLength() / sizeof(UInt32);

	// the AD7417 has one temp channel and four ADC channels.  If there are more
	// sensors than this indicated, something is wacky.
	if (n_sensors > 5)
	{
		DLOG("IOI2CAD741x::parseSensorParamsAndCreateNubs too many sensors %u\n", n_sensors);
		return(NULL);
	}

	id = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorZoneKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CAD741x::parseSensorParamsAndCreateNubs no zones\n");
		return(NULL);
	}

	zone = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorTypeKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CAD741x::parseSensorParamsAndCreateNubs no types\n");
		return(NULL);
	}

	type = (const char *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorLocationKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CAD741x::parseSensorParamsAndCreateNubs no locations\n");
		return(NULL);
	}

	location = (const char *)tmp_osdata->getBytesNoCopy();

	// Polling Period key is not required
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorPollingPeriodKey));
	if (tmp_osdata != NULL)
	{
		polling_period = (UInt32 *)tmp_osdata->getBytesNoCopy();
		DLOG("IOI2CAD741x::parseSensorParamsAndCreateNubs polling period %lx\n", polling_period);
	}

	// Create an OSData representation of the sensor nub name string
	strcpy(work, kHWTempSensorNubName);
	tempNubName = OSData::withBytes(work, strlen(work) + 1);
	if (tempNubName == NULL) return(0);

	strcpy(work, kHWADCSensorNubName);
	adcNubName = OSData::withBytes(work, strlen(work) + 1);
	if (adcNubName == NULL) return(0);

	// Iterate through the sensors and create their nubs
	for (i=0; i<n_sensors; i++)
	{
		DLOG("IOI2CAD741x::parseSensorParamsAndCreateNubs child nub %u\n", i);

		childNub = OSDynamicCast(IOService, 
				OSMetaClass::allocClassWithName("IOService"));

		if (!childNub || !childNub->init())
			continue;

		childNub->attach(this);

		// Make the mapping for this sensor-id
		fHWSensorIDMap[i] = id[i];

		// set name, device_type and compatible
		if (strcmp(type, "adc") == 0)
		{
			childNub->setName(kHWADCSensorNubName);
			childNub->setProperty("name", adcNubName);
			childNub->setProperty("compatible", adcNubName);
			childNub->setProperty("device_type", adcNubName);
		}
		else //if (strcmp(type, "temperature") == 0 || strcmp(type, "temp") == 0)
		{
			childNub->setName(kHWTempSensorNubName);
			childNub->setProperty("name", tempNubName);
			childNub->setProperty("compatible", tempNubName);
			childNub->setProperty("device_type", tempNubName);
		}

		// set the sensor properties
		childNub->setProperty(kHWSensorParamsVersionKey, &version, sizeof(UInt32));
		childNub->setProperty(kHWSensorIDKey, &id[i], sizeof(UInt32));
		childNub->setProperty(kHWSensorZoneKey, &zone[i], sizeof(UInt32));

		childNub->setProperty(kHWSensorTypeKey, type);
		type += strlen(type) + 1;

		childNub->setProperty(kHWSensorLocationKey, location);
		location += strlen(location) + 1;

		if (polling_period && polling_period[i] != kHWSensorPollingPeriodNULL)
			childNub->setProperty(kHWSensorPollingPeriodKey, &polling_period[i],
					sizeof(UInt32));

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

	tempNubName->release();
	adcNubName->release();
	return(nubArray);
}

#pragma mark -
#pragma mark *** Software Reset ***
#pragma mark -

/*******************************************************************************
 * Read sensor channels from the device
 *******************************************************************************/

IOReturn IOI2CAD741x::softReset( void )
{
	IOReturn status;
	UInt8 val;
	UInt32 key;

	/* According to the data sheet, software can simulate a reset by writing default
	   values to the config, config2, t_oti and t_hyst registers.  In practice, it
	   has been enough to only set config and config2, and this has the added
	   advantage of not clobbering the overtemp and hysteresis values if we have
	   to reset in the middle of operation. */

	if (kIOReturnSuccess == (status = lockI2CBus(&key)))
	{
		// Set the config1 and config2 registers
		val = 0;
		if (kIOReturnSuccess == (status = writeI2C( kConfig1Reg, &val, 1, key )))
		{
			val = 0;
			if (kIOReturnSuccess == (status = writeI2C( kConfig2Reg, &val, 1, key )))
			{
				// don't care what the value is, just want to make sure the read succeeds
				if (kIOReturnSuccess != (status = readI2C( kConfig1Reg, &val, 1, key )))
					DLOG("IOI2CAD741x::softReset failed to read back cfg1 reg\n");
			}
			else
				DLOG("IOI2CAD741x::softReset failed to write cfg2 reg\n");
		}
		else
			DLOG("IOI2CAD741x::softReset failed to write cfg1 reg\n");

		unlockI2CBus(key);
	}

	return status;
}

#pragma mark -
#pragma mark *** Read Sensor Channels ***
#pragma mark -

/*******************************************************************************
 * Read sensor channels from the device
 *******************************************************************************/

IOReturn IOI2CAD741x::getTemperature( SInt32 * temp )
{
	IOReturn status;
	UInt8 bytes[2];
	SInt16 reading;

	*temp = -1;

	if (kIOReturnSuccess != (status = readI2C( kTempValueReg, bytes, 2 )))
	{
		DLOG("IOI2CAD741x::getTemperature read temp failed!\n");
		*temp = -1;
		return status;
	}

	DLOG("IOI2CAD741x@%lx::getTemperature got bytes 0x%02X 0x%02X\n", getI2CAddress(), bytes[0], bytes[1]);

	reading = *((SInt16 *) bytes);
	// temperature is fixed point 8.2 in most significant 10 bits of the two bytes that were read
	*temp = (((SInt32)(reading & 0xFFC0)) << 8);
	return status;
}

IOReturn IOI2CAD741x::getADC( UInt8 channel, UInt32 * sample )
{
	IOReturn status;
	UInt8 cfg1, tmpByte, bytes[2];
	UInt16 rawSample;
	UInt32 key;

	if (channel < kAD1Channel || channel > kAD4Channel)
	{
		DLOG("IOI2CAD741x::getADC invalid channel\n");
		return(kIOReturnBadArgument);
	}

//	DLOG("IOI2CAD741x::getADC reading channel %u\n", channel);

	// Open the bus - this grabs a mutex in the I2C driver so it's thread-safe
	if (kIOReturnSuccess == (status = lockI2CBus(&key)))
	{
		status = readI2C( kConfig1Reg, &cfg1, 1, key );

		if (kIOReturnSuccess != status)
			DLOG("IOI2CAD741x::getADC read cfg1 failed!\n");
		else
		{
			// set the channel selection bits
			tmpByte = channel << kCfg1ChannelShift;

//			DLOG("IOI2CAD741x::getADC read cfg1 0x%02X\n", cfg1);

			cfg1 = (cfg1 & ~kCfg1ChannelMask) | (tmpByte & kCfg1ChannelMask);

//			DLOG("IOI2CAD741x::getADC new cfg1 0x%02X\n", cfg1);

			// write it back out to the cfg register
			status = writeI2C( kConfig1Reg, &cfg1, 1, key );
		}

		if (kIOReturnSuccess != status)
			DLOG("IOI2CAD741x::getADC write cfg1 failed!\n");
		else
			status = readI2C( kADCReg, bytes, 2, key );

		if (kIOReturnSuccess != status)
			DLOG("IOI2CAD741x::getADC read adc reg failed!\n");

		unlockI2CBus(key);
	}
	else
		DLOG("IOI2CAD741x::getADC error locking bus!\n");

	if (kIOReturnSuccess != status)
	{
		*sample = 0xFFFFFFFF;
		return status;
	}

	DLOG("IOI2CAD741x@%lx::getADC ch:%x got bytes 0x%02X 0x%02X\n", getI2CAddress(), channel, bytes[0], bytes[1]);

	rawSample = *((UInt16 *) bytes);
	*sample = ((UInt32)rawSample) >> 6;	// shift out bits [5:0] which are unused
	return status;
}

