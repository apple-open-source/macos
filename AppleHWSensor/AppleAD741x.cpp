/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 *  File: $Id: AppleAD741x.cpp,v 1.9 2005/04/26 23:20:22 mpontil Exp $
 *
 *  DRI: Eric Muehlhausen
 *
 *		$Log: AppleAD741x.cpp,v $
 *		Revision 1.9  2005/04/26 23:20:22  mpontil
 *		Fixed a build problem when logging was enabled.
 *		
 *		Revision 1.8  2005/04/11 23:38:44  dirty
 *		[4078743] Properly handle negative temperatures.
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

#include "AppleAD741x.h"

#define super IOService
OSDefineMetaClassAndStructors(AppleAD741x, IOService)

bool AppleAD741x::init( OSDictionary *dict )
{
	if (!super::init(dict)) return(false);

	fGetSensorValueSym = NULL;
	pmRootDomain = NULL;
	fPlatformFuncArray = NULL;
	fI2CReadBufferSize = 0;

	fSleep = false;

	return(true);
}

void AppleAD741x::free( void )
{
	super::free();
}

bool AppleAD741x::start( IOService *nub )
{
	IOService		*parentDev, *childNub;
	UInt32			fullAddr;
	OSData *		regprop;
	OSArray *		nubArray;
	const char *	drvName;
	const OSSymbol *instantiatePFuncs = OSSymbol::withCString("InstantiatePlatformFunctions");
	IOReturn		retval;

	DLOG("AppleAD741x::start - entered\n");

	if (!super::start(nub)) return(false);

	if (!fGetSensorValueSym)
		fGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	// Extract bus number and address from reg property
	if ((regprop = OSDynamicCast(OSData, nub->getProperty("reg"))) == NULL)
	{
		DLOG("AppleAD741x::start no reg property!\n");
		return(false);
	}
	else
	{
		fullAddr = *((UInt32 *)regprop->getBytesNoCopy());
		fI2CBus = (UInt8)((fullAddr >> 8) & 0x000000ff);
		fI2CAddress = (UInt8)(fullAddr & 0x000000fe);
		
		DLOG("AppleAD741x::start fI2CBus = %02x fI2CAddress = %02x\n",
				fI2CBus, fI2CAddress);
	}

	// Find the i2c driver
	if ((parentDev = OSDynamicCast(IOService, nub->getParentEntry(gIODTPlane))) != NULL)
	{
		DLOG("AppleAD741x::start got parentDev %s\n", parentDev->getName());
	}
	else
	{
		DLOG("AppleAD741x::start failed to get parentDev\n");
		return(false);
	}

	drvName = parentDev->getName();
	if (strcmp(drvName, "i2c") != 0)
	{
		DLOG("AppleAD741x::start warning: unexpected i2c device name %s\n", drvName);
	}

	if ((fI2C_iface = OSDynamicCast(IOService, parentDev->getChildEntry(gIOServicePlane))) != NULL &&
	    (fI2C_iface->metaCast("PPCI2CInterface") != NULL))
	{
		DLOG("AppleAD741x::start got fI2C_iface %s\n", fI2C_iface->getName());
	}
	else
	{
		DLOG("AppleAD741x::start failed to get fI2C_iface\n");
		return(false);
	}

	// soft-reset the device -- failure could mean that the device is not present or not
	// responding
	if (softReset() != kIOReturnSuccess)
	{
		IOLog("AppleAD741x::start failed to soft-reset device at bus %x addr %x\n", fI2CBus, fI2CAddress);
		return(false);
	}

	// parse thermal sensor properties and create nubs
	nubArray = parseSensorParamsAndCreateNubs( nub );
	if (nubArray == NULL || nubArray->getCount() == 0)
	{
		DLOG("AppleAD741x::start no thermal sensors found\n");
		if (nubArray) nubArray->release();
		return(false);
	}

	// Scan for platform-do-xxx functions
	fPlatformFuncArray = NULL;

	DLOG("AppleAD741x::start(%x) calling InstantiatePlatformFunctions\n", fI2CAddress);

	retval = nub->getPlatform()->callPlatformFunction(instantiatePFuncs, false,
			(void *)nub, (void *)&fPlatformFuncArray, (void *)0, (void *)0);
	instantiatePFuncs->release();

	DLOG("AppleAD741x::start(%x) InstantiatePlatformFunctions returned %ld, pfArray %sNULL\n",
			fI2CAddress, retval, fPlatformFuncArray ? "NOT " : "");

	if (retval == kIOReturnSuccess && (fPlatformFuncArray != NULL))
	{
		int i, funcCount;
		UInt32 flags;
		IOPlatformFunction *func;
		bool doSleepWake = false;

		funcCount = fPlatformFuncArray->getCount();

		DLOG("AppleAD741x::start(%x) iterating platformFunc array, count = %ld\n",
			fI2CAddress, funcCount);

		for (i = 0; i < funcCount; i++)
		{
			if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
			{
				flags = func->getCommandFlags();

				DLOG("AppleAD741x::start(%x) got function, flags 0x%08lx, pHandle 0x%08lx\n", 
					fI2CAddress, flags, func->getCommandPHandle());

				// If this function is flagged to be performed at initialization, do it
				if (flags & kIOPFFlagOnInit)
					performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);

				// If we need to do anything at sleep/wake time, we'll need to set this
				// flag so we know to register for notifications
				if (flags & (kIOPFFlagOnSleep | kIOPFFlagOnWake))
					doSleepWake = true;
			}
			else
			{
				// This function won't be used -- generate a warning
				DLOG("AppleAD741x::start(%x) not an IOPlatformFunction object\n",
						fI2CAddress);
			}
		}

		// Register sleep and wake notifications
		if (doSleepWake)
		{
			mach_timespec_t	waitTimeout;

			waitTimeout.tv_sec = 30;
			waitTimeout.tv_nsec = 0;

			pmRootDomain = OSDynamicCast(IOPMrootDomain,
					waitForService(serviceMatching("IOPMrootDomain"), &waitTimeout));

			if (pmRootDomain != NULL)
			{
				DLOG("AppleAD741x::start to acknowledge power changes\n");
				pmRootDomain->registerInterestedDriver(this);
			}
			else
			{
				DLOG("AppleAD741x::start(%x) failed to register PM interest\n",
						fI2CAddress);
			}
		}
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

void AppleAD741x::stop( IOService *nub )
{
	UInt32 flags, i;
	IOPlatformFunction *func;

	DLOG("AppleAD741x::stop - entered\n");

	// Execute any functions flagged as "on termination"
	for (i = 0; i < fPlatformFuncArray->getCount(); i++)
	{
		if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
		{
			flags = func->getCommandFlags();

			if (flags & kIOPFFlagOnTerm) 
				performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
		}
	}



	if (fGetSensorValueSym) { fGetSensorValueSym->release(); fGetSensorValueSym = NULL; }
	if (fPlatformFuncArray) { fPlatformFuncArray->release(); fPlatformFuncArray = NULL; }

	super::stop( nub );
}

/*******************************************************************************
 * Execute a platform function - performFunction()
 *******************************************************************************/

bool AppleAD741x::performFunction(IOPlatformFunction *func, void *pfParam1 = 0,
			void *pfParam2 = 0, void *pfParam3 = 0, void *pfParam4 = 0)
{
	bool						success = true;		// initialize to success
	IOPlatformFunctionIterator 	*iter;
	UInt8						scratchBuffer[READ_BUFFER_LEN], mode = kI2CUnspecifiedMode;
	UInt8						*maskBytes, *valueBytes;
	unsigned					delayMS;
	UInt32 						cmd, cmdLen, result, param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;

	DLOG ("AppleAD741x::performFunction(%x) - entered\n", fI2CAddress);

	if (!func)
		return false;
	
	if (!(iter = func->getCommandIterator()))
		return false;
	
	while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, &result)) {
		if (result != kIOPFNoError) {
			DLOG("AppleAD741x::performFunction(%x) error parsing platform function\n", fI2CAddress);

			success = false;
			goto PERFORM_FUNCTION_EXIT;
		}

		DLOG ("AppleAD741x::performFunction(%x) - 1)0x%lx, 2)0x%lx, 3)0x%lx, 4)0x%lx, 5)0x%lx,"
				"6)0x%lx, 7)0x%lx, 8)0x%lx, 9)0x%lx, 10)0x%lx\n", fI2CAddress, param1, param2, param3,
				param4, param5, param6, param7, param8, param9, param10);

		switch (cmd)
		{
			// kCommandReadI2C, kCommandWriteI2C and kCommandRMWI2C are not supported
			// because they don't specify the command/subaddress for the max6690
			// communication.

			case kCommandDelay:

				// param1 is in microseconds, but we are going to sleep so use
				// milliseconds
				delayMS = param1 / 1000;
				if (delayMS != 0) IOSleep(delayMS);

				DLOG("AppleAD741x::performFunction(%x) delay %u\n", fI2CAddress, delayMS);

				break;

			case kCommandReadI2CSubAddr:

				if (param2 > READ_BUFFER_LEN)
				{
					IOLog("AppleAD741x::performFunction(%x) r-sub operation too big!\n", fI2CAddress);

					success = false;
					goto PERFORM_FUNCTION_EXIT;
				}

				// open the bus
				if (!openI2C())
				{
					IOLog("AppleAD741x::performFunction(%x) r-sub failed to open I2C\n", fI2CAddress);

					success = false;
					goto PERFORM_FUNCTION_EXIT;
				}
				else
				{
					// set the bus mode
					if (mode == kI2CUnspecifiedMode || mode == kI2CCombinedMode)
						setI2CCombinedMode();
					else if (mode == kI2CDumbMode)
						setI2CDumbMode();
					else if (mode == kI2CStandardMode)
						setI2CStandardMode();
					else if (mode == kI2CStandardSubMode)
						setI2CStandardSubMode();
	
					// do the read
					fI2CReadBufferSize = (UInt16) param2;
					success = readI2C( (UInt8) param1, fI2CReadBuffer, fI2CReadBufferSize );

					DLOG("AppleAD741x::performFunction(%x) r-sub %x len %x, got data", fI2CAddress, param1, param2);
#ifdef MAX6690_DEBUG
					char bufDump[256], byteDump[8];
					bufDump[0] = '\0';
					for (UInt32 i = 0; i < param2; i++)
					{
						sprintf(byteDump, " %02X", fI2CReadBuffer[i]);
						strcat(bufDump, byteDump);
					}

					DLOG("%s\n", bufDump);
#endif
					// close the bus
					closeI2C();

				}

				if (!success) goto PERFORM_FUNCTION_EXIT;

				break;

			case kCommandWriteI2CSubAddr:

				// open the bus
				if (!openI2C())
				{
					IOLog("AppleAD741x::performFunction(%x) w-sub failed to open I2C\n", fI2CAddress);

					success = false;
					goto PERFORM_FUNCTION_EXIT;
				}
				else
				{
					// set the bus mode
					if (mode == kI2CUnspecifiedMode || mode == kI2CStandardSubMode)
						setI2CStandardSubMode();
					else if (mode == kI2CDumbMode)
						setI2CDumbMode();
					else if (mode == kI2CStandardMode)
						setI2CStandardMode();
					else if (mode == kI2CCombinedMode)
						setI2CCombinedMode();
	
					DLOG("AppleAD741x::performFunction(%x) w-sub %x len %x data", fI2CAddress, param1, param2);
#ifdef MAX6690_DEBUG
					char bufDump[256], byteDump[8];
					bufDump[0] = '\0';
					for (UInt32 i = 0; i < param2; i++)
					{
						sprintf(byteDump, " %02X", ((UInt8 *)param3)[i]);
						strcat(bufDump, byteDump);
					}
					
					DLOG("%s\n", bufDump);
#endif
					// perform the write
					success = writeI2C( (UInt8) param1, (UInt8 *) param3, (UInt16) param2 );

					// close the bus
					closeI2C();
				}

				if (!success) goto PERFORM_FUNCTION_EXIT;

				break;

			case kCommandI2CMode:

				// the "mode" variable only persists for the duration of a single
				// function, because it is intended to be used as a part of a
				// command list.

				if ((param1 == kI2CDumbMode) ||
				    (param1 == kI2CStandardMode) ||
					(param1 == kI2CStandardSubMode) ||
					(param1 == kI2CCombinedMode))
					mode = (UInt8) param1;

				DLOG("AppleAD741x::performFunction(%x) mode %x\n", fI2CAddress, mode);
				break;

			case kCommandRMWI2CSubAddr:

				// check parameters
				if ((param2 > fI2CReadBufferSize) ||	// number of mask bytes
				    (param3 > fI2CReadBufferSize) ||	// number of value bytes
					(param4 > fI2CReadBufferSize) ||	// number of transfer bytes
					(param3 > param2))	// param3 is not actually used, we assume that
										// any byte that is masked also gets a value
										// OR'ed in
				{
					IOLog("AppleAD741x::performFunction(%x) invalid mw-sub cycle\n", fI2CAddress);

					success = false;
					goto PERFORM_FUNCTION_EXIT;
				}

				// set up buffers
				maskBytes = (UInt8 *) param5;
				valueBytes = (UInt8 *) param6;

				// apply mask and OR in value -- follow reference implementation in
				// AppleHWClock driver
				for (unsigned int index = 0; index < param2; index++)
				{
					scratchBuffer[index]  = (valueBytes[index] & maskBytes[index]);
					scratchBuffer[index] |= (fI2CReadBuffer[index] & ~maskBytes[index]);
				}

				// open the bus
				if (!openI2C())
				{
					IOLog("AppleAD741x::performFunction(%x) mw-sub failed to open I2C\n", fI2CAddress);

					success = false;
					goto PERFORM_FUNCTION_EXIT;
				}
				else
				{
					// set the bus mode
					if (mode == kI2CUnspecifiedMode || mode == kI2CCombinedMode)
						setI2CCombinedMode();
					else if (mode == kI2CDumbMode)
						setI2CDumbMode();
					else if (mode == kI2CStandardMode)
						setI2CStandardMode();
					else if (mode == kI2CStandardSubMode)
						setI2CStandardSubMode();
	
					DLOG("AppleAD741x::performFunction(%x) mw-sub %x len %x data", fI2CAddress, param1, param4);
#ifdef MAX6690_DEBUG
					char bufDump[256], byteDump[8];
					bufDump[0] = '\0';
					for (UInt32 i = 0; i < param4; i++)
					{
						sprintf(byteDump, " %02X", scratchBuffer[i]);
						strcat(bufDump, byteDump);
					}
					
					DLOG("%s\n", bufDump);
#endif
					// write out data
					success = writeI2C( (UInt8) param1, scratchBuffer, (UInt16) param4 );

					// close the bus
					closeI2C();
				}

				if (!success) goto PERFORM_FUNCTION_EXIT;

				break;

			default:
				DLOG ("AppleAD741x::performFunction - bad command %ld\n", cmd);

				success = false;
				goto PERFORM_FUNCTION_EXIT;
		}
	}

