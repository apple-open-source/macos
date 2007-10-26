/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 *  DRI: Tom Sherman
 *
 */

#include "AppleLM8x.h"

#define super IOService
OSDefineMetaClassAndStructors(AppleLM8x, IOService)

bool AppleLM8x::systemIsRestarting = FALSE; // instantiate static member systemIsRestarting and reflect restart state as false

static const IOPMPowerState ourPowerStates[kLM8xNumStates] = 
{
	{kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {kIOPMPowerStateVersion1, kIOPMSleepCapability, kIOPMSleep, kIOPMSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
	{kIOPMPowerStateVersion1, kIOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

bool AppleLM8x::start(IOService *provider)
{
	mach_timespec_t		WaitTimeOut;
	OSData				*t;
	int					*reg;
    IOReturn 			status;

	// Proper etiquette
	if ( !(super::start(provider)) )
		return false;
	
	// Set flag to reflect non-restart state
	systemIsRestarting = FALSE;
	
	// Try and get the reg property
	if ( !(t = OSDynamicCast(OSData, provider->getProperty("reg"))) )
	{
		IOLog( "AppleLM8x::start couldn't find 'reg' property in registry.\n");
		return false;
	}
	
	// We have the reg property, lets make local copy
	if ( !(reg = (int*)t->getBytesNoCopy()) )
	{
		IOLog( "AppleLM8x::start 'reg' property is present but empty.\n");
		return false;
	}
	
	kLM8xBus = (UInt8)(*reg >> 8); // Determine I2C bus
	kLM8xAddr = (UInt8)(*reg >> 1); // Determine true address of device
	
	// Use a 30 second timeout when calling waitForService()
	WaitTimeOut.tv_sec = 30;
	WaitTimeOut.tv_nsec = 0;
	
	// Create some symbols for later use
	sOpenI2Cbus = OSSymbol::withCStringNoCopy(kOpenI2Cbus);
	sCloseI2Cbus = OSSymbol::withCStringNoCopy(kCloseI2Cbus);
	sSetPollingMode = OSSymbol::withCStringNoCopy(kSetPollingMode);
	sSetStandardSubMode = OSSymbol::withCStringNoCopy(kSetStandardSubMode);
	sSetCombinedMode = OSSymbol::withCStringNoCopy(kSetCombinedMode);
	sWriteI2Cbus = OSSymbol::withCStringNoCopy(kWriteI2Cbus);
	sReadI2Cbus = OSSymbol::withCStringNoCopy(kReadI2Cbus);
	sGetSensorValueSym = OSSymbol::withCString("getSensorValue");
	
	interface = OSDynamicCast(IOService, provider->getParentEntry(gIOServicePlane));
	if(interface == NULL)
	{
		IOLog("AppleLM8x::start(0x%x) failed to get i2c interface\n", kLM8xAddr<<1);
		return(false);
	}
	
	DLOG("AppleLM8x::start(0x%x) got i2c interface %s\n", kLM8xAddr<<1, interface->getName());
	
	// Configure the LM87
	if( initHW(provider) != kIOReturnSuccess )
	{
		IOLog("AppleLM8x::start(0x%x) failed to initialize sensor.\n", kLM8xAddr<<1);
		return false;
	}

	// Save registers for PM
	if ( saveRegisters() != kIOReturnSuccess )
		IOLog("AppleLM8x::powerStateWillChangeTo(0x%x) failed to save registers.\n", kLM8xAddr<<1);

	// Initialize Power Management superclass variables from IOService.h
    PMinit();

	// Register as controlling driver from IOService.h
    status = registerPowerDriver( this, (IOPMPowerState *) ourPowerStates, kLM8xNumStates );
	if (status != kIOReturnSuccess)
	{
		IOLog("%s: Failed to registerPowerDriver.\n", getName());
	}

	// Join the Power Management tree from IOService.h
	provider->joinPMtree( this);
	
	// Install power change handler (for restart notification)
	registerPrioritySleepWakeInterest(&sysPowerDownHandler, this, 0);
		
	// Register so others can find us with a waitForService()
	registerService();
	
	// Parse sensor properties and create nubs
	publishChildren(provider);

	return true;
}

void AppleLM8x::stop(IOService *provider)
{
	super::stop(provider);
}

IOReturn AppleLM8x::initHW(IOService *provider)
{
    IOReturn status;
    UInt8 cfgReg = 0;
    UInt8 attempts = 0;

    DLOG("AppleLM8x::initHW(0x%x) entered.\n", kLM8xAddr<<1);

    status = openI2C(kLM8xBus);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::initHW(0x%x) failed to open I2C bus.\n", kLM8xAddr<<1);
        return status;
    }
 
	// Attempt to start LM87 by setting bit 0 in Configuration Register 1.  If we detect the RESET
	// bit, then make a second attempt.
	for ( ; attempts < 2; attempts++ )
	{
		// Read Configuration Register 1
		status = readI2C((UInt8)kConfReg1, (UInt8 *) &cfgReg, 1);
		if(status != kIOReturnSuccess)
		{
			DLOG("AppleLM8x::initHW(0x%x) readI2C failed.\n", kLM8xAddr<<1);
			break;
		}

		DLOG("AppleLM8x::readI2C(0x%x) retrieved data = 0x%x\n", kLM8xAddr<<1, cfgReg);    

		// If we detect RESET, wait at least 45ms for it to clear.
		if ( cfgReg & kConfRegRESET )
		{
			IOSleep(50);
			status = kIOReturnError;
		}

		if(status != kIOReturnSuccess)
		{
			IOLog("AppleLM8x::initHW(0x%x) readI2C detected RESET bit.\n", kLM8xAddr<<1);
		}

		// Start monitoring operations
		cfgReg = 0x1;

		// Failure of this write operation is fatal
		status = writeI2C((UInt8)kConfReg1, (UInt8 *)&cfgReg, 1);

		// Read Configuration Register 1
		status = readI2C((UInt8)kConfReg1, (UInt8 *) &cfgReg, 1);
		if(status != kIOReturnSuccess)
		{
			DLOG("AppleLM8x::initHW(0x%x) readI2C failed.\n", kLM8xAddr<<1);
			break;
		}
	
		if ( cfgReg == kConfRegStart )
		{
			break;
		}
		else
		{
			IOLog("AppleLM8x::readI2C(0x%x) retrieved configuration register 1 = 0x%x\n", kLM8xAddr<<1, cfgReg);    
		}
	}

    closeI2C();

    return status;
}

// AppleLM87::getReading
IOReturn AppleLM8x::getReading(UInt32 subAddress, SInt32 *value)
{
    IOReturn status;
    UInt8 data;
    
    if (systemIsRestarting == TRUE)
        return kIOReturnOffline;

    status = openI2C(kLM8xBus);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::getReading(0x%x) failed to open I2C bus.\n", kLM8xAddr<<1);
        return status;
    }

    status = readI2C((UInt8)subAddress, (UInt8 *) &data, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::getReading(0x%x) readI2C failed.\n", kLM8xAddr<<1);
        closeI2C();
        return status;
    }
	
	closeI2C();

	*value = (SInt32)data;

	return kIOReturnSuccess;
}

#pragma mark -
#pragma mark *** Platform Functions ***
#pragma mark -

/*******************************************************************************
 * IOHWSensor entry point - callPlatformFunction()
 *******************************************************************************/

IOReturn AppleLM8x::callPlatformFunction
(
    const OSSymbol *functionName,
    bool waitForFunction,
    void *param1, 
    void *param2,
    void *param3, 
    void *param4
)
{
    UInt32 	reg = (UInt32)param1;
    SInt32 	*value = (SInt32 *)param2;
    
    DLOG
    (
        "AppleLM8x::callPlatformFunction(0x%x) %s %s %08lx %08lx %08lx %08lx\n",
        kLM8xAddr<<1,
        functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
        (UInt32)param1,
        (UInt32)param2,
        (UInt32)param3,
        (UInt32)param4
    );

    if (functionName->isEqualTo(sGetSensorValueSym) == TRUE)
    {
		IOReturn status;
		
        if (systemIsRestarting == TRUE)
            return(kIOReturnOffline);

		for(int i = 0; i < LUNtableElement; i++)
		{
			if(reg == LUNtable[i].SubAddress)
			{
				status = getReading((UInt8)LUNtable[i].SubAddress, value);

				if(LUNtable[i].type == kTypeTemperature)
				{
					UInt8					reading = *value;
					*value = ( ( ( ( SInt8 ) reading ) << 16 ) * LUNtable[i].ConversionMultiple );
				}
				
				if(LUNtable[i].type == kTypeADC)
					*value = (UInt32)(*value * LUNtable[i].ConversionMultiple);

				if(LUNtable[i].type == kTypeVoltage)
					*value = (UInt32)(*value * LUNtable[i].ConversionMultiple);
				
				return status;
			}
		}
    }
    
    return (super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4));
}

IOReturn AppleLM8x::publishChildren(IOService *nub)
{
	OSIterator			*childIterator = NULL;
	IORegistryEntry		*childEntry = NULL;
    IOService			*childNub = NULL;
    IOReturn 			status;

	childIterator = nub->getChildIterator(gIODTPlane);
	if( childIterator != NULL )
	{
		// Iterate through children and create nubs
		while ( ( childEntry = (IORegistryEntry *)( childIterator->getNextObject() ) ) != NULL )
		{
			// Build Table
			status = buildEntryTable(childEntry);
			LUNtableElement++;
			
			// Publish child as IOService
			childNub = OSDynamicCast(IOService, OSMetaClass::allocClassWithName("IOService"));
		
			childNub->init(childEntry, gIODTPlane);
			childNub->attach(this);
			childNub->registerService();
			
//			DLOG("AppleLM8x::publishChildren(0x%x) published child %s\n", kLM8xAddr<<1, childEntry->getName());
		}
	
		childIterator->release();
	}

	return kIOReturnSuccess;
}

IOReturn AppleLM8x::buildEntryTable(IORegistryEntry *child)
{
	OSData *data;
	
	// Build Table

	// SensorID -- no longer required - eem
/*
	data = OSDynamicCast(OSData, child->getProperty("sensor-id"));
	if(data)
		LUNtable[LUNtableElement].SensorID = *(UInt32 *)data->getBytesNoCopy();
*/
	// SubAddress
	data = OSDynamicCast(OSData, child->getProperty("reg"));
	if(data)
		LUNtable[LUNtableElement].SubAddress = *(UInt32 *)data->getBytesNoCopy();

	// ConversionMultiple
	if (LUNtable[LUNtableElement].SubAddress == 0x20) // 2.5Vin or External Temperature 2
	{
		data = OSDynamicCast(OSData, child->getProperty("device_type"));

		if (data)
		{
			char *ptr = (char *)data->getBytesNoCopy();

			if(strcmp(ptr, "adc-sensor") == 0) // fix for broken DT
			{
				LUNtable[LUNtableElement].ConversionMultiple = k25VinMultiplier;
				LUNtable[LUNtableElement].type = kTypeADC;
			}
			else if(strcmp(ptr, "voltage-sensor") == 0)
			{
				LUNtable[LUNtableElement].ConversionMultiple = k25VinMultiplier;
				LUNtable[LUNtableElement].type = kTypeVoltage;
			}
			else
			{
				LUNtable[LUNtableElement].ConversionMultiple = 1;				
				LUNtable[LUNtableElement].type = kTypeTemperature;
			}
		}
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x21) // Vccp1
	{
		LUNtable[LUNtableElement].ConversionMultiple = Vccp1Multiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x22) // Vcc
	{
		LUNtable[LUNtableElement].ConversionMultiple = kVccMultiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x23) // 5V
	{
		LUNtable[LUNtableElement].ConversionMultiple = k5VinMultiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x24) // 12V
	{
		LUNtable[LUNtableElement].ConversionMultiple = k12VinMultiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x25) // Vccp2
	{
		LUNtable[LUNtableElement].ConversionMultiple = Vccp2Multiplier;
		LUNtable[LUNtableElement].type = kTypeADC;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x26) // External Temperature 1
	{
		LUNtable[LUNtableElement].ConversionMultiple = 1;
		LUNtable[LUNtableElement].type = kTypeTemperature;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x27) // Internal Temperature
	{
		LUNtable[LUNtableElement].ConversionMultiple = 1;
		LUNtable[LUNtableElement].type = kTypeTemperature;
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x28) // FAN1 or AIN1
	{
		data = OSDynamicCast(OSData, child->getProperty("device_type"));

		if (data)
		{
			char *ptr = (char *)data->getBytesNoCopy();

			if(strcmp(ptr, "adc-sensor") == 0) // fix for broken DT
			{
				LUNtable[LUNtableElement].ConversionMultiple = AIN1Multiplier;
				LUNtable[LUNtableElement].type = kTypeADC;
			}
			else if(strcmp(ptr, "voltage-sensor") == 0)
			{
				LUNtable[LUNtableElement].ConversionMultiple = AIN1Multiplier;
				LUNtable[LUNtableElement].type = kTypeVoltage;
			}
			else
			{
				LUNtable[LUNtableElement].ConversionMultiple = 0; // special case
				LUNtable[LUNtableElement].type = kTypeRPM;
			}
		}
	}
	else if (LUNtable[LUNtableElement].SubAddress == 0x29) // FAN2 or AIN2
	{
		data = OSDynamicCast(OSData, child->getProperty("device_type"));

		if (data)
		{
			char *ptr = (char *)data->getBytesNoCopy();

			if(strcmp(ptr, "adc-sensor") == 0) // fix for broken DT
			{
				LUNtable[LUNtableElement].ConversionMultiple = AIN2Multiplier;
				LUNtable[LUNtableElement].type = kTypeADC;
			}
			else if(strcmp(ptr, "voltage-sensor") == 0)
			{
				LUNtable[LUNtableElement].ConversionMultiple = AIN2Multiplier;
				LUNtable[LUNtableElement].type = kTypeVoltage;
			}
			else
			{
				LUNtable[LUNtableElement].ConversionMultiple = 0; // special case;
				LUNtable[LUNtableElement].type = kTypeRPM;
			}
		}
	}

	return kIOReturnSuccess;
}

#pragma mark -
#pragma mark *** I2C ***
#pragma mark -

// Open and set up the i2c bus
IOReturn AppleLM8x::openI2C(UInt8 id)
{
    IOReturn status;

    DLOG("AppleLM8x::openI2C(0x%x) entered.\n", kLM8xAddr<<1);

    if (interface == NULL)
    {
        DLOG("AppleLM8x::openI2C(0x%x) interface is NULL.\n", kLM8xAddr<<1);
        return kIOReturnBadArgument;
    }
    
    // Open the interface
    status = interface->callPlatformFunction(sOpenI2Cbus, false, (void *)((UInt32)id), NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::openI2C(0x%x) failed on 'OpenI2Cbus' command.\n", kLM8xAddr<<1);
        return status;
    }

    // the i2c driver does not support well read in interrupt mode
    // so it is better to "go polling" (read does not timeout on errors
    // in interrupt mode).
    status = interface->callPlatformFunction(sSetPollingMode, false, (void *)TRUE, NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::openI2C(0x%x) failed to set polling mode, status = %08lx\n", kLM8xAddr<<1, (UInt32) status);
        return status;
    }

    return kIOReturnSuccess;
}

// Close the i2c bus
IOReturn AppleLM8x::closeI2C( void )
{
    IOReturn status;

    DLOG("AppleLM8x::closeI2C(0x%x) entered.\n", kLM8xAddr<<1);

    if (interface == NULL)
    {
        DLOG("AppleLM8x::closeI2C(0x%x) interface is NULL.\n", kLM8xAddr<<1);
        return kIOReturnBadArgument;
    }

    status = interface->callPlatformFunction(sCloseI2Cbus, false, NULL, NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::closeI2C(0x%x) failed on 'closeI2Cbus' command.\n", kLM8xAddr<<1);
        return status;
    }
    
    return kIOReturnSuccess;
}

// Send "size" bytes in the i2c bus.
IOReturn AppleLM8x::writeI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
    IOReturn status;
    UInt8 attempts = 0;

    DLOG("AppleLM8x::writeI2C(0x%x) entered.\n", kLM8xAddr<<1);

    if ( (interface == NULL) || (data == NULL) || (size == 0) )
    {
        DLOG("AppleLM8x::writeI2C(0x%x) called with incorrect function parameters.\n", kLM8xAddr<<1);
        return kIOReturnBadArgument;
    }
    
    status = interface->callPlatformFunction(sSetStandardSubMode, false, NULL, NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::readI2C(0x%x) failed to set 'StandardSub' mode.\n", kLM8xAddr<<1);
        return status;
    }

    while(attempts < kTriesToAttempt)
    {
        status = 
        interface->callPlatformFunction
        (
            sWriteI2Cbus, 
            false, 
            (void *)((UInt32)kLM8xAddr), 
            (void *)((UInt32)subAddr), 
            (void *)data, 
            (void *)((UInt32)size)
        );

		if(status == kIOReturnSuccess)
			break;
        
        attempts++;

		IOSleep(10); // sleep 10 milliseconds
    }

	if(status != kIOReturnSuccess)
	{
		IOLog("AppleLM8x::writeI2C(0x%x) failed %d attempts of the 'writeI2C' command for subaddr 0x%x.\n", kLM8xAddr<<1, attempts, subAddr);
		return status;
	}
	
	DLOG("AppleLM8x::writeI2C(0x%x) wrote data = 0x%x\n", kLM8xAddr<<1, *data);    

    return status;
}

