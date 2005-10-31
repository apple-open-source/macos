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

#include "AppleLM7x.h"

#define super IOService
OSDefineMetaClassAndStructors(AppleLM7x, IOService)

bool AppleLM7x::systemIsRestarting = FALSE; // instantiate static member systemIsRestarting and reflect restart state as false

static const IOPMPowerState ourPowerStates[kLM7xNumStates] = 
{
	{kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {kIOPMPowerStateVersion1, kIOPMSleepCapability, kIOPMSleep, kIOPMSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
	{kIOPMPowerStateVersion1, kIOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

bool AppleLM7x::start(IOService *provider)
{
    mach_timespec_t 	WaitTimeOut;
    IOService			*childNub;
    OSArray 			*nubArray;
    OSData 				*t;
    int 				*reg;
    IOReturn 			status;
    
    // Proper etiquette
    if ( !(super::start(provider)) )
        return false;

    // Set flag to reflect non-restart state
    systemIsRestarting = FALSE;

    // Try and get the reg property
    if ( !(t = OSDynamicCast(OSData, provider->getProperty("reg"))) )
    {
        IOLog( "AppleLM7x::start couldn't find 'reg' property in registry.\n");
        return false;
    }

    // We have the reg property, lets make local copy
    if ( !(reg = (int*)t->getBytesNoCopy()) )
    {
        IOLog( "AppleLM7x::start 'reg' property is present but empty.\n");
        return false;
    }
		
    kLM7xBus = (UInt8)(*reg >> 8); // Determine I2C bus
    kLM7xAddr = (UInt8)(*reg >> 1); // Determine true address of device

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
        IOLog("AppleLM7x::start(0x%x) failed to get i2c interface\n", kLM7xAddr<<1);
        return(false);
    }

    DLOG("AppleLM7x::start(0x%x) got i2c interface %s\n", kLM7xAddr<<1, interface->getName());

    // Configure the DS1775
    if( initHW(provider) != kIOReturnSuccess )
    {
        IOLog("AppleLM7x::start(0x%x) failed to initialize sensor.\n", kLM7xAddr<<1);
        return false;
    }

    // Parse sensor properties and create nubs
    nubArray = parseSensorParamsAndCreateNubs(provider);
    if (nubArray == NULL || nubArray->getCount() == 0)
    {
        IOLog("AppleLM7x::start(0x%x) no thermal sensors found\n", kLM7xAddr<<1);
        if (nubArray)
            nubArray->release();
        
        return false;
    }

	// Save registers for PM
	if ( saveRegisters() != kIOReturnSuccess )
		IOLog("AppleLM7x::powerStateWillChangeTo(0x%x) failed to save registers.\n", kLM7xAddr<<1);

	// Initialize Power Management superclass variables from IOService.h
    PMinit();

	// Register as controlling driver from IOService.h
    status = registerPowerDriver( this, (IOPMPowerState *) ourPowerStates, kLM7xNumStates );
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

    // tell the world my sensor nubs are here
    for (unsigned int index = 0; index < nubArray->getCount(); index++)
    {
        childNub = OSDynamicCast(IOService, nubArray->getObject(index));
        if (childNub) 
            childNub->registerService();
    }
    nubArray->release();

    return true;
}

void AppleLM7x::stop(IOService *provider)
{
    super::stop(provider);
}

IOReturn AppleLM7x::initHW(IOService *provider)
{
    IOReturn status;
    UInt8 cfgReg;

    DLOG("AppleLM7x::initHW(0x%x) entered.\n", kLM7xAddr<<1);

    status = openI2C(kLM7xBus);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::initHW(0x%x) failed to open I2C bus.\n", kLM7xAddr<<1);
        return status;
    }
    
    status = readI2C((UInt8)kConfigurationReg, (UInt8 *) &cfgReg, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::initHW(0x%x) readI2C failed.\n", kLM7xAddr<<1);
        closeI2C();
        return status;
    }

    // Set R0 and R1 to get 12-bit resolution
    cfgReg |= (kCfgRegR0 | kCfgRegR1);

    // Failure of this write operation is not fatal -- just won't have extended temperature resolution
    status = writeI2C((UInt8)kConfigurationReg, (UInt8 *)&cfgReg, 1);

    closeI2C();

    return kIOReturnSuccess;
}

IOReturn AppleLM7x::getTemperature(SInt32 *temperature)
{
    IOReturn 	status;
    UInt8 	bytes[2];
    SInt16	reading;
    
    if (systemIsRestarting == TRUE)
        return false;

    status = openI2C(kLM7xBus);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::getTemperature(0x%x) failed to open I2C bus.\n", kLM7xAddr<<1);
        return status;
    }

    status = readI2C((UInt8)kTemperatureReg, bytes, 2);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::getTemperature(0x%x) readI2C failed.\n", kLM7xAddr<<1);
        closeI2C();
        return status;
    }

    closeI2C();

    reading = *((SInt16 *) bytes);
    // Temperature data is represented by a 9-bit, twoÕs complement word with 
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

IOReturn AppleLM7x::callPlatformFunction
(
    const OSSymbol *functionName,
    bool waitForFunction,
    void *param1, 
    void *param2,
    void *param3, 
    void *param4
)
{
    UInt32 	id = (UInt32)param1;
    SInt32 	*value = (SInt32 *)param2;
    
    DLOG
    (
        "AppleLM7x::callPlatformFunction(0x%x) %s %s %08lx %08lx %08lx %08lx\n",
        kLM7xAddr<<1,
        functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
        (UInt32)param1,
        (UInt32)param2,
        (UInt32)param3,
        (UInt32)param4
    );

    if (functionName->isEqualTo(sGetSensorValueSym) == TRUE)
    {
        if (systemIsRestarting == TRUE)
            return(kIOReturnOffline);
      
        if (id == fHWSensorIDMap[0])
            return(getTemperature(value));
    }
    
    return (super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4));
}