PERFORM_FUNCTION_EXIT:
	iter->release();

	DLOG ("AppleAD741x::performFunction - done - returning %s\n", success ? "TRUE" : "FALSE");
	return(success);
}

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn AppleAD741x::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 id = (UInt32)param1;
	UInt32 *value_buf = (UInt32 *)param2;
	SInt32 *temp_buf = (SInt32 *)param2;
	bool found = false;
	UInt8 i;

	DLOG("AppleAD741x::callPlatformFunction %s %s %08lx %08lx %08lx %08lx\n",
			functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
			(UInt32) param1, (UInt32) param2, (UInt32) param3, (UInt32) param4);

	if (functionName->isEqualTo(fGetSensorValueSym) == true)
	{
		if (fSleep == true)
			return(kIOReturnOffline);

		if (temp_buf == NULL)
			return(kIOReturnBadArgument);

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

OSArray *AppleAD741x::parseSensorParamsAndCreateNubs(IOService *nub)
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
		DLOG("AppleAD741x::parseSensorParamsAndCreateNubs no param version\n");
		return(NULL);
	}

	version = *((UInt32 *)tmp_osdata->getBytesNoCopy());

	// Get pointers inside the libkern containers for all properties
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorIDKey));
	if (tmp_osdata == NULL)
	{
		DLOG("AppleAD741x::parseSensorParamsAndCreateNubs no ids\n");
		return(NULL);
	}

	n_sensors = tmp_osdata->getLength() / sizeof(UInt32);

	// the AD7417 has one temp channel and four ADC channels.  If there are more
	// sensors than this indicated, something is wacky.
	if (n_sensors > 5)
	{
		DLOG("AppleAD741x::parseSensorParamsAndCreateNubs too many sensors %u\n", n_sensors);
		return(NULL);
	}

	id = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorZoneKey));
	if (tmp_osdata == NULL)
	{
		DLOG("AppleAD741x::parseSensorParamsAndCreateNubs no zones\n");
		return(NULL);
	}

	zone = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorTypeKey));
	if (tmp_osdata == NULL)
	{
		DLOG("AppleAD741x::parseSensorParamsAndCreateNubs no types\n");
		return(NULL);
	}

	type = (const char *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorLocationKey));
	if (tmp_osdata == NULL)
	{
		DLOG("AppleAD741x::parseSensorParamsAndCreateNubs no locations\n");
		return(NULL);
	}

	location = (const char *)tmp_osdata->getBytesNoCopy();

	// Polling Period key is not required
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorPollingPeriodKey));
	if (tmp_osdata != NULL)
	{
		polling_period = (UInt32 *)tmp_osdata->getBytesNoCopy();
		DLOG("AppleAD741x::parseSensorParamsAndCreateNubs polling period %lx\n", polling_period);
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
		DLOG("AppleAD741x::parseSensorParamsAndCreateNubs child nub %u\n", i);

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

IOReturn AppleAD741x::softReset( void )
{
	UInt8 val;

	/* According to the data sheet, software can simulate a reset by writing default
	   values to the config, config2, t_oti and t_hyst registers.  In practice, it
	   has been enough to only set config and config2, and this has the added
	   advantage of not clobbering the overtemp and hysteresis values if we have
	   to reset in the middle of operation. */

	if (!openI2C())
	{
 		DLOG("AppleAD741x::softReset failed to open bus\n");
		goto failNoClose;
	}

	// Set the config1 and config2 registers
	if (!setI2CStandardSubMode())
	{
		DLOG("AppleAD741x::softReset failed to set bus mode\n");
		goto failClose;
	}

	val = 0;
	if (!writeI2C( kConfig1Reg, &val, 1 ))
	{
		DLOG("AppleAD741x::softReset failed to write cfg1 reg\n");
		goto failClose;
	}

	val = 0;
	if (!writeI2C( kConfig2Reg, &val, 1 ))
	{
		DLOG("AppleAD741x::softReset failed to write cfg2 reg\n");
		goto failClose;
	}

	// Read back the config1 register (read operations clear fault conditions)
	if (!setI2CCombinedMode())
	{
		DLOG("AppleAD741x::softReset failed to set bus mode\n");
		goto failClose;
	}

	// don't care what the value is, just want to make sure the read succeeds
	if (!readI2C( kConfig1Reg, &val, 1 ))
	{
		DLOG("AppleAD741x::softReset failed to read back cfg1 reg\n");
		goto failClose;
	}

	closeI2C();

	return(kIOReturnSuccess);

failClose:
	closeI2C();

failNoClose:
	return(kIOReturnError);
}

#pragma mark -
#pragma mark *** Read Sensor Channels ***
#pragma mark -

/*******************************************************************************
 * Read sensor channels from the device
 *******************************************************************************/

IOReturn AppleAD741x::getTemperature( SInt32 * temp )
{
	UInt8 bytes[2];
	SInt16 reading;
	bool success = false;

	// Open the bus - this grabs a mutex in the I2C driver so it's thread-safe
	if (!openI2C())
	{
		DLOG("AppleAD741x::getTemperature error opening bus!\n");
	}
	else
	{
		do
		{
			// reads should be performed in combined mode
			if (!setI2CCombinedMode())
			{
				DLOG("AppleAD741x::getTemperature failed to set bus mode!\n");
				break;
			}

			if (!readI2C( kTempValueReg, bytes, 2 ))
			{
				DLOG("AppleAD741x::getTemperature read temp failed!\n");
				break;
			}

			success = true;

		} while (false);

		closeI2C();
	}

	if (success)
	{
		DLOG("AppleAD741x::getTemperature got bytes 0x%02X 0x%02X\n", bytes[0], bytes[1]);

		reading = *((SInt16 *) bytes);
		// temperature is fixed point 8.2 in most significant 10 bits of the two bytes that were read
		*temp = ( ( ( SInt16 ) ( reading & 0xFFC0 ) ) << 8 );
		return(kIOReturnSuccess);
	}
	else
	{
		*temp = -1;
		return(kIOReturnError);
	}
}

IOReturn AppleAD741x::getADC( UInt8 channel, UInt32 * sample )
{
	UInt8 cfg1, tmpByte, bytes[2];
	UInt16 rawSample;
	bool success = false;

	if (channel < kAD1Channel || channel > kAD4Channel)
	{
		DLOG("AppleAD741x::getADC invalid channel\n");
		return(kIOReturnBadArgument);
	}

	DLOG("AppleAD741x::getADC reading channel %u\n", channel);

	// Open the bus - this grabs a mutex in the I2C driver so it's thread-safe
	if (!openI2C())
	{
		DLOG("AppleAD741x::getADC error opening bus!\n");
	}
	else
	{
		do
		{
			// reads should be performed in combined mode
			if (!setI2CCombinedMode())
			{
				DLOG("AppleAD741x::getADC failed to set bus mode for read!\n");
				break;
			}

			if (!readI2C( kConfig1Reg, &cfg1, 1 ))
			{
				DLOG("AppleAD741x::getADC read cfg1 failed!\n");
				break;
			}

			// set the channel selection bits
			tmpByte = channel << kCfg1ChannelShift;

			DLOG("AppleAD741x::getADC read cfg1 0x%02X\n", cfg1);

			cfg1 = (cfg1 & ~kCfg1ChannelMask) | (tmpByte & kCfg1ChannelMask);

			DLOG("AppleAD741x::getADC new cfg1 0x%02X\n", cfg1);
	
			// write it back out to the cfg register
			if (!setI2CStandardSubMode())
			{
				DLOG("AppleAD741x::getADC failed to set bus mode for write!\n");
				break;
			}

			if (!writeI2C( kConfig1Reg, &cfg1, 1 ))
			{
				DLOG("AppleAD741x::getADC write cfg1 failed!\n");
				break;
			}

			// now read the adc register to get the sample
			if (!setI2CCombinedMode())
			{
				DLOG("AppleAD741x::getADC failed to set bus mode for read!\n");
				break;
			}

			if (!readI2C( kADCReg, bytes, 2 ))
			{
				DLOG("AppleAD741x::getADC read adc reg failed!\n");
				break;
			}

			success = true;

		} while (false);

		closeI2C();
	}

	if (success)
	{
		DLOG("AppleAD741x::getADC got bytes 0x%02X 0x%02X\n", bytes[0], bytes[1]);

		rawSample = *((UInt16 *) bytes);
		*sample = ((UInt32)rawSample) >> 6;	// shift out bits [5:0] which are unused
		return(kIOReturnSuccess);
	}
	else
	{
		*sample = 0xFFFFFFFF;
		return(kIOReturnError);
	}
}

#pragma mark -
#pragma mark *** Power Management ***
#pragma mark -

/*******************************************************************************
 * Power Management callbacks and handlers
 *******************************************************************************/

IOReturn AppleAD741x::powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*)
{	
    if ( ! (theFlags & IOPMPowerOn) ) {
        // Sleep sequence:
		DLOG("AppleAD741x::powerStateWillChangeTo - sleep\n");
		doSleep();
   } else {
        // Wake sequence:
		DLOG("AppleAD741x::powerStateWillChangeTo - wake\n");
		doWake();
    }
	
    return IOPMAckImplied;
}

