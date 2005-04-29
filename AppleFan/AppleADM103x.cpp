/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#include "AppleADM103x.h"

#define super IOService

OSDefineMetaClassAndStructors(AppleADM103x, IOService)

bool AppleADM103x::init(OSDictionary *dict)
{
	if (!super::init(dict)) return(false);

	fI2CInterface = NULL;
	fI2CBus = 0x00;
	fI2CAddr = 0x00;
	getSensorValueSymbol = OSSymbol::withCString(kGetSensorValueSymbol);

	return(true);
}

void AppleADM103x::free(void)
{
	if (getSensorValueSymbol) getSensorValueSymbol->release();

	super::free();
}

IOService *AppleADM103x::probe(IOService *provider, SInt32 *score)
{
	OSData *tmp_osdata;
	const char *compat;
	UInt32 params_version;

	DLOG("+AppleADM103x::probe\n");

	do {
		/*
		 * If I find a property key "playform-getTemp" in my provider's
		 * node, we DO NOT want probing to succeed.  That is AppleFan
		 * territory.
		 */
		tmp_osdata =
			OSDynamicCast(OSData, provider->getProperty(kGetTempSymbol));

		if (tmp_osdata != NULL)
		{
			DLOG("-AppleADM103x::probe found platform-getTemp, returning failure\n");
			break;
		}

		/*
		 * I can drive an ADM1030 or an ADM1031.  If the provider represents
		 * something else, PROBING FAILS.
		 */
		tmp_osdata =
			OSDynamicCast(OSData, provider->getProperty("compatible"));

		if (tmp_osdata == NULL)
		{
			DLOG("-AppleADM103x::probe failed to fetch compatible property\n");
			break;
		}

		compat = (const char *)tmp_osdata->getBytesNoCopy();

		if (strcmp(compat, kADM1030Compatible) != 0 &&
		    strcmp(compat, kADM1031Compatible) != 0)
		{
			DLOG("-AppleADM103x::probe unsupported part %s\n", compat);
			break;
		}

		/*
		 * Look for a supported version of sensor parameters in the device
		 * tree.
		 */
		tmp_osdata = OSDynamicCast(OSData,
			provider->getProperty(kDTSensorParamsVersionKey));

		if (tmp_osdata == NULL)
		{
			DLOG("-AppleADM103x::probe sensor params not found\n");
			break;
		}

		// Currently supported version(s): 1
		params_version = *((UInt32 *)tmp_osdata->getBytesNoCopy());

		if (params_version != 1)
		{
			DLOG("-AppleADM103x::probe unsupported sensor params version\n");
			break;
		}

		/*
		 * If we've gotten here, all criteria are met for a successful probe
		 */
		*score = kADM103xProbeSuccessScore;
		return (this);

	} while (false);

	// I can't drive this fan controller
	*score = kADM103xProbeFailureScore;
	return(NULL);
}

