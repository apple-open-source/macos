/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CMaxim6690.cpp,v 1.4 2005/04/11 23:39:59 dirty Exp $
 *
 *		$Log: IOI2CMaxim6690.cpp,v $
 *		Revision 1.4  2005/04/11 23:39:59  dirty
 *		[4078743] Properly handle negative temperatures.
 *		
 *		Revision 1.3  2004/12/15 04:44:51  jlehrer
 *		[3867728] Support for failed hardware.
 *		
 *		Revision 1.2  2004/09/18 01:12:01  jlehrer
 *		Removed APSL header.
 *		
 *		Revision 1.1  2004/06/14 20:26:28  jlehrer
 *		Initial Checkin
 *		
 *		Revision 1.6  2004/01/30 23:52:00  eem
 *		[3542678] IOHWSensor/IOHWControl should use "reg" with version 2 thermal parameters
 *		Remove AppleSMUSensor/AppleSMUFan since that code will be in AppleSMUDevice class.
 *		Fix IOHWMonitor, AppleMaxim6690, AppleAD741x to use setPowerState() API instead of
 *		unsynchronized powerStateWIllChangeTo() API.
 *		
 *		Revision 1.5  2003/08/15 23:34:55  eem
 *		Merged performFunction-leak branch for 3379113, 3379339 and bumped
 *		version to 1.0.4b1.
 *		
 *		Revision 1.4.4.1  2003/08/15 02:26:50  eem
 *		3379113, 3379339: leaking objects in performFunction()
 *		
 *		Revision 1.4  2003/07/03 01:52:06  dirty
 *		Add CVS log and id keywords.  Add more information to i2c error log.
 *		
 */

#include "IOI2CMaxim6690.h"

#define super IOI2CDevice
OSDefineMetaClassAndStructors(IOI2CMaxim6690, IOI2CDevice)

bool IOI2CMaxim6690::init( OSDictionary *dict )
{
	if (!super::init(dict)) return(false);

	fGetSensorValueSym = NULL;

	return(true);
}

void IOI2CMaxim6690::free( void )
{
	super::free();
}

bool IOI2CMaxim6690::start( IOService *nub )
{
	IOService		*childNub;
	OSArray *		nubArray;

	DLOG("IOI2CMaxim6690::start - entered\n");

	if (!fGetSensorValueSym)
		fGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	if (!super::start(nub)) return(false);

	while (isI2COffline())
		IOSleep(10);

	// sanity check on device communication - read the device ID register
	UInt8 deviceID;
	if (kIOReturnSuccess != readI2C(kReadDeviceID, &deviceID, 1))
	{
		IOLog("IOI2CMaxim6690@%lx::start device not responding!\n", getI2CAddress());
		freeI2CResources();
		return false;
	}

	// parse thermal sensor properties and create nubs
	nubArray = parseSensorParamsAndCreateNubs( nub );
	if (nubArray == NULL || nubArray->getCount() == 0)
	{
		IOLog("IOI2CMaxim6690@%lx::start no thermal sensors found\n", getI2CAddress());
		if (nubArray) nubArray->release();
		freeI2CResources();
		return(false);
	}

	// tell the world i'm here
	registerService();

	// tell the world my sensor nubs are here
	for (unsigned int index = 0; index < nubArray->getCount(); index++)
	{
		childNub = OSDynamicCast(IOService, nubArray->getObject(index));
		if (childNub) childNub->registerService();
	}
	nubArray->release();

	return(true);
}

void IOI2CMaxim6690::stop( IOService *nub )
{
	DLOG("IOI2CMaxim6690::stop - entered\n");

	// Execute any functions flagged as "on termination"
	performFunctionsWithFlags(kIOPFFlagOnTerm);

	if (fGetSensorValueSym) { fGetSensorValueSym->release(); fGetSensorValueSym = NULL; }
	if (fPlatformFuncArray) { fPlatformFuncArray->release(); fPlatformFuncArray = NULL; }

	super::stop( nub );
}

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn IOI2CMaxim6690::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 id = (UInt32)param1;
	SInt32 *temp_buf = (SInt32 *)param2;

	DLOG("IOI2CMaxim6690::callPlatformFunction(%x) %s %s %08lx %08lx %08lx %08lx\n",
			getI2CAddress(), functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
			(UInt32) param1, (UInt32) param2, (UInt32) param3, (UInt32) param4);

	if (functionName->isEqualTo(fGetSensorValueSym) == true)
	{
		if (isI2COffline() == true)
			return(kIOReturnOffline);

		if (temp_buf == NULL)
			return(kIOReturnBadArgument);

		if (id == fHWSensorIDMap[0])
		{
			return(getInternalTemp(temp_buf));
		}
		else if (id == fHWSensorIDMap[1])
		{
			return(getExternalTemp(temp_buf));
		}
	}

	return(super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4));
}

/*******************************************************************************
 * Create and set up sensor nubs - parseSensorParamsAndCreateNubs()
 *******************************************************************************/

