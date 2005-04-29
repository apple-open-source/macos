/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CADT746x.cpp,v 1.5 2005/01/08 03:42:49 jlehrer Exp $
 *
 *		$Log: IOI2CADT746x.cpp,v $
 *		Revision 1.5  2005/01/08 03:42:49  jlehrer
 *		[3944335] Set flag to read SMB interrupt registers 0x41 and 0x42 on wake from sleep
 *		
 *		Revision 1.4  2004/12/20 19:22:46  knott
 *		*** empty log message ***
 *		
 *		Revision 1.3  2004/12/17 22:40:28  knott
 *		*** empty log message ***
 *		
 *		Revision 1.2  2004/12/15 04:15:57  jlehrer
 *		[3867728] Support for failed hardware.
 *		
 *		Revision 1.1  2004/11/04 21:11:28  jlehrer
 *		Initial checkin.
 *		
 *
 *
 */

#include <IOKit/IODeviceTreeSupport.h>
#include "IOI2CADT746x.h"

#define super IOI2CDevice

OSDefineMetaClassAndStructors(IOI2CADT746x, IOI2CDevice)

bool IOI2CADT746x::start(IOService *provider)
{
	IOService		*childNub;
	OSArray *		nubArray;

	if (false == super::start(provider))
		return false;

	// Read the device ID register so we know whether this is an ADM1030 or an ADM1031
	if (kIOReturnSuccess != readI2C(kDeviceIDReg, &fDeviceID, 1))
	{
		// If we can not communicate with the device then terminate.
		ERRLOG("IOI2CADT746x@%lx::start device not responding!\n", getI2CAddress());
		freeI2CResources();
		return false;
	}

	// Parse the sensor parameters and create nubs
	// If I didn't create any nubs, I have no reason to exist
	nubArray = parseSensorParamsAndCreateNubs(provider);
	if (nubArray == NULL || nubArray->getCount() == 0)
	{
		ERRLOG("-IOI2CADT746x@%lx::start no thermal sensors found\n", getI2CAddress());
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

	registerService();

	return true;
}


OSArray * IOI2CADT746x::parseSensorParamsAndCreateNubs(IOService *provider)
{
	IOService *nub;
	OSData *tmp_osdata;
	OSArray *nubArray = NULL;
	unsigned i, n_sensors = 0, nubs_created = 0;
	UInt32 version, *id = NULL, *zone = NULL, *polling_period = NULL;
	const char *type = NULL, *location = NULL;
	char work[32];

	// Get the version
	if (NULL == (tmp_osdata = OSDynamicCast(OSData, provider->getProperty(kDTSensorParamsVersionKey))))
		return NULL;

	version = *((UInt32 *)tmp_osdata->getBytesNoCopy());

	if (version == 2) // I can handle version 2 params now! ==)
	{
		IORegistryEntry * deviceNub;
		OSIterator * kids;

		if (kids = provider->getChildIterator( gIODTPlane ))
		{
			while ( deviceNub = (IORegistryEntry *)kids->getNextObject() )
			{
				// if necessary, examine deviceNub to extract device configuration info

				nub = new IOService;
				nub->init( deviceNub, gIODTPlane );
				nub->attach(this);

				// add this nub to the array
				if (nubArray == NULL)
					nubArray = OSArray::withObjects((const OSObject **) &nub, 1);
				else
					nubArray->setObject( nub );
			}

			kids->release();
		}

		return nubArray;
	}
	else
	// I can handle version 1
	if (version != 1)
	{
		ERRLOG("IOI2CADT746x::parseSensorParamsAndCreateNubs(0x%lx) version != 1\n", getI2CAddress());
		return(NULL);
	}

	// Get pointers inside the libkern containers for all properties
	if (NULL == (tmp_osdata = OSDynamicCast(OSData, provider->getProperty(kDTSensorIDKey))))
		return NULL;

	n_sensors = tmp_osdata->getLength() / sizeof(UInt32);
	if (n_sensors > 6)
		return NULL;

	id = (UInt32 *)tmp_osdata->getBytesNoCopy();

	if (NULL == (tmp_osdata = OSDynamicCast(OSData, provider->getProperty(kDTSensorZoneKey))))
		return NULL;

	zone = (UInt32 *)tmp_osdata->getBytesNoCopy();

	if (NULL == (tmp_osdata = OSDynamicCast(OSData, provider->getProperty(kDTSensorTypeKey))))
		return NULL;

	type = (const char *)tmp_osdata->getBytesNoCopy();

	if (NULL == (tmp_osdata = OSDynamicCast(OSData, provider->getProperty(kDTSensorLocationKey))))
		return NULL;

	location = (const char *)tmp_osdata->getBytesNoCopy();

	// Polling Period key is not required
	if (tmp_osdata = OSDynamicCast(OSData, provider->getProperty(kDTSensorPollingPeriodKey)))
		polling_period = (UInt32 *)tmp_osdata->getBytesNoCopy();

	// Iterate through the sensors and create their nubs
	for (i=0; i<n_sensors; i++)
	{
		nub = new IOService;

		if (!nub || !nub->init())
			continue;
                        
		// Create an OSData representation of the sensor nub name string

		if (!strcmp(type, kDTTemperatureSensorType))
		{
			strcpy(work, kHWSensorTemperatureNubName);
			nub->setName(kHWSensorTemperatureNubName);
		}
		else if (!strcmp(type, kDTVoltageSensorType))
		{
			strcpy(work, kHWSensorVoltageNubName);
			nub->setName(kHWSensorVoltageNubName);
		}
		else if (!strcmp(type, kDTFanSpeedSensorType))
		{
			strcpy(work, kHWSensorFanSpeedNubName);
			nub->setName(kHWSensorFanSpeedNubName);
		}
		else
			return NULL;	// Code needs to be reved for types we do not understand yet
			
		tmp_osdata = OSData::withBytes(work, strlen(work) + 1);
		if (tmp_osdata == NULL)
			return NULL;

		nub->attach(this);

		// Make the mapping for this sensor-id
		fHWSensorIDMap[i] = id[i];

		// set name, device_type and compatible to temp-sensor
		
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

		if (polling_period != NULL)
		{
			if (polling_period[i] != kHWSensorPollingPeriodNULL)
				nub->setProperty(kHWSensorPollingPeriodKey, &polling_period[i],
						sizeof(UInt32));
		}

		nubs_created++;

		// add this nub to the array
		if (nubArray == NULL)
			nubArray = OSArray::withObjects((const OSObject **) &nub, 1);
		else
			nubArray->setObject( nub );
	}
            

	tmp_osdata->release();
	return nubArray;
}



IOReturn IOI2CADT746x::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 		id = (UInt32)param1;
	SInt32 		*temp_buf = (SInt32 *)param2;
 	UInt8 		statusByte;
	UInt32		key;
           
	if (0 == functionName)
	{
		ERRLOG("@IOI2CADT746x::callPlatformFunction bad arg!\n");
		return kIOReturnBadArgument;
	}
        
	if (functionName->isEqualTo(kGetSensorValueSymbol) == true)
	{
		if (fClearSMBAlertStatus == true)
		{
			IOReturn status;
			if (kIOReturnSuccess == (status = lockI2CBus(&key)))
			{
				if (kIOReturnSuccess != (status = readI2C(kIntStatusReg1, &statusByte, 1, key)))
					ERRLOG("@IOI2CADT746x::CPF error reading I2C reg:0x%x: 0x%lx\n", kIntStatusReg1, (UInt32)status);
				else
				if (kIOReturnSuccess != (status = readI2C(kIntStatusReg2, &statusByte, 1, key)))
					ERRLOG("@IOI2CADT746x::CPF error reading I2C reg:0x%x: 0x%lx\n", kIntStatusReg2, (UInt32)status);
				
				unlockI2CBus(key);
			}
			else
			{
				ERRLOG("@IOI2CADT746x::CPF error locking I2C bus: 0x%lx\n", (UInt32)status);
				return status;
			}

			fClearSMBAlertStatus = false;
		}
        
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
		else if (id == fHWSensorIDMap[3])
		{
			return(getVoltage(temp_buf));
		}
		else if (id == fHWSensorIDMap[4])
		{
			return(getFanTach(temp_buf, kFanTachOne));
		}
		else if (id == fHWSensorIDMap[5])
		{
			return(getFanTach(temp_buf, kFanTachTwo));
		}
	}

	return(super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4));
}

