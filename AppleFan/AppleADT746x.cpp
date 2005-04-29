/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2003 Apple Computer, Inc.  All rights reserved.
 *
 */

#include <IOKit/IODeviceTreeSupport.h>
#include "AppleADT746x.h"

#define super IOService

OSDefineMetaClassAndStructors(AppleADT746x, IOService)

bool AppleADT746x::init(OSDictionary *dict)
{
	if (!super::init(dict)) return(false);

	fI2CInterface = NULL;
	fI2CBus = 0x00;
	fI2CAddr = 0x00;
        
    fClearSMBAlertStatus = false;

	return(true);
}

void AppleADT746x::free(void)
{
	super::free();
}

IOService *AppleADT746x::probe(IOService *provider, SInt32 *score)
{
	OSData *tmp_osdata;
	const char *compat;
	UInt32 params_version;

	DLOG("+AppleADT746x::probe\n");

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
			DLOG("-AppleADT746x::probe found platform-getTemp, returning failure\n");
			break;
		}

		/*
		 * I can drive an ADT7460.  If the provider represents something else, PROBING FAILS.
		 */
		tmp_osdata =
			OSDynamicCast(OSData, provider->getProperty("compatible"));

		if (tmp_osdata == NULL)
		{
			DLOG("-AppleADT746x::probe failed to fetch compatible property\n");
			break;
		}

		compat = (const char *)tmp_osdata->getBytesNoCopy();

		if (strcmp(compat, kADT7460Compatible) != 0)
		{
                    if (strcmp(compat, kADT7467Compatible) != 0)
                    {
			DLOG("-AppleADT746x::probe unsupported part %s\n", compat);
			break;
                    }
		}

		/*
		 * Look for a supported version of sensor parameters in the device
		 * tree.
		 */
                 
		tmp_osdata = OSDynamicCast(OSData,
			provider->getProperty(kDTSensorParamsVersionKey));

		if (tmp_osdata == NULL)
		{
			DLOG("-AppleADT746x::probe sensor params not found\n");
			break;
		}

		// Currently supported version(s): 1
		params_version = *((UInt32 *)tmp_osdata->getBytesNoCopy());

		if (params_version != 1)
		{
			DLOG("-AppleADT746x::probe unsupported sensor params version\n");
			break;
		}

		/*
		 * If we've gotten here, all criteria are met for a successful probe
		 */
		*score = kADT746xProbeSuccessScore;
		return (this);

	} while (false);

	// I can't drive this fan controller
	*score = kADT746xProbeFailureScore;
	return(NULL);
}

bool AppleADT746x::start(IOService *provider)
{
	OSData 			*tmp_osdata;
	UInt32 			*tmp_uint32;
	unsigned 		n_sensors;
	bool 			success;
	mach_timespec_t		t;
        IOPMrootDomain		*iPMRootDomain;

	if (!super::start(provider)) return(false);

	// get my I2C address from the device tree
	tmp_osdata = OSDynamicCast(OSData, provider->getProperty("reg"));
	if (!tmp_osdata)
	{
		IOLog("AppleADT746x::start failed to fetch provider's reg property!\n");
		DLOG("-AppleADT746x::start\n");
		return(false);
	}

	tmp_uint32 = (UInt32 *)tmp_osdata->getBytesNoCopy();
	fI2CBus = (UInt8)((*tmp_uint32 & 0xFF00) >> 8);
	fI2CAddr = (UInt8)(*tmp_uint32 & 0xFF);
	fI2CAddr >>= 1;		// right shift by one to make a 7-bit address

	DLOG("@AppleADT746x::start fI2CBus=%02x fI2CAddr=%02x\n",
			fI2CBus, fI2CAddr);

	// cache a pointer to the I2C driver
	//fI2CInterface = OSDynamicCast(PPCI2CInterface, provider->getProvider());
	fI2CInterface = (PPCI2CInterface *)provider->getProvider();
	if (fI2CInterface == NULL)
	{
		IOLog("AppleADT746x::start failed to find I2C driver\n");
		DLOG("-AppleADT746x::start\n");
		return(false);
	}

	// Read the device ID register so we know whether this is an
	// ADT7460 or an ADT7467
	if (!doI2COpen())
	{
		IOLog("AppleADT746x::start failed to open I2C bus\n");
		DLOG("-AppleADT746x::start\n");
		return(false);
	}

	success = doI2CRead(kDeviceIDReg, &fDeviceID, 1);

	doI2CClose();

	if (!success)
	{
		IOLog("AppleADT746x::start failed to read device ID register\n");
		DLOG("-AppleADT746x::start\n");
		return(false);
	}

	// Parse the sensor parameters and create nubs
	n_sensors =	parseSensorParamsAndCreateNubs(provider);

	// If I didn't create any nubs, I have no reason to exist
	if (n_sensors == 0)
	{
		DLOG("-AppleADT746x::start created zero sensor nubs\n");
		return(false);
	}

        DLOG("-AppleADT746x::Before registerService() call\n");

	registerService();
        
        DLOG("-AppleADT746x::After registerService() call\n");
        
        // Register sleep and wake notifications
        t.tv_sec = 30;
        t.tv_nsec = 0;

        iPMRootDomain = OSDynamicCast(IOPMrootDomain, waitForService(serviceMatching("IOPMrootDomain"), &t));

        if(iPMRootDomain != 0)
        {
                DLOG("-AppleADT746x::registering PM Interest with IOPMrootDomain\n");
                iPMRootDomain->registerInterestedDriver(this);
        }
        else
        {
                DLOG("-AppleADT746x::failed to register PM interest");
        }

	return(true);
}