bool AppleADM103x::start(IOService *provider)
{
	OSData *tmp_osdata;
	UInt32 *tmp_uint32;
	unsigned n_sensors;
	bool 	success;
        
        UInt8			workValue;
        bool			isThisAP99 = false;
        char 			* platform_name;
        IORegistryEntry   	* root;
        OSData *	    	data = 0;
        

	if (!super::start(provider)) return(false);

	// get my I2C address from the device tree
	tmp_osdata = OSDynamicCast(OSData, provider->getProperty("reg"));
	if (!tmp_osdata)
	{
            IOLog("AppleADM103x::start failed to fetch provider's reg property!\n");
            DLOG("-AppleADM103x::start\n");
            return(false);
	}

	tmp_uint32 = (UInt32 *)tmp_osdata->getBytesNoCopy();
	fI2CBus = (UInt8)((*tmp_uint32 & 0xFF00) >> 8);
	fI2CAddr = (UInt8)(*tmp_uint32 & 0xFF);
	fI2CAddr >>= 1;		// right shift by one to make a 7-bit address

	DLOG("@AppleADM103x::start fI2CBus=%02x fI2CAddr=%02x\n",
			fI2CBus, fI2CAddr);

	// cache a pointer to the I2C driver
	//fI2CInterface = OSDynamicCast(PPCI2CInterface, provider->getProvider());
	fI2CInterface = (PPCI2CInterface *)provider->getProvider();
	if (fI2CInterface == NULL)
	{
		IOLog("AppleADM103x::start failed to find I2C driver\n");
		DLOG("-AppleADM103x::start\n");
		return(false);
	}

	// Read the device ID register so we know whether this is an
	// ADM1030 or an ADM1031
	if (!doI2COpen())
	{
		IOLog("AppleADM103x::start failed to open I2C bus\n");
		DLOG("-AppleADM103x::start\n");
		return(false);
	}

	success = doI2CRead(kDeviceIDReg, &fDeviceID, 1);

	doI2CClose();

	if (!success)
	{
		IOLog("AppleADM103x::start failed to read device ID register\n");
		DLOG("-AppleADM103x::start\n");
		return(false);
	}

	// Parse the sensor parameters and create nubs
	n_sensors =	parseSensorParamsAndCreateNubs(provider);

	// If I didn't create any nubs, I have no reason to exist
	if (n_sensors == 0)
	{
		DLOG("-AppleADM103x::start created zero sensor nubs\n");
		return(false);
	}

	registerService();
        
        // On a P99 ( 12" Powerbook, 867Mhz, Jan 2003 ) we want to adjust the thermal
        // settings to be a bit more agressive about spinning up the fans and lowering
        // overall case temperature.
        
        if ((root = IORegistryEntry::fromPath("/", gIOServicePlane)))
        {
            data = OSDynamicCast(OSData, root->getProperty("model"));
            root->release();
            
            platform_name = (char *) data->getBytesNoCopy();
  
            DLOG("AppleADM103x::start - machine type %s\n", platform_name);

            isThisAP99 = !strcmp(platform_name, "PowerBook6,1");
	}
        else
             DLOG("AppleADM103x::start - failed to access the power plane \n");

        if ( isThisAP99 == true )
        {
            if (doI2COpen())
            {
                // The new fan turn on point specified by marketing is 52deg C ( 0x69 ).
                // The bootROm currently has this set to 64C ( 0x81 ).
                
                workValue = 0x69;	
                doI2CWrite(kLocTminTrangeReg, &workValue, 1);
                doI2CWrite(kRmt1TminTrangeReg, &workValue, 1);

                doI2CClose();
                
                DLOG("AppleADM103x::start - Thermal enhancements should be in place \n");
            }
            else
            {
                DLOG("AppleADM103x::start failed to open I2C bus\n");
                DLOG("-AppleADM103x::start\n");
            }

            doI2CClose();
        }
        else
            DLOG("AppleADM103x::start - This is NOT a P99 \n");

	return(true);
}

void AppleADM103x::stop(IOService *provider)
{
	super::stop(provider);
}