OSArray *AppleLM7x::parseSensorParamsAndCreateNubs(IOService *nub)
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
        DLOG("AppleLM7x::parseSensorParamsAndCreateNubs(0x%x) no param version\n", kLM7xAddr<<1);
        return(NULL);
    }

    version = *((UInt32 *)tempOSData->getBytesNoCopy());

	// I can only handle version 1
	if (version != 1)
	{
		DLOG("AppleLM7x::parseSensorParamsAndCreateNubs(0x%x) version != 1\n", kLM7xAddr<<1);
		return(NULL);
	}

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorIDKey));
    if (tempOSData == NULL)
    {
        DLOG("AppleLM7x::parseSensorParamsAndCreateNubs(0x%x) no ids\n", kLM7xAddr<<1);
        return(NULL);
    }

    n_sensors = tempOSData->getLength() / sizeof(UInt32);

    // the LM75 has only one temperature channel.  If there are more
    // sensors than this, something is wacky.
    if (n_sensors > 1)
    {
        DLOG("AppleLM7x::parseSensorParamsAndCreateNubs(0x%x) too many sensors %u\n", kLM7xAddr<<1, n_sensors);
        return(NULL);
    }

    id = (UInt32 *)tempOSData->getBytesNoCopy();

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorZoneKey));
    if (tempOSData == NULL)
    {
        DLOG("AppleLM7x::parseSensorParamsAndCreateNubs(0x%x) no zones\n", kLM7xAddr<<1);
        return(NULL);
    }

    zone = (UInt32 *)tempOSData->getBytesNoCopy();

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorTypeKey));
    if (tempOSData == NULL)
    {
        DLOG("AppleLM7x::parseSensorParamsAndCreateNubs(0x%x) no types\n", kLM7xAddr<<1);
        return(NULL);
    }

    type = (const char *)tempOSData->getBytesNoCopy();

    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorLocationKey));
    if (tempOSData == NULL)
    {
        DLOG("AppleLM7x::parseSensorParamsAndCreateNubs(0x%x) no locations\n", kLM7xAddr<<1);
        return(NULL);
    }

    location = (const char *)tempOSData->getBytesNoCopy();

    // Polling Period key is not required
    tempOSData = OSDynamicCast(OSData, nub->getProperty(kDTSensorPollingPeriodKey));
    if (tempOSData != NULL)
    {
        polling_period = (UInt32 *)tempOSData->getBytesNoCopy();
        DLOG("AppleLM7x::parseSensorParamsAndCreateNubs(0x%x) polling period %lu\n", kLM7xAddr<<1, polling_period);
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
#pragma mark *** I2C ***
#pragma mark -

// Open and set up the i2c bus
IOReturn AppleLM7x::openI2C(UInt8 id)
{
    IOReturn status;

    DLOG("AppleLM7x::openI2C(0x%x) entered.\n", kLM7xAddr<<1);

    if (interface == NULL)
    {
        DLOG("AppleLM7x::openI2C(0x%x) interface is NULL.\n", kLM7xAddr<<1);
        return kIOReturnBadArgument;
    }
    
    // Open the interface
    status = interface->callPlatformFunction(sOpenI2Cbus, false, (void *)((UInt32)id), NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::openI2C(0x%x) failed on 'OpenI2Cbus' command.\n", kLM7xAddr<<1);
        return status;
    }

    // the i2c driver does not support well read in interrupt mode
    // so it is better to "go polling" (read does not timeout on errors
    // in interrupt mode).
    status = interface->callPlatformFunction(sSetPollingMode, false, (void *)TRUE, NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::openI2C(0x%x) failed to set polling mode, status = %08lx\n", kLM7xAddr<<1, (UInt32) status);
        return status;
    }

    return kIOReturnSuccess;
}

// Close the i2c bus
IOReturn AppleLM7x::closeI2C( void )
{
    IOReturn status;

    DLOG("AppleLM7x::closeI2C(0x%x) entered.\n", kLM7xAddr<<1);

    if (interface == NULL)
    {
        DLOG("AppleLM7x::closeI2C(0x%x) interface is NULL.\n", kLM7xAddr<<1);
        return kIOReturnBadArgument;
    }

    status = interface->callPlatformFunction(sCloseI2Cbus, false, NULL, NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::closeI2C(0x%x) failed on 'closeI2Cbus' command.\n", kLM7xAddr<<1);
        return status;
    }
    
    return kIOReturnSuccess;
}

// Send "size" bytes in the i2c bus.
IOReturn AppleLM7x::writeI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
    IOReturn status;
    UInt8 attempts = 0;

    DLOG("AppleLM7x::writeI2C(0x%x) entered.\n", kLM7xAddr<<1);

    if ( (interface == NULL) || (data == NULL) || (size == 0) )
    {
        DLOG("AppleLM7x::writeI2C(0x%x) called with incorrect function parameters.\n", kLM7xAddr<<1);
        return kIOReturnBadArgument;
    }
    
    status = interface->callPlatformFunction(sSetStandardSubMode, false, NULL, NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::readI2C(0x%x) failed to set 'StandardSub' mode.\n", kLM7xAddr<<1);
        return status;
    }

    while(attempts < kTriesToAttempt)
    {
        status = 
        interface->callPlatformFunction
        (
            sWriteI2Cbus, 
            false, 
            (void *)((UInt32)kLM7xAddr), 
            (void *)((UInt32)subAddr), 
            (void *)data, 
            (void *)((UInt32)size)
        );
        
        if(status == kIOReturnSuccess)
        {
            DLOG("AppleLM7x::writeI2C(0x%x) wrote data = 0x%x\n", kLM7xAddr<<1, *data);    
            break;
        }
        
        DLOG("AppleLM7x::writeI2C(0x%x) failed attempt %d of 'writeI2C' command.\n", kLM7xAddr<<1, attempts+1);

        attempts++;
    }
    
    return status;
}