IOReturn IOI2CADT746x::getFanTach(SInt32 *fanSpeed, SInt16 whichFan)
{
	UInt8		tachLowByteAddr, tachHighByteAddr;
	UInt8		tachLowByte, tachHighByte;
	UInt16		fullValue;
	UInt8		fanPulsePerRev;
	UInt16		fanWhole;
	IOReturn	status;
	UInt32		key;

	if (fanSpeed == NULL)
		return kIOReturnBadArgument;

	if (whichFan == kFanTachOne)
	{
		tachLowByteAddr = kTACH1LowByte;
		tachHighByteAddr = kTACH1HighByte;
	}
	else if (whichFan == kFanTachTwo)
	{
		tachLowByteAddr = kTACH2LowByte;
		tachHighByteAddr = kTACH2HighByte;
	}
	else // We can only read first two tachs right now.
	{
		*fanSpeed = -1;
		return kIOReturnUnsupported;
	}

	if (kIOReturnSuccess == (status = lockI2CBus(&key)))
	{
		// extended needs to be read first, it will freeze the other two voltage registers till all are read.

		// Read the low byte, must be read first
		if (kIOReturnSuccess != (status = readI2C(tachLowByteAddr, &tachLowByte, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getFanTach i2c read low A:0x%x error:0x%x\n", getI2CAddress(), tachLowByteAddr, status);
		else // Then lets get the high byte
		if (kIOReturnSuccess != (status = readI2C(tachHighByteAddr, &tachHighByte, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getFanTach i2c read high A:0x%x error:0x%x\n", getI2CAddress(), tachHighByteAddr, status);
		else // And of course we need to know the proper pulse per revolution
		if (kIOReturnSuccess != (status = readI2C(kFanPulsePerRev, &fanPulsePerRev, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getFanTach i2c read ppr A:0x%x error:0x%x\n", getI2CAddress(), kFanPulsePerRev, status);

		unlockI2CBus(key);
	}
        
	if (kIOReturnSuccess != status)
	{
		*fanSpeed = -1;
		return status;
	}

	fullValue = ((UInt16)tachHighByte << 8) | tachLowByte;
	if ((fullValue == 0xFFFF) || (fullValue == 0x0))		// Fan not spinning or stalled
		fanWhole = 0;
	else
		fanWhole = ((90000 * 60) / fullValue);			// 90K based on 11.11 microsec timing period

	*fanSpeed = (fanWhole << 16);					// Fixed number, so shift it high.

	return kIOReturnSuccess;
}

IOReturn IOI2CADT746x::getVoltage(SInt32 *voltage)
{
	UInt8 		fullByte, ext, dummybyte, config4, config2;
	UInt32		volIndex;
	UInt32		incrementsPerVolt;
	UInt8		vccReadAddr;
	IOReturn	status;
	UInt32		key;
        
	if (voltage == NULL)
		return kIOReturnBadArgument;

	if (kIOReturnSuccess == (status = lockI2CBus(&key)))
	{
		if (fDeviceID == 0xFF) 			// Something went wrong first time, try it again.
			status = readI2C(kDeviceIDReg, &fDeviceID, 1, key);

        // Are we a 7460 or a 7467
        vccReadAddr = ((fDeviceID == kDeviceIDADT7460) ? k2_5VReading : k2_5VccpReading );
    
		// extended needs to be read first, it will freeze the other two voltage registers till all are read.

		// Read the config4 reg
		if (kIOReturnSuccess != (status = readI2C(kConfigReg4, &config4, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getVoltage i2c read config1 A:0x%x error:0x%x\n", getI2CAddress(), kConfigReg1, status);
		else // Read the config2 reg
		if (kIOReturnSuccess != (status = readI2C(kConfigReg2, &config2, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getVoltage i2c read config2 A:0x%x error:0x%x\n", getI2CAddress(), kConfigReg2, status);
		else // Read the extended resolution reg
		if (kIOReturnSuccess != (status = readI2C(kExtendedRes1, &ext, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getVoltage i2c read kExtendedRes1 A:0x%x error:0x%x\n", getI2CAddress(), kExtendedRes1, status);
		else // Read the local temp reg
		if (kIOReturnSuccess != (status = readI2C(vccReadAddr, &fullByte, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getVoltage i2c read vccReadAddr A:0x%x error:0x%x\n", getI2CAddress(), vccReadAddr, status);

		// Now we read the other two so that we can unstick the frozen sampling.
		else // Read the local temp reg
		if (kIOReturnSuccess != (status = readI2C(kVccReading, &dummybyte, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getVoltage i2c read kVccReading A:0x%x error:0x%x\n", getI2CAddress(), kVccReading, status);

		// Should we do anything if we did fail?

		unlockI2CBus(key);
	}

	if (kIOReturnSuccess != status)
	{
		return status;
	}

 	volIndex = (SInt32) VOLTAGE_INDEX_FROM_BYTES(fullByte, ext) * 100;

    if (fDeviceID == kDeviceIDADT7460)			// Voltage calculations for a 7460
    {
        if ((config2 & k2_5VAttenuationMask) != 0)
            incrementsPerVolt = kIncrementPerVolt2_25Max;
        else
            incrementsPerVolt = kIncrementPerVolt3_3Max;
    }
    else										// Voltage calculations for a 7467
    {
        if (((config2 & k2_5VAttenuationMask) != 0) || ((kConfigReg4 & k2_5VAttenuationMask) != 0))
            incrementsPerVolt = kUnitsPerVoltWithAttenuation7467;
        else
            incrementsPerVolt = kUnitsPerVoltWithoutAttenuation7467;
    }
    
    *voltage = ((volIndex << 16) / incrementsPerVolt);

	return kIOReturnSuccess;
}

IOReturn IOI2CADT746x::getLocalTemp(SInt32 *temperature)
{
    SInt8 highbyte;
	UInt8 lowbyte, ext, dummybyte;
	SInt32 sensor_value;
	IOReturn	status;
	UInt32		key;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	if (kIOReturnSuccess == (status = lockI2CBus(&key)))
	{
		// As per the documentation on the ADT7460 chip once the extended register is read, all
		// three remaining thermal registers must also be read, and sampling is stalled until that happens.
		// Extended needs to be read first, it will freeze the other three registers till all are read.

		// Read the extended resolution reg
		if (kIOReturnSuccess != (status = readI2C(kExtendedRes2, &ext, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getLocalTemp i2c read kExtendedRes2 A:0x%x error:0x%x\n", getI2CAddress(), kExtendedRes2, status);
		else // Read the local temp reg
		if (kIOReturnSuccess != (status = readI2C(kLocalTemperature, (UInt8 *)&highbyte, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getLocalTemp i2c read kLocalTemperature A:0x%x error:0x%x\n", getI2CAddress(), kLocalTemperature, status);
		else // Read the kRemote1Temp reg
		if (kIOReturnSuccess != (status = readI2C(kRemote1Temp, &dummybyte, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getLocalTemp i2c read kRemote1Temp A:0x%x error:0x%x\n", getI2CAddress(), kRemote1Temp, status);
		else // Read the kRemote2Temp reg
		if (kIOReturnSuccess != (status = readI2C(kRemote2Temp, &dummybyte, 1, key)))
			ERRLOG("IOI2CADT746x@%lx::getLocalTemp i2c read kRemote2Temp A:0x%x error:0x%x\n", getI2CAddress(), kRemote2Temp, status);

		unlockI2CBus(key);
	}

	if (kIOReturnSuccess != status)
		return status;

	lowbyte = LOCAL_FROM_EXT_TEMP(ext);
	sensor_value = SIGNED_TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;
	return kIOReturnSuccess;
}

IOReturn IOI2CADT746x::getRemote1Temp(SInt32 *temperature)
{
    SInt8 highbyte;
	UInt8 lowbyte, ext, dummybyte;
	SInt32 sensor_value;
	IOReturn status;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	// extended needs to be read first, it will freeze the other three registers till all are read.
        
	// Read the extended resolution reg
	if (kIOReturnSuccess != (status = readI2C(kExtendedRes2, &ext, 1)))
		ERRLOG("IOI2CADT746x::getRemote1Temp error:0x%lx reading I2C reg:%d\n", (UInt32)status, kExtendedRes2);

	// Read the remote 1 temp reg
	if (kIOReturnSuccess != (status = readI2C(kRemote1Temp, (UInt8 *) &highbyte, 1)))
		ERRLOG("IOI2CADT746x::getRemote1Temp error:0x%lx reading I2C reg:%d\n", (UInt32)status, kRemote1Temp);

	// Now we read the other two so that we can unstick the frozen sampling.

	readI2C(kLocalTemperature, &dummybyte, 1);	// Should we do anything if we did fail?
	readI2C(kRemote2Temp, &dummybyte, 1);		// Should we do anything if we did fail?

	// As per the documentation on the ADT7460 chip once the extended register is read, all
	// three remaining thermal registers must also be read, and sampling is stalled until
	// that happens.

	if (status != kIOReturnSuccess)
		return status;

	lowbyte = REMOTE1_FROM_EXT_TEMP(ext);
	sensor_value = SIGNED_TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;
	return kIOReturnSuccess;
}

IOReturn IOI2CADT746x::getRemote2Temp(SInt32 *temperature)
{
    SInt8 highbyte;
	UInt8 lowbyte, ext, dummybyte;
	SInt32 sensor_value;
	IOReturn status;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	// extended needs to be read first, it will freeze the other three registers till all are read.
        
    // Read the extended resolution reg
	if (kIOReturnSuccess != (status = readI2C(kExtendedRes2, &ext, 1)))
		ERRLOG("IOI2CADT746x::getRemote2Temp error:0x%lx reading I2C reg:%d\n", (UInt32)status, kExtendedRes2);

	// Read the remote 2 temp reg
	if (kIOReturnSuccess != (status = readI2C(kRemote2Temp, (UInt8 *) &highbyte, 1)))
		ERRLOG("IOI2CADT746x::getRemote2Temp error:0x%lx reading I2C reg:%d\n", (UInt32)status, kRemote2Temp);

	// Now we read the other two so that we can unstick the frozen sampling.
                
	readI2C(kRemote1Temp, &dummybyte, 1);		// Should we do anything if we did fail?
	readI2C(kLocalTemperature, &dummybyte, 1);	// Should we do anything if we did fail?
            
	// As per the documentation on the ADT7460 chip once the extended register is read, all
	// three remaining thermal registers must also be read, and sampling is stalled until
	// that happens.

	if (status != kIOReturnSuccess)
		return status;

	lowbyte = REMOTE2_FROM_EXT_TEMP(ext);
	sensor_value = SIGNED_TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;

    return kIOReturnSuccess;
}

void IOI2CADT746x::processPowerEvent(UInt32 eventType)
{
	switch (eventType)
	{
		case kI2CPowerEvent_ON:
		case kI2CPowerEvent_WAKE:
			fClearSMBAlertStatus = true;	// 3944335 Need to setup to clear SMB alerts on wake.
			break;
	}
}