unsigned AppleADM103x::parseSensorParamsAndCreateNubs(IOService *provider)
{
	IOService *nub;
	OSData *tmp_osdata;
	unsigned i, n_sensors = 0, nubs_created = 0;
	UInt32 version, *id = NULL, *zone = NULL, *polling_period = NULL;
	const char *type = NULL, *location = NULL;
	char work[32];

	// Get the version
	tmp_osdata = OSDynamicCast(OSData,
			provider->getProperty(kDTSensorParamsVersionKey));
	if (tmp_osdata == NULL) return(0);

	version = *((UInt32 *)tmp_osdata->getBytesNoCopy());

	// Get pointers inside the libkern containers for all properties
	tmp_osdata = OSDynamicCast(OSData,
			provider->getProperty(kDTSensorIDKey));
	if (tmp_osdata == NULL) return(0);

	n_sensors = tmp_osdata->getLength() / sizeof(UInt32);

	id = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			provider->getProperty(kDTSensorZoneKey));
	if (tmp_osdata == NULL) return(0);

	zone = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			provider->getProperty(kDTSensorTypeKey));
	if (tmp_osdata == NULL) return(0);

	type = (const char *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			provider->getProperty(kDTSensorLocationKey));
	if (tmp_osdata == NULL) return(0);

	location = (const char *)tmp_osdata->getBytesNoCopy();

	// Polling Period key is not required
	tmp_osdata = OSDynamicCast(OSData,
			provider->getProperty(kDTSensorPollingPeriodKey));
	if (tmp_osdata != NULL)
	{
		polling_period = (UInt32 *)tmp_osdata->getBytesNoCopy();
	}

	// Create an OSData representation of the sensor nub name string
	strcpy(work, kHWSensorNubName);
	tmp_osdata = OSData::withBytes(work, strlen(work) + 1);
	if (tmp_osdata == NULL) return(0);

	// Iterate through the sensors and create their nubs
	for (i=0; i<n_sensors; i++)
	{
		nub = new IOService;

		if (!nub || !nub->init())
			continue;

		nub->attach(this);

		// Make the mapping for this sensor-id
		fHWSensorIDMap[i] = id[i];

		// set name, device_type and compatible to temp-sensor
		nub->setName(kHWSensorNubName);
		nub->setProperty("name", tmp_osdata);
		nub->setProperty("compatible", tmp_osdata);
		nub->setProperty("device_type", tmp_osdata);

		// set the sensor properties
		nub->setProperty(kHWSensorParamsVersionKey, &version, sizeof(UInt32));
		nub->setProperty(kHWSensorIDKey, &id[i], sizeof(UInt32));
		nub->setProperty(kHWSensorZoneKey, &zone[i], sizeof(UInt32));

		nub->setProperty(kHWSensorTypeKey, type);
		type += strlen(type) + 1;

		nub->setProperty(kHWSensorLocationKey, location);
		location += strlen(location) + 1;

		if (polling_period[i] != kHWSensorPollingPeriodNULL)
			nub->setProperty(kHWSensorPollingPeriodKey, &polling_period[i],
					sizeof(UInt32));

		// start matching on the nub
		nub->registerService();

		nubs_created++;
	}

	tmp_osdata->release();
	return(nubs_created);
}

IOReturn AppleADM103x::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 id = (UInt32)param1;
	SInt32 *temp_buf = (SInt32 *)param2;

	if (functionName->isEqualTo(getSensorValueSymbol) == true)
	{
		if (id == fHWSensorIDMap[0])
		{
			return(getLocalTemp(temp_buf));
		}
		else if (id == fHWSensorIDMap[1])
		{
			return(getRemote1Temp(temp_buf));
		}
		else if (id == fHWSensorIDMap[2])
		{
			return(getRemote2Temp(temp_buf));
		}
	}

	return(super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4));
}

IOReturn AppleADM103x::getLocalTemp(SInt32 *temperature)
{
	UInt8 highbyte, lowbyte, ext;
	SInt32 sensor_value;
	bool success;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	if (!doI2COpen())
		return kIOReturnIOError;

	success = true;
	do {
		// Read the local temp reg
		if (!doI2CRead(kLocalTempReg, &highbyte, 1))
		{
			success = false;
			break;
		}

		// Read the extended resolution reg
		if (!doI2CRead(kExtTempReg, &ext, 1))
			success = false;

	} while (false);

	doI2CClose();

	if (!success)
		return kIOReturnIOError;

	lowbyte = LOCAL_FROM_EXT_TEMP(ext);
	sensor_value = TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;
	return kIOReturnSuccess;
}