// Read "size" bytes from the i2c bus.
IOReturn AppleLM8x::readI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
    IOReturn status;
    UInt8 attempts = 0;

    DLOG("AppleLM8x::readI2C(0x%x) entered.\n", kLM8xAddr<<1);

    if ( (interface == NULL) || (data == NULL) || (size == 0) )
    {
        DLOG("AppleLM8x::readI2C(0x%x) called with incorrect function parameters.\n", kLM8xAddr<<1);
        return kIOReturnBadArgument;
    }
    
    status = interface->callPlatformFunction(sSetCombinedMode, false, NULL, NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::readI2C(0x%x) failed to set 'Combined' mode.\n", kLM8xAddr<<1);
        return status;
    }
    
    while( attempts < kTriesToAttempt)
    {
        status = 
        interface->callPlatformFunction
        (
            sReadI2Cbus, 
            false, 
            (void *)((UInt32)kLM8xAddr), 
            (void *)((UInt32)subAddr), 
            (void *)data, 
            (void *)((UInt32)size)
        );

		// Occasionally we get a valid I2C read that contains bogus data.  With the current
		// hardware, the bogus data is always 0xFF.  We just a check for this, and retry
		// if necessary.
        if(*data == 0xFF)
			status = kIOReturnError;
        
        if(status == kIOReturnSuccess)
            break;
                    
        attempts++;

		IOSleep(10); // sleep 10 milliseconds
    }

	if(*data == 0xFF)
	{
		IOLog("AppleLM8x::readI2C(0x%x) failed %d attempts of the 'readI2C' command for subaddr 0x%x (BOGUS DATA)\n", kLM8xAddr<<1, attempts, subAddr);    
		return status;
	}
	
	if(status != kIOReturnSuccess)
	{
		IOLog("AppleLM8x::readI2C(0x%x) failed %d attempts of the 'readI2C' command for subaddr 0x%x.\n", kLM8xAddr<<1, attempts, subAddr);
		return status;
	}

	DLOG("AppleLM8x::readI2C(0x%x) retrieved data 0x%x from subaddr 0x%x\n", kLM8xAddr<<1, *data, subAddr);    

    return status;
}