// Read "size" bytes from the i2c bus.
IOReturn AppleLM7x::readI2C(UInt8 subAddr, UInt8 *data, UInt16 size)
{
    IOReturn status;
    UInt8 attempts = 0;

    DLOG("AppleLM7x::readI2C(0x%x) entered.\n", kLM7xAddr<<1);

    if ( (interface == NULL) || (data == NULL) || (size == 0) )
    {
        DLOG("AppleLM7x::readI2C(0x%x) called with incorrect function parameters.\n", kLM7xAddr<<1);
        return kIOReturnBadArgument;
    }
    
    status = interface->callPlatformFunction(sSetCombinedMode, false, NULL, NULL, NULL, NULL);
    if (status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::readI2C(0x%x) failed to set 'Combined' mode.\n", kLM7xAddr<<1);
        return status;
    }
    
    while( attempts < kTriesToAttempt)
    {
        status = 
        interface->callPlatformFunction
        (
            sReadI2Cbus, 
            false, 
            (void *)((UInt32)kLM7xAddr), 
            (void *)((UInt32)subAddr), 
            (void *)data, 
            (void *)((UInt32)size)
        );
        
        if(status == kIOReturnSuccess)
        {
            DLOG("AppleLM7x::readI2C(0x%x) retrieved data = 0x%x\n", kLM7xAddr<<1, *data);    
            break;
        }
        
        DLOG("AppleLM7x::readI2C(0x%x) failed attempt %d of 'readI2C' command.\n", kLM7xAddr<<1, attempts+1);
            
        attempts++;
    }

    return status;
}