void AppleADT746x::stop(IOService *provider)
{
	super::stop(provider);
}


unsigned AppleADT746x::parseSensorParamsAndCreateNubs(IOService *provider)
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
                    return(0);	// Code needs to be reved for types we do not understand yet
                    
                tmp_osdata = OSData::withBytes(work, strlen(work) + 1);
                if (tmp_osdata == NULL) return(0);

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

IOReturn AppleADT746x::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 		id = (UInt32)param1;
	SInt32 		*temp_buf = (SInt32 *)param2;
 	UInt8 		statusByte;
           
        DLOG("@AppleADT746x::callPlatformFunction start\n");
        
	if (functionName->isEqualTo(kGetSensorValueSymbol) == true)
	{
                if (fClearSMBAlertStatus == true)
                {
                    if (doI2COpen())
                    {
                        doI2CRead(kIntStatusReg1, &statusByte, 1);
                        doI2CRead(kIntStatusReg2, &statusByte, 1);
                        
                        doI2CClose();
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

IOReturn AppleADT746x::getFanTach(SInt32 *fanSpeed, SInt16 whichFan)
{
        UInt8		tachLowByteAddr, tachHighByteAddr;
        UInt16		fullValue;
        UInt8		fanPulsePerRev;
        UInt16		fanWhole;
	bool 		success;
        UInt8		*byteLoc;

	if (fanSpeed == NULL)
		return kIOReturnBadArgument;

	if (!doI2COpen())
		return kIOReturnIOError;
                
	success = true;
	do {
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
                else
                {
                    doI2CClose();				// We can only read first two tachs right now.
                    *fanSpeed = -1;
                    return kIOReturnSuccess;
                }
            
                // extended needs to be read first, it will freeze the other two voltage registers till all are read.
        
                // Create a pointer to the second byte
                byteLoc = (UInt8*)&fullValue;
                byteLoc++;
        
                // Read the low byte, must be read first
		if (!doI2CRead(tachLowByteAddr, byteLoc, 1))
			success = false;
                
                // Now make sure we are pointing at the high byte of the word
                byteLoc--;
        
                // Then lets get the high byte
		if (!doI2CRead(tachHighByteAddr, byteLoc, 1))
			success = false;
        
                // And of course we need to know the proper pulse per revolution
		if (!doI2CRead(kFanPulsePerRev, &fanPulsePerRev, 1))
			success = false;
        
	} while (false);

	doI2CClose();
        
	if (!success)
		return kIOReturnIOError;
                
        if ((fullValue == 0xFFFF) || (fullValue == 0x0))		// Fan not spinning or stalled
            fanWhole = 0;
        else
            fanWhole = ((90000 * 60) / fullValue);			// 90K based on 11.11 microsec timing period
        
        *fanSpeed = (fanWhole << 16);					// Fixed number, so shift it high.
        
	return kIOReturnSuccess;
}

IOReturn AppleADT746x::getVoltage(SInt32 *voltage)
{
	UInt8 		fullByte, ext, dummybyte, config2, config4;
	bool 		success = false;
    UInt32		volIndex;
    UInt32		incrementsPerVolt;
    UInt8		vccReadAddr;
        
	if (voltage == NULL)
		return kIOReturnBadArgument;

	if (!doI2COpen())
		return kIOReturnIOError;
                
	if (fDeviceID == 0xFF) 			// Something went wrong first time, try it again.
        success = doI2CRead(kDeviceIDReg, &fDeviceID, 1);
                
	// Are we a 7460 or a 7467
	vccReadAddr = ((fDeviceID == kDeviceIDADT7460) ? k2_5VReading : k2_5VccpReading );
                
	do {
        // Configuration registers needed to determine attenuation state.
		if (!doI2CRead(kConfigReg2, &config2, 1))  				break;
		if (!doI2CRead(kConfigReg4, &config4, 1))  				break;
        
        // Read the extended resolution reg
		if (!doI2CRead(kExtendedRes1, &ext, 1))  				break;
        
		// Read the local temp reg
		if (!doI2CRead(vccReadAddr, &fullByte, 1))  			break;

        // Now we read the other voltage level so that we can unstick the frozen sampling.
		doI2CRead(kVccReading, &dummybyte, 1);

        success = true;
	
    } while (false);

	doI2CClose();

	if (!success) return kIOReturnIOError;

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

IOReturn AppleADT746x::getLocalTemp(SInt32 *temperature)
{
    SInt8 highbyte;
	UInt8 lowbyte, ext, dummybyte;
	SInt32 sensor_value;
	bool success;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	if (!doI2COpen())
		return kIOReturnIOError;

	success = true;
	do {
        // extended needs to be read first, it will freeze the other three registers till all are read.
        
        // Read the extended resolution reg
		if (!doI2CRead(kExtendedRes2, &ext, 1))
			success = false;
        
		// Read the local temp reg
		if (!doI2CRead(kLocalTemperature, (UInt8 *) &highbyte, 1))
		{
			success = false;
			break;
		}

        // As per the documentation on the ADT7460 chip once the extended register is read, all
        // three remaining thermal registers must also be read, and sampling is stalled until
        // that happens.
        //
        // Now we read the other two so that we can unstick the frozen sampling.
                
		doI2CRead(kRemote1Temp, &dummybyte, 1);		// Should we do anything if we did fail?
		doI2CRead(kRemote2Temp, &dummybyte, 1);		// Should we do anything if we did fail?
            

	} while (false);

	doI2CClose();

	if (!success)
		return kIOReturnIOError;
    
	lowbyte = LOCAL_FROM_EXT_TEMP(ext);
	sensor_value = SIGNED_TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;
	return kIOReturnSuccess;

    return kIOReturnSuccess;
}

IOReturn AppleADT746x::getRemote1Temp(SInt32 *temperature)
{
    SInt8 highbyte;
	UInt8 lowbyte, ext, dummybyte;
	SInt32 sensor_value;
	bool success;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	if (!doI2COpen())
		return kIOReturnIOError;

	success = true;
	do {
                // extended needs to be read first, it will freeze the other three registers till all are read.
        
                // Read the extended resolution reg
		if (!doI2CRead(kExtendedRes2, &ext, 1))
			success = false;
        
		// Read the remote 1 temp reg
		if (!doI2CRead(kRemote1Temp, (UInt8 *) &highbyte, 1))
		{
			success = false;
			break;
		}

                // Now we read the other two so that we can unstick the frozen sampling.
                
		doI2CRead(kLocalTemperature, &dummybyte, 1);	// Should we do anything if we did fail?
		doI2CRead(kRemote2Temp, &dummybyte, 1);		// Should we do anything if we did fail?
            
                // As per the documentation on the ADT7460 chip once the extended register is read, all
                // three remaining thermal registers must also be read, and sampling is stalled until
                // that happens.

	} while (false);

	doI2CClose();

	if (!success)
		return kIOReturnIOError;

	lowbyte = REMOTE1_FROM_EXT_TEMP(ext);
	sensor_value = SIGNED_TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;
	return kIOReturnSuccess;
    return kIOReturnSuccess;
}

IOReturn AppleADT746x::getRemote2Temp(SInt32 *temperature)
{
    SInt8 highbyte;
	UInt8 lowbyte, ext, dummybyte;
	SInt32 sensor_value;
	bool success;

	if (temperature == NULL)
		return kIOReturnBadArgument;

	if (!doI2COpen())
		return kIOReturnIOError;

	success = true;
	do {
                // extended needs to be read first, it will freeze the other three registers till all are read.
        
                // Read the extended resolution reg
		if (!doI2CRead(kExtendedRes2, &ext, 1))
			success = false;
        
		// Read the remote 2 temp reg
		if (!doI2CRead(kRemote2Temp, (UInt8 *) &highbyte, 1))
		{
			success = false;
			break;
		}

                // Now we read the other two so that we can unstick the frozen sampling.
                
		doI2CRead(kRemote1Temp, &dummybyte, 1);		// Should we do anything if we did fail?
		doI2CRead(kLocalTemperature, &dummybyte, 1);	// Should we do anything if we did fail?
            
                // As per the documentation on the ADT7460 chip once the extended register is read, all
                // three remaining thermal registers must also be read, and sampling is stalled until
                // that happens.

	} while (false);

	doI2CClose();

	if (!success)
		return kIOReturnIOError;

	lowbyte = REMOTE2_FROM_EXT_TEMP(ext);
	sensor_value = SIGNED_TEMP_FROM_BYTES(highbyte, lowbyte);

	*temperature = sensor_value;
	return kIOReturnSuccess;

    return kIOReturnSuccess;
}

bool AppleADT746x::doI2COpen(void)
{
	DLOG("@AppleADT746x::doI2COpen bus=%02x\n", fI2CBus);
	return(fI2CInterface->openI2CBus(fI2CBus));
}

void AppleADT746x::doI2CClose(void)
{
	DLOG("@AppleADT746x::doI2CClose\n");
	fI2CInterface->closeI2CBus();
}

bool AppleADT746x::doI2CRead(UInt8 sub, UInt8 *bytes, UInt16 len)
{
	UInt8 retries;

#ifdef APPLEADT746x_DEBUG
	char	debugStr[128];
	sprintf(debugStr, "@AppleADT746x::doI2CRead addr=%02x sub=%02x bytes=%08x len=%04x",
			fI2CAddr, sub, (unsigned int)bytes, len);
#endif

	fI2CInterface->setCombinedMode();

	retries = kNumRetries;

	while (!fI2CInterface->readI2CBus(fI2CAddr, sub, bytes, len))
	{
		if (retries > 0)
		{
			IOLog("AppleADT746x::doI2CRead read failed, retrying...\n");
			retries--;
		}
		else
		{
			IOLog("AppleADT746x::doI2CRead cannot read from I2C!!\n");
			return(false);
		}
	}

//	DLOG("%s (first byte %02x)\n", debugStr, bytes[0]);

	return(true);
}

bool AppleADT746x::doI2CWrite(UInt8 sub, UInt8 *bytes, UInt16 len)
{
	UInt8 retries;

	DLOG("@AppleADT746x::doI2CWrite addr=%02x sub=%02x bytes=%08x len=%04x (first byte %02x)\n",
			fI2CAddr, sub, (unsigned int)bytes, len, bytes[0]);

	fI2CInterface->setStandardSubMode();

	retries = kNumRetries;

	while (!fI2CInterface->writeI2CBus(fI2CAddr, sub, bytes, len))
	{
		if (retries > 0)
		{
			IOLog("AppleADT746x::doI2CWrite write failed, retrying...\n");
			retries--;
		}
		else
		{
			IOLog("AppleADT746x::doI2CWrite cannot write to I2C!!\n");
			return(false);
		}
	}

	return(true);
}

#pragma mark  
#pragma mark *** Power Management ***

// power management notification routines 

IOReturn AppleADT746x::powerStateWillChangeTo(IOPMPowerFlags theFlags, unsigned long, IOService*)
{
	bool 	waking;

	waking = ((theFlags & IOPMPowerOn) != 0);
	
	// we only want to call doSleepWake from here when we are waking up
	if(waking)
	{   // Set the boolean to make sure we read the 7460 status register to clear the SMBAlert status.
         
            fClearSMBAlertStatus = true;
        }
	
    return IOPMAckImplied;
}

IOReturn AppleADT746x::powerStateDidChangeTo(IOPMPowerFlags theFlags, unsigned long, IOService*)
{
    bool waking;

    waking = ((theFlags & IOPMPowerOn) != 0);
    
    // For now, this code needs to do nothing when going to sleep.
	
    return IOPMAckImplied;
}