#pragma mark -
#pragma mark *** Power Management ***
#pragma mark -

/*******************************************************************************
 * Power Management callbacks and handlers
 *******************************************************************************/

IOReturn AppleLM8x::setPowerState(unsigned long whatState, IOService *dontCare)
{
    DLOG("AppleLM8x::setPowerState called with state:%lu\n", whatState);

    if ( (whatState == kLM8xOffState) )
    {
        // No Power State
        systemIsRestarting = TRUE; // set flag to reflect shutting down state.

        if ( saveRegisters() != kIOReturnSuccess )
            IOLog("AppleLM8x::powerStateWillChangeTo(0x%x) failed to save registers.\n", kLM8xAddr<<1);
    }

    if ( (whatState == kLM8xOnState) )
    {
        // Full Power State
        if ( restoreRegisters() != kIOReturnSuccess )
            IOLog("AppleLM8x::powerStateWillChangeTo(0x%x) failed to restore registers.\n", kLM8xAddr<<1);

        systemIsRestarting = FALSE; // set flag to reflect we are not shutting down.
    }

    return IOPMAckImplied;
}

IOReturn AppleLM8x::sysPowerDownHandler(void *target, void *refCon, UInt32 messageType, IOService *service, void *messageArgument, vm_size_t argSize )
{
    IOReturn status = kIOReturnSuccess;

    DLOG("AppleLM8x::sysPowerDownHandler called with 0x%x\n", messageType);

    switch (messageType)
    {
        case kIOMessageSystemWillSleep:
            break;
            
        case kIOMessageSystemWillPowerOff: // iokit_common_msg(0x250)
        case kIOMessageSystemWillRestart: // iokit_common_msg(0x310)
            systemIsRestarting = TRUE; // set flag to reflect shutting down state.
            break;

        default:
            status = kIOReturnUnsupported;
            break;
    }
    
    return status;
}