IOReturn AppleADM103x::getRemote1Temp(SInt32 *temperature)
{
	UInt8 highbyte, lowbyte, ext;
	SInt32 sensor_value;
	bool success;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	if (!doI2COpen())
		return kIOReturnIOError;

	success = true;
	do {
		// Read the remote1 temp reg
		if (!doI2CRead(kRemote1TempReg, &highbyte, 1))
		{
			success = false;
			break;
		}

		// Read the extended resolution reg
		if (!doI2CRead(kExtTempReg, &ext, 1))
			success = false;

	} while (false);

	doI2CClose();

	if (!success)
		return kIOReturnIOError;

	lowbyte = REMOTE1_FROM_EXT_TEMP(ext);
	sensor_value = TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;
	return kIOReturnSuccess;
}

IOReturn AppleADM103x::getRemote2Temp(SInt32 *temperature)
{
	UInt8 highbyte, lowbyte, ext;
	SInt32 sensor_value;
	bool success;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	// Double check that this is an ADM1031 -- the 1030 does not have
	// a second remote temperature channel
	if (fDeviceID != kDeviceIDADM1031)
		return kIOReturnNoDevice;

	if (!doI2COpen())
		return kIOReturnIOError;

	success = true;
	do {
		// Read the remote1 temp reg
		if (!doI2CRead(kRemote2TempReg, &highbyte, 1))
		{
			success = false;
			break;
		}

		// Read the extended resolution reg
		if (!doI2CRead(kExtTempReg, &ext, 1))
			success = false;

	} while (false);

	doI2CClose();

	if (!success)
		return kIOReturnIOError;

	lowbyte = REMOTE2_FROM_EXT_TEMP(ext);
	sensor_value = TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;
	return kIOReturnSuccess;
}

bool AppleADM103x::doI2COpen(void)
{
	DLOG("@AppleADM103x::doI2COpen bus=%02x\n", fI2CBus);
	return(fI2CInterface->openI2CBus(fI2CBus));
}

void AppleADM103x::doI2CClose(void)
{
	DLOG("@AppleADM103x::doI2CClose\n");
	fI2CInterface->closeI2CBus();
}

bool AppleADM103x::doI2CRead(UInt8 sub, UInt8 *bytes, UInt16 len)
{
	UInt8 retries;

#ifdef APPLEADM103x_DEBUG
	char	debugStr[128];
	sprintf(debugStr, "@AppleADM103x::doI2CRead addr=%02x sub=%02x bytes=%08x len=%04x",
			fI2CAddr, sub, (unsigned int)bytes, len);
#endif

	fI2CInterface->setCombinedMode();

	retries = kNumRetries;

	while (!fI2CInterface->readI2CBus(fI2CAddr, sub, bytes, len))
	{
		if (retries > 0)
		{
			IOLog("AppleADM103x::doI2CRead read failed, retrying...\n");
			retries--;
		}
		else
		{
			IOLog("AppleADM103x::doI2CRead cannot read from I2C!!\n");
			return(false);
		}
	}

	DLOG("%s (first byte %02x)\n", debugStr, bytes[0]);

	return(true);
}

bool AppleADM103x::doI2CWrite(UInt8 sub, UInt8 *bytes, UInt16 len)
{
	UInt8 retries;

	DLOG("@AppleADM103x::doI2CWrite addr=%02x sub=%02x bytes=%08x len=%04x (first byte %02x)\n",
			fI2CAddr, sub, (unsigned int)bytes, len, bytes[0]);

	fI2CInterface->setStandardSubMode();

	retries = kNumRetries;

	while (!fI2CInterface->writeI2CBus(fI2CAddr, sub, bytes, len))
	{
		if (retries > 0)
		{
			IOLog("AppleADM103x::doI2CWrite write failed, retrying...\n");
			retries--;
		}
		else
		{
			IOLog("AppleADM103x::doI2CWrite cannot write to I2C!!\n");
			return(false);
		}
	}

	return(true);
}