void AppleAD741x::doSleep(void)
{
	UInt32 flags, i;
	IOPlatformFunction *func;

	fSleep = true;

	// Execute any functions flagged as "on sleep"
	for (i = 0; i < fPlatformFuncArray->getCount(); i++)
	{
		if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
		{
			flags = func->getCommandFlags();

			if (flags & kIOPFFlagOnSleep) 
				performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
		}
	}
}

void AppleAD741x::doWake(void)
{
	UInt32 flags, i;
	IOPlatformFunction *func;

	// Execute any functions flagged as "on wake"
	for (i = 0; i < fPlatformFuncArray->getCount(); i++)
	{
		if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
		{
			flags = func->getCommandFlags();

			if (flags & kIOPFFlagOnWake) 
				performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
		}
	}

	fSleep = false;
}

#pragma mark -
#pragma mark *** I2C Helpers ***
#pragma mark -

/*******************************************************************************
 * I2C Helpers
 *******************************************************************************/

bool AppleAD741x::openI2C( void )
{
	UInt32 passBus;
	IOReturn status;

	DLOG("AppleAD741x::openI2C - entered\n");

	if (fI2C_iface == NULL)
		return false;

	// Open the interface
	passBus = (UInt32) fI2CBus;	// cast from 8-bit to machine long word length
	if ((status = (fI2C_iface->callPlatformFunction(kOpenI2Cbus, false, (void *) passBus, NULL, NULL, NULL)))
			!= kIOReturnSuccess)
	{
		DLOG("AppleAD741x::openI2C failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

void AppleAD741x::closeI2C( void )
{
	IOReturn status;

	DLOG("AppleAD741x::closeI2C - entered\n");

	if (fI2C_iface == NULL) return;

	if ((status = (fI2C_iface->callPlatformFunction(kCloseI2Cbus, false, NULL, NULL, NULL, NULL)))
			!= kIOReturnSuccess)
	{
		DLOG("AppleAD741x::closeI2C failed, status = %08lx\n", (UInt32) status);
	}
}

bool AppleAD741x::setI2CDumbMode( void )
{
	IOReturn status;

	DLOG("AppleAD741x::setI2CDumbMode - entered\n");

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetDumbMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		DLOG("AppleAD741x::setI2CDumbMode failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleAD741x::setI2CStandardMode( void )
{
	IOReturn status;

	DLOG("AppleAD741x::setI2CStandardMode - entered\n");

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetStandardMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		DLOG("AppleAD741x::setI2CStandardMode failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleAD741x::setI2CStandardSubMode( void )
{
	IOReturn status;

	DLOG("AppleAD741x::setI2CStandardSubMode - entered\n");

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetStandardSubMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		DLOG("AppleAD741x::setI2CStandardSubMode failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleAD741x::setI2CCombinedMode( void )
{
	IOReturn status;

	DLOG("AppleAD741x::setI2CCombinedMode - entered\n");

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetCombinedMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		DLOG("AppleAD741x::setI2CCombinedMode failed, status = %08lx\n", (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleAD741x::writeI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
	UInt32 passAddr, passSubAddr, passSize;
	unsigned int retries = 0;
	IOReturn status;

	DLOG("AppleAD741x::writeI2C - entered\n");

	if (fI2C_iface == NULL || data == NULL || size == 0)
		return false;

	passAddr = (UInt32) (fI2CAddress >> 1);
	passSubAddr = (UInt32) subAddr;
	passSize = (UInt32) size;

	while ((retries < kNumRetries) &&
	       ((status = (fI2C_iface->callPlatformFunction(kWriteI2Cbus, false, (void *) passAddr,
		    (void *) passSubAddr, (void *) data, (void *) passSize))) != kIOReturnSuccess))
	{
		DLOG("AppleAD741x::writeI2C transaction failed, status = %08lx\n", (UInt32) status);
		retries++;
	}

	if (retries >= kNumRetries)
		return(false);
	else
		return(true);
}

bool AppleAD741x::readI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
	UInt32 passAddr, passSubAddr, passSize;
	unsigned int retries = 0;
	IOReturn status;

	DLOG("AppleAD741x::readI2C \n");

	if (fI2C_iface == NULL || data == NULL || size == 0)
		return false;

	passAddr = (UInt32) (fI2CAddress >> 1);
	passSubAddr = (UInt32) subAddr;
	passSize = (UInt32) size;

	while ((retries < kNumRetries) &&
	       ((status = (fI2C_iface->callPlatformFunction(kReadI2Cbus, false, (void *) passAddr,
		    (void *) passSubAddr, (void *) data, (void *) passSize))) != kIOReturnSuccess))
	{
		DLOG("AppleAD741x::readI2C transaction failed, status = %08lx\n", (UInt32) status);
		retries++;
	}

	if (retries >= kNumRetries)
		return(false);
	else
		return(true);
}