IOReturn AppleLM8x::saveRegisters()
{
    IOReturn status;

    DLOG("AppleLM8x::saveRegisters(0x%x) entered.\n", kLM8xAddr<<1);

    status = openI2C(kLM8xBus);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::saveRegisters(0x%x) failed to open I2C bus.\n", kLM8xAddr<<1);
        return status;
    }

    status = readI2C((UInt8)kChannelModeRegister, (UInt8 *)&savedRegisters.ChannelMode, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::saveRegisters(0x%x) readI2C failed.\n", kLM8xAddr<<1);
        closeI2C();
        return status;
    }

    status = readI2C((UInt8)kConfReg1, (UInt8 *)&savedRegisters.Configuration1, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::saveRegisters(0x%x) readI2C failed.\n", kLM8xAddr<<1);
        closeI2C();
        return status;
    }

    status = readI2C((UInt8)kConfReg2, (UInt8 *)&savedRegisters.Configuration2, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::saveRegisters(0x%x) readI2C failed.\n", kLM8xAddr<<1);
        closeI2C();
        return status;
    }

    closeI2C();

    return status;
}

IOReturn AppleLM8x::restoreRegisters()
{
    IOReturn status;

    DLOG("AppleLM8x::restoreRegisters(0x%x) entered.\n", kLM8xAddr<<1);

    status = openI2C(kLM8xBus);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::restoreRegisters(0x%x) failed to open I2C bus.\n", kLM8xAddr<<1);
        return status;
    }

    status = writeI2C((UInt8)kChannelModeRegister, (UInt8 *)&savedRegisters.ChannelMode, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::restoreRegisters(0x%x) writeI2C failed.\n", kLM8xAddr<<1);
        closeI2C();
        return status;
    }

    status = writeI2C((UInt8)kConfReg2, (UInt8 *)&savedRegisters.Configuration2, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::restoreRegisters(0x%x) writeI2C failed.\n", kLM8xAddr<<1);
        closeI2C();
        return status;
    }

	// restore Configuration Register 1 last, since it starts polling
    status = writeI2C((UInt8)kConfReg1, (UInt8 *)&savedRegisters.Configuration1, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM8x::restoreRegisters(0x%x) writeI2C failed.\n", kLM8xAddr<<1);
        closeI2C();
        return status;
    }
	
    closeI2C();

    return status;
}
