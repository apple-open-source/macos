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
 *  File: $Id: AppleMaxim6690.cpp,v 1.6 2004/01/30 23:52:00 eem Exp $
 *
 *  DRI: Eric Muehlhausen
 *
 *		$Log: AppleMaxim6690.cpp,v $
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

#include "AppleMaxim6690.h"

#define super IOService
OSDefineMetaClassAndStructors(AppleMaxim6690, IOService)

bool AppleMaxim6690::init( OSDictionary *dict )
{
	if (!super::init(dict)) return(false);

	fGetSensorValueSym = NULL;
	pmRootDomain = NULL;
	fPlatformFuncArray = NULL;
	fI2CReadBufferSize = 0;

	fSleep = false;

	return(true);
}

void AppleMaxim6690::free( void )
{
	super::free();
}

bool AppleMaxim6690::start( IOService *nub )
{
	IOService		*parentDev, *childNub;
	UInt32			fullAddr;
	OSData *		regprop;
	OSArray *		nubArray;
	const char *	drvName;
	const OSSymbol *instantiatePFuncs = OSSymbol::withCString("InstantiatePlatformFunctions");
	IOReturn		retval;

	DLOG("AppleMaxim6690::start - entered\n");

	if (!super::start(nub)) return(false);

	if (!fGetSensorValueSym)
		fGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	// Extract bus number and address from reg property
	if ((regprop = OSDynamicCast(OSData, nub->getProperty("reg"))) == NULL)
	{
		IOLog("AppleMaxim6690::start no reg property!\n");
		return(false);
	}
	else
	{
		fullAddr = *((UInt32 *)regprop->getBytesNoCopy());
		fI2CBus = (UInt8)((fullAddr >> 8) & 0x000000ff);
		fI2CAddress = (UInt8)(fullAddr & 0x000000fe);
		
		DLOG("AppleMaxim6690::start fI2CBus = %02x fI2CAddress = %02x\n",
				fI2CBus, fI2CAddress);
	}

	// Find the i2c driver
	if ((parentDev = OSDynamicCast(IOService, nub->getParentEntry(gIODTPlane))) != NULL)
	{
		DLOG("AppleMaxim6690::start got parentDev %s\n", parentDev->getName());
	}
	else
	{
		DLOG("AppleMaxim6690::start failed to get parentDev\n");
		return(false);
	}

	drvName = parentDev->getName();
	if (strcmp(drvName, "i2c") != 0)
	{
		IOLog("AppleMaxim6690::start warning: unexpected i2c device name %s\n", drvName);
	}

	if ((fI2C_iface = OSDynamicCast(IOService, parentDev->getChildEntry(gIOServicePlane))) != NULL &&
	    (fI2C_iface->metaCast("PPCI2CInterface") != NULL))
	{
		DLOG("AppleMaxim6690::start got fI2C_iface %s\n", fI2C_iface->getName());
	}
	else
	{
		DLOG("AppleMaxim6690::start failed to get fI2C_iface\n");
		return(false);
	}

	// sanity check on device communication - read the device ID register
	if (!openI2C())
	{
		IOLog("AppleMaxim6690::start failed to open I2C bus!\n");
		return(false);
	}
	else
	{
		bool success = false;
		UInt8 deviceID;

		do {

			if (!setI2CCombinedMode())
			{
				IOLog("AppleMaxim6690::start failed to set bus mode!\n");
				break;
			}

			if (!readI2C(kReadDeviceID, &deviceID, 1))
			{
				IOLog("AppleMaxim6690::start device at bus %x addr %x not responding!\n",
						fI2CBus, fI2CAddress);
				break;
			}

			success = true;

		} while (0);

		closeI2C();

		if (!success)
			return(false);
	}

	// parse thermal sensor properties and create nubs
	nubArray = parseSensorParamsAndCreateNubs( nub );
	if (nubArray == NULL || nubArray->getCount() == 0)
	{
		IOLog("AppleMaxim6690::start no thermal sensors found\n");
		if (nubArray) nubArray->release();
		return(false);
	}

	// Scan for platform-do-xxx functions
	fPlatformFuncArray = NULL;

	DLOG("AppleMaxim6690::start(%x) calling InstantiatePlatformFunctions\n", fI2CAddress);

	retval = nub->getPlatform()->callPlatformFunction(instantiatePFuncs, false,
			(void *)nub, (void *)&fPlatformFuncArray, (void *)0, (void *)0);
	instantiatePFuncs->release();

	DLOG("AppleMaxim6690::start(%x) InstantiatePlatformFunctions returned %ld, pfArray %sNULL\n",
			fI2CAddress, retval, fPlatformFuncArray ? "NOT " : "");

	if (retval == kIOReturnSuccess && (fPlatformFuncArray != NULL))
	{
		int i, funcCount;
		UInt32 flags;
		IOPlatformFunction *func;
		bool doSleepWake = false;

		funcCount = fPlatformFuncArray->getCount();

		DLOG("AppleMaxim6690::start(%x) iterating platformFunc array, count = %ld\n",
			fI2CAddress, funcCount);

		for (i = 0; i < funcCount; i++)
		{
			if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
			{
				flags = func->getCommandFlags();

				DLOG("AppleMaxim6690::start(%x) got function, flags 0x%08lx, pHandle 0x%08lx\n", 
					fI2CAddress, flags, func->getCommandPHandle());

				// If this function is flagged to be performed at initialization, do it
				if (flags & kIOPFFlagOnInit)
				{
					bool funcResult = performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
					DLOG("AppleMaxim6690::start(%x) startup function %s\n", fI2CAddress,
							funcResult ? "SUCCEEDED" : "FAILED");
				}

				// If we need to do anything at sleep/wake time, we'll need to set this
				// flag so we know to register for notifications
				if (flags & (kIOPFFlagOnSleep | kIOPFFlagOnWake))
					doSleepWake = true;
			}
			else
			{
				// This function won't be used -- generate a warning
				DLOG("AppleMaxim6690::start(%x) not an IOPlatformFunction object\n",
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
				DLOG("AppleMaxim6690::start(%x) to acknowledge power changes\n", fI2CAddress);
				pmRootDomain->registerInterestedDriver(this);
			}
			else
			{
				IOLog("AppleMaxim6690::start(%x) failed to register PM interest\n",
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

void AppleMaxim6690::stop( IOService *nub )
{
	UInt32 flags, i;
	IOPlatformFunction *func;

	DLOG("AppleMaxim6690::stop - entered\n");

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

bool AppleMaxim6690::performFunction(IOPlatformFunction *func, void *pfParam1 = 0,
			void *pfParam2 = 0, void *pfParam3 = 0, void *pfParam4 = 0)
{
	bool						success = true;		// initialize to success
	IOPlatformFunctionIterator 	*iter;
	UInt8						scratchBuffer[READ_BUFFER_LEN], mode = kI2CUnspecifiedMode;
	UInt8						*maskBytes, *valueBytes;
	unsigned					delayMS;
	UInt32 						cmd, cmdLen, result, param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;

	DLOG ("AppleMaxim6690::performFunction(%x) - entered\n", fI2CAddress);

	if (!func)
		return false;
	
	if (!(iter = func->getCommandIterator()))
		return false;
	
	while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, &result)) {
		if (result != kIOPFNoError) {
			DLOG("AppleMaxim6690::performFunction(%x) error parsing platform function\n", fI2CAddress);

			success = false;
			goto PERFORM_FUNCTION_EXIT;
		}

		DLOG ("AppleMaxim6690::performFunction(%x) - 1)0x%lx, 2)0x%lx, 3)0x%lx, 4)0x%lx, 5)0x%lx,"
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

				DLOG("AppleMaxim6690::performFunction(%x) delay %u\n", fI2CAddress, delayMS);

				break;

			case kCommandReadI2CSubAddr:

				if (param2 > READ_BUFFER_LEN)
				{
					IOLog("AppleMaxim6690::performFunction(%x) r-sub operation too big!\n", fI2CAddress);

					success = false;
					goto PERFORM_FUNCTION_EXIT;
				}

				// open the bus
				if (!openI2C())
				{
					IOLog("AppleMaxim6690::performFunction(%x) r-sub failed to open I2C\n", fI2CAddress);

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

					DLOG("AppleMaxim6690::performFunction(%x) r-sub %x len %x, got data", fI2CAddress, param1, param2);
#ifdef MAX6690_DEBUG
					char bufDump[256], byteDump[8];
					bufDump[0] = '\0';
					for (UInt32 i = 0; i < param2; i++)
					{
						sprintf(byteDump, " %02X", fI2CReadBuffer[i]);
						strcat(bufDump, byteDump);
					}
#endif
					DLOG("%s\n", bufDump);

					// close the bus
					closeI2C();

				}

				if (!success) goto PERFORM_FUNCTION_EXIT;

				break;

			case kCommandWriteI2CSubAddr:

				// open the bus
				if (!openI2C())
				{
					IOLog("AppleMaxim6690::performFunction(%x) w-sub failed to open I2C\n", fI2CAddress);

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
	
					DLOG("AppleMaxim6690::performFunction(%x) w-sub %x len %x data", fI2CAddress, param1, param2);
#ifdef MAX6690_DEBUG
					char bufDump[256], byteDump[8];
					bufDump[0] = '\0';
					for (UInt32 i = 0; i < param2; i++)
					{
						sprintf(byteDump, " %02X", ((UInt8 *)param3)[i]);
						strcat(bufDump, byteDump);
					}
#endif
					DLOG("%s\n", bufDump);
	
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

				DLOG("AppleMaxim6690::performFunction(%x) mode %x\n", fI2CAddress, mode);
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
					IOLog("AppleMaxim6690::performFunction(%x) invalid mw-sub cycle\n", fI2CAddress);

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
					IOLog("AppleMaxim6690::performFunction(%x) mw-sub failed to open I2C\n", fI2CAddress);

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
	
					DLOG("AppleMaxim6690::performFunction(%x) mw-sub %x len %x data", fI2CAddress, param1, param4);
#ifdef MAX6690_DEBUG
					char bufDump[256], byteDump[8];
					bufDump[0] = '\0';
					for (UInt32 i = 0; i < param4; i++)
					{
						sprintf(byteDump, " %02X", scratchBuffer[i]);
						strcat(bufDump, byteDump);
					}
#endif
					DLOG("%s\n", bufDump);

					// write out data
					success = writeI2C( (UInt8) param1, scratchBuffer, (UInt16) param4 );

					// close the bus
					closeI2C();
				}

				if (!success) goto PERFORM_FUNCTION_EXIT;

				break;

			default:
				DLOG ("AppleMaxim6690::performFunction - bad command %ld\n", cmd);

				success = false;
				goto PERFORM_FUNCTION_EXIT;
		}
	}

PERFORM_FUNCTION_EXIT:
	iter->release();

	DLOG ("AppleMaxim6690::performFunction - done - returning %s\n", success ? "TRUE" : "FALSE");
	return(success);
}

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn AppleMaxim6690::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 id = (UInt32)param1;
	SInt32 *temp_buf = (SInt32 *)param2;

	DLOG("AppleMaxim6690::callPlatformFunction(%x) %s %s %08lx %08lx %08lx %08lx\n",
			fI2CAddress, functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
			(UInt32) param1, (UInt32) param2, (UInt32) param3, (UInt32) param4);

	if (functionName->isEqualTo(fGetSensorValueSym) == true)
	{
		if (fSleep == true)
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

OSArray *AppleMaxim6690::parseSensorParamsAndCreateNubs(IOService *nub)
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
		DLOG("AppleMaxim6690::parseSensorParamsAndCreateNubs no param version\n");
		return(NULL);
	}

	version = *((UInt32 *)tmp_osdata->getBytesNoCopy());

	// Get pointers inside the libkern containers for all properties
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorIDKey));
	if (tmp_osdata == NULL)
	{
		DLOG("AppleMaxim6690::parseSensorParamsAndCreateNubs no ids\n");
		return(NULL);
	}

	n_sensors = tmp_osdata->getLength() / sizeof(UInt32);

	// the Maxim 6690 has only two temperature channels.  If there are more
	// sensors than this indicated, something is wacky.
	if (n_sensors > 2)
	{
		DLOG("AppleMaxim6690::parseSensorParamsAndCreateNubs too many sensors %u\n", n_sensors);
		return(NULL);
	}

	id = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorZoneKey));
	if (tmp_osdata == NULL)
	{
		DLOG("AppleMaxim6690::parseSensorParamsAndCreateNubs no zones\n");
		return(NULL);
	}

	zone = (UInt32 *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorTypeKey));
	if (tmp_osdata == NULL)
	{
		DLOG("AppleMaxim6690::parseSensorParamsAndCreateNubs no types\n");
		return(NULL);
	}

	type = (const char *)tmp_osdata->getBytesNoCopy();

	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorLocationKey));
	if (tmp_osdata == NULL)
	{
		DLOG("AppleMaxim6690::parseSensorParamsAndCreateNubs no locations\n");
		return(NULL);
	}

	location = (const char *)tmp_osdata->getBytesNoCopy();

	// Polling Period key is not required
	tmp_osdata = OSDynamicCast(OSData,
			nub->getProperty(kDTSensorPollingPeriodKey));
	if (tmp_osdata != NULL)
	{
		polling_period = (UInt32 *)tmp_osdata->getBytesNoCopy();
		DLOG("AppleMaxim6690::parseSensorParamsAndCreateNubs polling period %lu\n", polling_period);
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

IOReturn AppleMaxim6690::getInternalTemp( SInt32 * temp )
{
//	UInt8				status;
	UInt8				integer;
	UInt8				fraction;

	// Open the bus - this grabs a mutex in the I2C driver so it's thread-safe
	if (!openI2C())
	{
		IOLog("AppleMaxim6690::getInternalTemp error opening bus!\n");
		goto failNoClose;
	}

	// reads should be performed in combined mode
	if (!setI2CCombinedMode())
	{
		IOLog("AppleMaxim6690::getInternalTemp failed to set bus mode!\n");
		goto failClose;
	}

/*
	// wait for the conversion to finish
	// bit 7 of the status register is the busy bit
	do {

		DLOG("AppleMaxim6690::getInternalTemp(%x) checking busy bit...\n", fI2CAddress);

		if (!readI2C( kReadStatusByte, &status, 1 ))
		{
			IOLog("AppleMaxim6690::getInternalTemp error reading status byte!\n");
			goto failClose;
		}

	} while (status & 0x80);
*/

	// get the internal temperature register
	if (!readI2C( kReadInternalTemp, &integer, 1 ))
	{
		IOLog("AppleMaxim6690::getInternalTemp read temp failed!\n");
		goto failClose;
	}

	// get the internal extended temperature register
	if (!readI2C( kReadInternalExtTemp, &fraction, 1 ))
	{
		IOLog("AppleMaxim6690::getInternalTemp read ext temp failed!\n");
		goto failClose;
	}

	// close the bus
	closeI2C();

	// format the 16.16 fixed point temperature and return it
	*temp = ((integer << 16) & 0x00FF0000) | ((fraction << 8) & 0x0000E000);
	return(kIOReturnSuccess);

failClose:
	closeI2C();

failNoClose:
	*temp = -1;
	return(kIOReturnError);
}

IOReturn AppleMaxim6690::getExternalTemp( SInt32 * temp )
{
//	UInt8			status;
	UInt8			integer;
	UInt8			fraction;

	// Open the bus - this grabs a mutex in the I2C driver so it's thread-safe
	if (!openI2C())
	{
		IOLog("AppleMaxim6690::getExternalTemp error opening bus!\n");
		goto failNoClose;
	}

	// reads should be performed in combined mode
	if (!setI2CCombinedMode())
	{
		IOLog("AppleMaxim6690::getExternalTemp failed to set bus mode!\n");
		goto failClose;
	}

/*
	// wait for the conversion to finish
	// bit 7 of the status register is the busy bit
	do {

		DLOG("AppleMaxim6690::getExternalTemp(%x) checking busy bit...\n", fI2CAddress);

		if (!readI2C( kReadStatusByte, &status, 1 ))
		{
			IOLog("AppleMaxim6690::getExternalTemp error reading status byte!\n");
			goto failClose;
		}

	} while (status & 0x80);
*/

	// get the internal temperature register
	if (!readI2C( kReadExternalTemp, &integer, 1 ))
	{
		IOLog("AppleMaxim6690::getExternalTemp read temp failed!\n");
		goto failClose;
	}

	// get the internal extended temperature register
	if (!readI2C( kReadExternalExtTemp, &fraction, 1 ))
	{
		IOLog("AppleMaxim6690::getExternalTemp read ext temp failed!\n");
		goto failClose;
	}

	// close the bus
	closeI2C();

	// format the 16.16 fixed point temperature and return it
	*temp = ((integer << 16) & 0x00FF0000) | ((fraction << 8) & 0x0000E000);
	return(kIOReturnSuccess);

failClose:
	closeI2C();

failNoClose:
	*temp = -1;
	return(kIOReturnError);
}

#pragma mark -
#pragma mark *** Power Management ***
#pragma mark -

/*******************************************************************************
 * Power Management callbacks and handlers
 *******************************************************************************/

IOReturn AppleMaxim6690::powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*)
{	
    if ( ! (theFlags & IOPMPowerOn) ) {
        // Sleep sequence:
		DLOG("AppleMaxim6690::powerStateWillChangeTo - sleep\n");
		doSleep();
   } else {
        // Wake sequence:
		DLOG("AppleMaxim6690::powerStateWillChangeTo - wake\n");
		doWake();
    }
	
    return IOPMAckImplied;
}