OSArray *IOI2CMaxim6690::parseSensorParamsAndCreateNubs(IOService *nub)
{
	IOService *childNub;
	OSData *tmp_osdata;
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
		DLOG("IOI2CMaxim6690::parseSensorParamsAndCreateNubs no param version\n");
		return(NULL);
	}

	version = *((UInt32 *)tmp_osdata->getBytesNoCopy());

	// Get pointers inside the libkern containers for all properties
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorIDKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CMaxim6690::parseSensorParamsAndCreateNubs no ids\n");
		return(NULL);
	}

	n_sensors = tmp_osdata->getLength() / sizeof(UInt32);

	// the Maxim 6690 has only two temperature channels.  If there are more
	// sensors than this indicated, something is wacky.
	if (n_sensors > 2)
	{
		DLOG("IOI2CMaxim6690::parseSensorParamsAndCreateNubs too many sensors %u\n", n_sensors);
		return(NULL);
	}

	id = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorZoneKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CMaxim6690::parseSensorParamsAndCreateNubs no zones\n");
		return(NULL);
	}

	zone = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorTypeKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CMaxim6690::parseSensorParamsAndCreateNubs no types\n");
		return(NULL);
	}

	type = (const char *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorLocationKey));
	if (tmp_osdata == NULL)
	{
		DLOG("IOI2CMaxim6690::parseSensorParamsAndCreateNubs no locations\n");
		return(NULL);
	}

	location = (const char *)tmp_osdata->getBytesNoCopy();

	// Polling Period key is not required
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorPollingPeriodKey));
	if (tmp_osdata != NULL)
	{
		polling_period = (UInt32 *)tmp_osdata->getBytesNoCopy();
		DLOG("IOI2CMaxim6690::parseSensorParamsAndCreateNubs polling period %lu\n", polling_period);
	}

	// Create an OSData representation of the sensor nub name string
	strcpy(work, kHWSensorNubName);
	tmp_osdata = OSData::withBytes(work, strlen(work) + 1);
	if (tmp_osdata == NULL) return(0);

	// Iterate through the sensors and create their nubs
	for (i=0; i<n_sensors; i++)
	{
		childNub = OSDynamicCast(IOService, 
				OSMetaClass::allocClassWithName("IOService"));

		if (!childNub || !childNub->init())
			continue;

		childNub->attach(this);

		// Make the mapping for this sensor-id
		fHWSensorIDMap[i] = id[i];

		// set name, device_type and compatible to temp-sensor
		childNub->setName(kHWSensorNubName);
		childNub->setProperty("name", tmp_osdata);
		childNub->setProperty("compatible", tmp_osdata);
		childNub->setProperty("device_type", tmp_osdata);

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

	tmp_osdata->release();
	return(nubArray);
}

#pragma mark -
#pragma mark *** Read Temperature Channels ***
#pragma mark -

/*******************************************************************************
 * Read temperature channels from the device
 *******************************************************************************/

IOReturn IOI2CMaxim6690::getInternalTemp( SInt32 * temp )
{
	IOReturn			status;
	UInt8				integer;
	UInt8				fraction;

	// get the internal temperature register
	if (kIOReturnSuccess != (status = readI2C( kReadInternalTemp, &integer, 1 )))
	{
		IOLog("IOI2CMaxim6690::getInternalTemp read temp failed:0x%x\n", status);
	}
	else	// get the internal extended temperature register
	if (kIOReturnSuccess != (status = readI2C( kReadInternalExtTemp, &fraction, 1 )))
	{
		IOLog("IOI2CMaxim6690::getInternalTemp read ext temp failed:0x%x\n", status);
	}

	if (kIOReturnSuccess != status)	// Set default return value upon error.
		*temp = -1;
	else	// format the 16.16 fixed point temperature and return it
		*temp = ( ( ( ( SInt8 ) integer ) << 16 ) | ( ( fraction & 0xE0 ) << 8) );

	return status;
}

IOReturn IOI2CMaxim6690::getExternalTemp( SInt32 * temp )
{
	IOReturn		status;
	UInt8			integer;
	UInt8			fraction;

	// get the internal temperature register
	if (kIOReturnSuccess != (status = readI2C( kReadExternalTemp, &integer, 1 )))
	{
		IOLog("IOI2CMaxim6690::getExternalTemp read temp failed:0x%x\n", status);
	}
	else	// get the internal extended temperature register
	if (kIOReturnSuccess != (status = readI2C( kReadExternalExtTemp, &fraction, 1 )))
	{
		IOLog("IOI2CMaxim6690::getExternalTemp read ext temp failed:0x%x\n", status);
	}

	if (kIOReturnSuccess != status)	// Set default return value upon error.
		*temp = -1;
	else	// format the 16.16 fixed point temperature and return it
		*temp = ( ( ( ( SInt8 ) integer ) << 16 ) | ( ( fraction & 0xE0 ) << 8) );

	return status;
}