#pragma mark -
#pragma mark *** Power Management ***
#pragma mark -

/*******************************************************************************
 * Power Management callbacks and handlers
 *******************************************************************************/

IOReturn AppleLM7x::setPowerState(unsigned long whatState, IOService *dontCare)
{
    DLOG("AppleLM7x::setPowerState called with state:%lu\n", whatState);

    if ( (whatState == kLM7xOffState) )
    {
        // No Power State
        systemIsRestarting = TRUE; // set flag to reflect shutting down state.

        if ( saveRegisters() != kIOReturnSuccess )
            kprintf("AppleLM7x::setPowerState(0x%x) failed to save registers.\n", kLM7xAddr<<1);
    }

    if ( (whatState == kLM7xOnState) )
    {
        // Full Power State
        if ( restoreRegisters() != kIOReturnSuccess )
            kprintf("AppleLM7x::setPowerState(0x%x) failed to restore registers.\n", kLM7xAddr<<1);

        systemIsRestarting = FALSE; // set flag to reflect we are not shutting down.
    }
    
    return IOPMAckImplied;
}

IOReturn AppleLM7x::sysPowerDownHandler(void *target, void *refCon, UInt32 messageType, IOService *service, void *messageArgument, vm_size_t argSize )
{
    IOReturn status = kIOReturnSuccess;

    DLOG("AppleLM7x::sysPowerDownHandler called with 0x%x\n", messageType);

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

IOReturn AppleLM7x::saveRegisters()
{
    IOReturn status;

    DLOG("AppleLM7x::saveRegisters(0x%x) entered.\n", kLM7xAddr<<1);

    status = openI2C(kLM7xBus);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::saveRegisters(0x%x) failed to open I2C bus.\n", kLM7xAddr<<1);
        return status;
    }

    status = readI2C((UInt8)kConfigurationReg, (UInt8 *)&savedRegisters.Configuration, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::saveRegisters(0x%x) readI2C failed.\n", kLM7xAddr<<1);
        closeI2C();
        return status;
    }

    status = readI2C((UInt8)kT_hystReg, (UInt8 *)&savedRegisters.Thyst, 2);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::saveRegisters(0x%x) readI2C failed.\n", kLM7xAddr<<1);
        closeI2C();
        return status;
    }

    status = readI2C((UInt8)kT_osReg, (UInt8 *)&savedRegisters.Tos, 2);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::saveRegisters(0x%x) readI2C failed.\n", kLM7xAddr<<1);
        closeI2C();
        return status;
    }

    closeI2C();

    return status;
}

IOReturn AppleLM7x::restoreRegisters()
{
    IOReturn status;

    DLOG("AppleLM7x::restoreRegisters(0x%x) entered.\n", kLM7xAddr<<1);

    status = openI2C(kLM7xBus);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::restoreRegisters(0x%x) failed to open I2C bus.\n", kLM7xAddr<<1);
        return status;
    }

    status = writeI2C((UInt8)kConfigurationReg, (UInt8 *)&savedRegisters.Configuration, 1);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::restoreRegisters(0x%x) writeI2C failed.\n", kLM7xAddr<<1);
        closeI2C();
        return status;
    }

    status = writeI2C((UInt8)kT_hystReg, (UInt8 *)&savedRegisters.Thyst, 2);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::restoreRegisters(0x%x) writeI2C failed.\n", kLM7xAddr<<1);
        closeI2C();
        return status;
    }

    status = writeI2C((UInt8)kT_osReg, (UInt8 *)&savedRegisters.Tos, 2);
    if(status != kIOReturnSuccess)
    {
        DLOG("AppleLM7x::restoreRegisters(0x%x) writeI2C failed.\n", kLM7xAddr<<1);
        closeI2C();
        return status;
    }

    closeI2C();

    return status;
}