void AppleMaxim6690::doSleep(void)
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

void AppleMaxim6690::doWake(void)
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

bool AppleMaxim6690::openI2C( void )
{
	UInt32 passBus;
	IOReturn status;

	DLOG("AppleMaxim6690::openI2C(%x) - entered\n", fI2CAddress);

	if (fI2C_iface == NULL)
		return false;

	// Open the interface
	passBus = (UInt32) fI2CBus;	// cast from 8-bit to machine long word length
	if ((status = (fI2C_iface->callPlatformFunction(kOpenI2Cbus, false, (void *) passBus, NULL, NULL, NULL)))
			!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim6690::openI2C(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

void AppleMaxim6690::closeI2C( void )
{
	IOReturn status;

	DLOG("AppleMaxim6690::closeI2C(%x) - entered\n", fI2CAddress);

	if (fI2C_iface == NULL) return;

	if ((status = (fI2C_iface->callPlatformFunction(kCloseI2Cbus, false, NULL, NULL, NULL, NULL)))
			!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim6690::closeI2C(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
	}
}

bool AppleMaxim6690::setI2CDumbMode( void )
{
	IOReturn status = kIOReturnSuccess;

	DLOG("AppleMaxim6690::setI2CDumbMode(%x) - entered\n", fI2CAddress);

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetDumbMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim6690::setI2CDumbMode(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleMaxim6690::setI2CStandardMode( void )
{
	IOReturn status = kIOReturnSuccess;

	DLOG("AppleMaxim6690::setI2CStandardMode(%x) - entered\n", fI2CAddress);

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetStandardMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim6690::setI2CStandardMode(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleMaxim6690::setI2CStandardSubMode( void )
{
	IOReturn status = kIOReturnSuccess;

	DLOG("AppleMaxim6690::setI2CStandardSubMode(%x) - entered\n", fI2CAddress);

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetStandardSubMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim6690::setI2CStandardSubMode(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleMaxim6690::setI2CCombinedMode( void )
{
	IOReturn status = kIOReturnSuccess;

	DLOG("AppleMaxim6690::setI2CCombinedMode(%x) - entered\n", fI2CAddress);

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetCombinedMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim6690::setI2CCombinedMode(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleMaxim6690::writeI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
	UInt32 passAddr, passSubAddr, passSize;
	unsigned int retries = 0;
	IOReturn status;

	DLOG("AppleMaxim6690::writeI2C(%x) - entered\n", fI2CAddress);

	if (fI2C_iface == NULL || data == NULL || size == 0)
		return false;

	passAddr = (UInt32) (fI2CAddress >> 1);
	passSubAddr = (UInt32) subAddr;
	passSize = (UInt32) size;

	while ((retries < kNumRetries) &&
	       ((status = (fI2C_iface->callPlatformFunction(kWriteI2Cbus, false, (void *) passAddr,
		    (void *) passSubAddr, (void *) data, (void *) passSize))) != kIOReturnSuccess))
	{
		IOLog("AppleMaxim6690::writeI2C(%x) transaction failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		retries++;
	}

	if (retries >= kNumRetries)
		return(false);
	else
		return(true);
}

bool AppleMaxim6690::readI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
	UInt32 passAddr, passSubAddr, passSize;
	unsigned int retries = 0;
	IOReturn status;

	DLOG("AppleMaxim6690::readI2C(%x) \n", fI2CAddress);

	if (fI2C_iface == NULL || data == NULL || size == 0)
		return false;

	passAddr = (UInt32) (fI2CAddress >> 1);
	passSubAddr = (UInt32) subAddr;
	passSize = (UInt32) size;

	while ((retries < kNumRetries) &&
	       ((status = (fI2C_iface->callPlatformFunction(kReadI2Cbus, false, (void *) passAddr,
		    (void *) passSubAddr, (void *) data, (void *) passSize))) != kIOReturnSuccess))
	{
		IOLog("AppleMaxim6690::readI2C error (0x%08X) I2CAddr = 0x%02X, subAddr = 0x%02X\n", (unsigned int) status, fI2CAddress, subAddr);
		retries++;
	}

	if (retries >= kNumRetries)
		return(false);
	else
		return(true);
}
