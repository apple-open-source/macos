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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *		
 */

#include "AppleMaxim1989.h"

#define super IOService
OSDefineMetaClassAndStructors(AppleMaxim1989, IOService)

bool AppleMaxim1989::init( OSDictionary *dict )
{
	if (!super::init(dict)) return(false);

	fGetSensorValueSym = NULL;
	pmRootDomain = NULL;
	fPlatformFuncArray = NULL;
	fI2CReadBufferSize = 0;

	fSleep = false;

	return(true);
}

void AppleMaxim1989::free( void )
{
	super::free();
}

bool AppleMaxim1989::start( IOService *nub )
{
	IOService		*parentDev;
	UInt32			fullAddr;
	OSData *		regprop;
	const char *	drvName;
	const OSSymbol *instantiatePFuncs = OSSymbol::withCString("InstantiatePlatformFunctions");
	IOReturn		retval;

	DLOG("AppleMaxim1989::start - entered\n");

	if (!super::start(nub)) return(false);

	if (!fGetSensorValueSym)
		fGetSensorValueSym = OSSymbol::withCString("getSensorValue");

	// Extract bus number and address from reg property
	if ((regprop = OSDynamicCast(OSData, nub->getProperty("reg"))) == NULL)
	{
		IOLog("AppleMaxim1989::start no reg property!\n");
		return(false);
	}
	else
	{
		fullAddr = *((UInt32 *)regprop->getBytesNoCopy());
		fI2CBus = (UInt8)((fullAddr >> 8) & 0x000000ff);
		fI2CAddress = (UInt8)(fullAddr & 0x000000fe);
		
		DLOG("AppleMaxim1989::start fI2CBus = %02x fI2CAddress = %02x\n",
				fI2CBus, fI2CAddress);
	}

	// Find the i2c driver
	if ((parentDev = OSDynamicCast(IOService, nub->getParentEntry(gIODTPlane))) != NULL)
	{
		DLOG("AppleMaxim1989::start got parentDev %s\n", parentDev->getName());
	}
	else
	{
		DLOG("AppleMaxim1989::start failed to get parentDev\n");
		return(false);
	}

	drvName = parentDev->getName();
	if (strcmp(drvName, "i2c") != 0)
	{
		IOLog("AppleMaxim1989::start warning: unexpected i2c device name %s\n", drvName);
	}

	if ((fI2C_iface = OSDynamicCast(IOService, parentDev->getChildEntry(gIOServicePlane))) != NULL &&
	    (fI2C_iface->metaCast("PPCI2CInterface") != NULL))
	{
		DLOG("AppleMaxim1989::start got fI2C_iface %s\n", fI2C_iface->getName());
	}
	else
	{
		DLOG("AppleMaxim1989::start failed to get fI2C_iface\n");
		return(false);
	}

	// sanity check on device communication - read the device ID register
	if (!openI2C())
	{
		IOLog("AppleMaxim1989::start failed to open I2C bus!\n");
		return(false);
	}
	else
	{
		bool success = false;
		UInt8 deviceID;

		do {

			if (!setI2CCombinedMode())
			{
				IOLog("AppleMaxim1989::start failed to set bus mode!\n");
				break;
			}

			if (!readI2C(kReadDeviceID, &deviceID, 1))
			{
				IOLog("AppleMaxim1989::start device at bus %x addr %x not responding!\n",
						fI2CBus, fI2CAddress);
				break;
			}

			success = true;

		} while (0);

		closeI2C();

		if (!success)
			return(false);
	}


	// Scan for platform-do-xxx functions
	fPlatformFuncArray = NULL;

	DLOG("AppleMaxim1989::start(%x) calling InstantiatePlatformFunctions\n", fI2CAddress);

	retval = nub->getPlatform()->callPlatformFunction(instantiatePFuncs, false,
			(void *)nub, (void *)&fPlatformFuncArray, (void *)0, (void *)0);
	instantiatePFuncs->release();

	DLOG("AppleMaxim1989::start(%x) InstantiatePlatformFunctions returned %ld, pfArray %sNULL\n",
			fI2CAddress, retval, fPlatformFuncArray ? "NOT " : "");

	if (retval == kIOReturnSuccess && (fPlatformFuncArray != NULL))
	{
		int i, funcCount;
		UInt32 flags;
		IOPlatformFunction *func;
		bool doSleepWake = false;

		funcCount = fPlatformFuncArray->getCount();

		DLOG("AppleMaxim1989::start(%x) iterating platformFunc array, count = %ld\n",
			fI2CAddress, funcCount);

		for (i = 0; i < funcCount; i++)
		{
			if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
			{
				flags = func->getCommandFlags();

				DLOG("AppleMaxim1989::start(%x) got function, flags 0x%08lx, pHandle 0x%08lx\n", 
					fI2CAddress, flags, func->getCommandPHandle());

				// If this function is flagged to be performed at initialization, do it
				if (flags & kIOPFFlagOnInit)
				{
					bool funcResult = performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
					DLOG("AppleMaxim1989::start(%x) startup function %s\n", fI2CAddress,
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
				DLOG("AppleMaxim1989::start(%x) not an IOPlatformFunction object\n",
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
				DLOG("AppleMaxim1989::start(%x) to acknowledge power changes\n", fI2CAddress);
				pmRootDomain->registerInterestedDriver(this);
			}
			else
			{
				IOLog("AppleMaxim1989::start(%x) failed to register PM interest\n",
						fI2CAddress);
			}
		}
	}

	// tell the world i'm here
	registerService();

	// Publish any child nubs under the max1989 node...
	publishChildren(nub);

	return(true);
}

void AppleMaxim1989::stop( IOService *nub )
{
	UInt32 flags, i;
	IOPlatformFunction *func;

	DLOG("AppleMaxim1989::stop - entered\n");

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

bool AppleMaxim1989::performFunction(IOPlatformFunction *func, void *pfParam1 = 0,
			void *pfParam2 = 0, void *pfParam3 = 0, void *pfParam4 = 0)
{
	bool						success = true;		// initialize to success
	IOPlatformFunctionIterator 	*iter;
	UInt8						scratchBuffer[READ_BUFFER_LEN], mode = kI2CUnspecifiedMode;
	UInt8						*maskBytes, *valueBytes;
	unsigned					delayMS;
	UInt32 						cmd, cmdLen, result, param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;

	DLOG ("AppleMaxim1989::performFunction(%x) - entered\n", fI2CAddress);

	if (!func)
		return false;
	
	if (!(iter = func->getCommandIterator()))
		return false;
	
	while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, &result)) {
		if (result != kIOPFNoError) {
			DLOG("AppleMaxim1989::performFunction(%x) error parsing platform function\n", fI2CAddress);

			success = false;
			goto PERFORM_FUNCTION_EXIT;
		}

		DLOG ("AppleMaxim1989::performFunction(%x) - 1)0x%lx, 2)0x%lx, 3)0x%lx, 4)0x%lx, 5)0x%lx,"
				"6)0x%lx, 7)0x%lx, 8)0x%lx, 9)0x%lx, 10)0x%lx\n", fI2CAddress, param1, param2, param3,
				param4, param5, param6, param7, param8, param9, param10);

		switch (cmd)
		{
			// kCommandReadI2C, kCommandWriteI2C and kCommandRMWI2C are not supported
			// because they don't specify the command/subaddress for the max1989
			// communication.

			case kCommandDelay:

				// param1 is in microseconds, but we are going to sleep so use
				// milliseconds
				delayMS = param1 / 1000;
				if (delayMS != 0) IOSleep(delayMS);

				DLOG("AppleMaxim1989::performFunction(%x) delay %u\n", fI2CAddress, delayMS);

				break;

			case kCommandReadI2CSubAddr:

				if (param2 > READ_BUFFER_LEN)
				{
					IOLog("AppleMaxim1989::performFunction(%x) r-sub operation too big!\n", fI2CAddress);

					success = false;
					goto PERFORM_FUNCTION_EXIT;
				}

				// open the bus
				if (!openI2C())
				{
					IOLog("AppleMaxim1989::performFunction(%x) r-sub failed to open I2C\n", fI2CAddress);

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

					DLOG("AppleMaxim1989::performFunction(%x) r-sub %x len %x, got data", fI2CAddress, param1, param2);
#ifdef MAX1989_DEBUG
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
					IOLog("AppleMaxim1989::performFunction(%x) w-sub failed to open I2C\n", fI2CAddress);

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
	
					DLOG("AppleMaxim1989::performFunction(%x) w-sub %x len %x data", fI2CAddress, param1, param2);
#ifdef MAX1989_DEBUG
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

				DLOG("AppleMaxim1989::performFunction(%x) mode %x\n", fI2CAddress, mode);
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
					IOLog("AppleMaxim1989::performFunction(%x) invalid mw-sub cycle\n", fI2CAddress);

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
					IOLog("AppleMaxim1989::performFunction(%x) mw-sub failed to open I2C\n", fI2CAddress);

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
	
					DLOG("AppleMaxim1989::performFunction(%x) mw-sub %x len %x data", fI2CAddress, param1, param4);
#ifdef MAX1989_DEBUG
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
				DLOG ("AppleMaxim1989::performFunction - bad command %ld\n", cmd);

				success = false;
				goto PERFORM_FUNCTION_EXIT;
		}
	}

PERFORM_FUNCTION_EXIT:
	iter->release();

	DLOG ("AppleMaxim1989::performFunction - done - returning %s\n", success ? "TRUE" : "FALSE");
	return(success);
}

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn AppleMaxim1989::callPlatformFunction(const OSSymbol *functionName,
				bool waitForFunction, void *param1, void *param2,
				void *param3, void *param4)
{
	UInt32 maximReg = (UInt32)param1;
	SInt32 *temp_buf = (SInt32 *)param2;

	DLOG("AppleMaxim1989::callPlatformFunction(%x) %s %s %08lx %08lx %08lx %08lx\n",
			fI2CAddress, functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
			(UInt32) param1, (UInt32) param2, (UInt32) param3, (UInt32) param4);

	if (functionName->isEqualTo(fGetSensorValueSym) == true)
	{
		if (fSleep == true)
			return(kIOReturnOffline);

		if (temp_buf == NULL)
			return(kIOReturnBadArgument);

		return(getTemp(maximReg, temp_buf));
	}

	return(super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4));
}

/*******************************************************************************
 * Publish the children nubs for the Maxim 1989 nub - publishChildren()
 *******************************************************************************/

IOReturn AppleMaxim1989::publishChildren(IOService *nub)
{
    OSIterator			*childIterator = NULL;
    IORegistryEntry		*childEntry = NULL;
    IOService			*childNub = NULL;

	childIterator = nub->getChildIterator(gIODTPlane);
	if( childIterator != NULL )
	{
		// Iterate through children and create nubs
		while ( ( childEntry = (IORegistryEntry *)( childIterator->getNextObject() ) ) != NULL )
		{
			// Publish child as IOService
			childNub = OSDynamicCast(IOService, OSMetaClass::allocClassWithName("IOService"));
		
			childNub->init(childEntry, gIODTPlane);
			childNub->attach(this);
			childNub->registerService();
                           			
			DLOG("AppleMaxim1989::publishChildren(0x%x) published child %s\n", fI2CAddress<<1, childEntry->getName());
		}
	
		childIterator->release();
	}

	return kIOReturnSuccess;
}

#pragma mark -
#pragma mark *** Read Temperature Channels ***
#pragma mark -

/*******************************************************************************
 * Read temperature channels from the device
 *******************************************************************************/

IOReturn AppleMaxim1989::getTemp( UInt32 maximReg, SInt32 * temp )
{
//	UInt8				status;
	UInt8				integer;

	// Open the bus - this grabs a mutex in the I2C driver so it's thread-safe
	if (!openI2C())
	{
		IOLog("AppleMaxim1989::getTemp error opening bus!\n");
		goto failNoClose;
	}

	// reads should be performed in combined mode
	if (!setI2CCombinedMode())
	{
		IOLog("AppleMaxim1989::getTemp failed to set bus mode!\n");
		goto failClose;
	}

	// get the temperature associated with maximReg register
	if (!readI2C( maximReg, &integer, 1 ))
	{
		IOLog("AppleMaxim1989::getTemp read temp failed!\n");
		goto failClose;
	}

	// close the bus
	closeI2C();

	// format the 16.16 fixed point temperature and return it
	// Integer is a signed 7-bit temperature value.  Funky casting is required to sign-extend
	// temperature.

	*temp = ( ( ( SInt8 ) ( integer << 1 ) ) << 15 );
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

IOReturn AppleMaxim1989::powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*)
{	
    if ( ! (theFlags & IOPMPowerOn) ) {
        // Sleep sequence:
		DLOG("AppleMaxim1989::powerStateWillChangeTo - sleep\n");
		doSleep();
   } else {
        // Wake sequence:
		DLOG("AppleMaxim1989::powerStateWillChangeTo - wake\n");
		doWake();
    }
	
    return IOPMAckImplied;
}

void AppleMaxim1989::doSleep(void)
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

void AppleMaxim1989::doWake(void)
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

bool AppleMaxim1989::openI2C( void )
{
	UInt32 passBus;
	IOReturn status;

	DLOG("AppleMaxim1989::openI2C(%x) - entered\n", fI2CAddress);

	if (fI2C_iface == NULL)
		return false;

	// Open the interface
	passBus = (UInt32) fI2CBus;	// cast from 8-bit to machine long word length
	if ((status = (fI2C_iface->callPlatformFunction(kOpenI2Cbus, false, (void *) passBus, NULL, NULL, NULL)))
			!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim1989::openI2C(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

void AppleMaxim1989::closeI2C( void )
{
	IOReturn status;

	DLOG("AppleMaxim1989::closeI2C(%x) - entered\n", fI2CAddress);

	if (fI2C_iface == NULL) return;

	if ((status = (fI2C_iface->callPlatformFunction(kCloseI2Cbus, false, NULL, NULL, NULL, NULL)))
			!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim1989::closeI2C(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
	}
}

bool AppleMaxim1989::setI2CDumbMode( void )
{
	IOReturn status = kIOReturnSuccess;

	DLOG("AppleMaxim1989::setI2CDumbMode(%x) - entered\n", fI2CAddress);

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetDumbMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim1989::setI2CDumbMode(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleMaxim1989::setI2CStandardMode( void )
{
	IOReturn status = kIOReturnSuccess;

	DLOG("AppleMaxim1989::setI2CStandardMode(%x) - entered\n", fI2CAddress);

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetStandardMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim1989::setI2CStandardMode(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleMaxim1989::setI2CStandardSubMode( void )
{
	IOReturn status = kIOReturnSuccess;

	DLOG("AppleMaxim1989::setI2CStandardSubMode(%x) - entered\n", fI2CAddress);

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetStandardSubMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim1989::setI2CStandardSubMode(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleMaxim1989::setI2CCombinedMode( void )
{
	IOReturn status = kIOReturnSuccess;

	DLOG("AppleMaxim1989::setI2CCombinedMode(%x) - entered\n", fI2CAddress);

	if ((fI2C_iface == NULL) ||
		(status = (fI2C_iface->callPlatformFunction(kSetCombinedMode, false, NULL, NULL, NULL, NULL)))
				!= kIOReturnSuccess)
	{
		IOLog("AppleMaxim1989::setI2CCombinedMode(%x) failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		return(false);
	}

	return(true);
}

bool AppleMaxim1989::writeI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
	UInt32 passAddr, passSubAddr, passSize;
	unsigned int retries = 0;
	IOReturn status;

	DLOG("AppleMaxim1989::writeI2C(%x) - entered\n", fI2CAddress);

	if (fI2C_iface == NULL || data == NULL || size == 0)
		return false;

	passAddr = (UInt32) (fI2CAddress >> 1);
	passSubAddr = (UInt32) subAddr;
	passSize = (UInt32) size;

	while ((retries < kNumRetries) &&
	       ((status = (fI2C_iface->callPlatformFunction(kWriteI2Cbus, false, (void *) passAddr,
		    (void *) passSubAddr, (void *) data, (void *) passSize))) != kIOReturnSuccess))
	{
		IOLog("AppleMaxim1989::writeI2C(%x) transaction failed, status = %08lx\n", fI2CAddress, (UInt32) status);
		retries++;
	}

	if (retries >= kNumRetries)
		return(false);
	else
		return(true);
}

bool AppleMaxim1989::readI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
	UInt32 passAddr, passSubAddr, passSize;
	unsigned int retries = 0;
	IOReturn status;

	DLOG("AppleMaxim1989::readI2C(%x) \n", fI2CAddress);

	if (fI2C_iface == NULL || data == NULL || size == 0)
		return false;

	passAddr = (UInt32) (fI2CAddress >> 1);
	passSubAddr = (UInt32) subAddr;
	passSize = (UInt32) size;

	while ((retries < kNumRetries) &&
	       ((status = (fI2C_iface->callPlatformFunction(kReadI2Cbus, false, (void *) passAddr,
		    (void *) passSubAddr, (void *) data, (void *) passSize))) != kIOReturnSuccess))
	{
		IOLog("AppleMaxim1989::readI2C error (0x%08X) I2CAddr = 0x%02X, subAddr = 0x%02X\n", (unsigned int) status, fI2CAddress, subAddr);
		retries++;
	}

	if (retries >= kNumRetries)
		return(false);
	else
		return(true);
}
