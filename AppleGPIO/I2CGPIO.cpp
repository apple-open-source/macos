/*
 * Copyright (c) 2002-2005 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include "I2CGPIO.h"

#define super IOService

OSDefineMetaClassAndStructors(I2CGPIO, IOService)

bool I2CGPIO::init(OSDictionary *dict)
{
	UInt32 i;

	if (!super::init(dict)) return(false);

	fI2CBus = 0;
	fI2CAddress = 0;
	
	fI2CInterface = 0;

	fRegisteredWithI2C = false;
	fRegisteredWithI2CLock = 0;

	fBayIndex = 0;
	fIntAddrInfo = 0;
	fIntRegState = 0;

	fCurrentPowerState = 0;

	fConfigReg = 0xFF;		// default to all input pins
	fPolarityReg = 0x00;	// no polarity inversion

#ifdef I2CGPIO_DEBUG
	fWriteCookie = 0;
	fReadCookie = 0;
#endif

	for (i=0; i<kNumGPIOs; i++)
		fClients[i] = 0;		// void the interrupt vectors

	return(true);
}

void I2CGPIO::free(void)
{
    super::free();
}

IOService *I2CGPIO::probe(IOService *provider, SInt32 *score)
{
	// combined 9554 node is just an information holder
	if (provider->getProperty(kI2CGPIOCombined))
	{
		*score = 0;
		return(0);
	}
	
	// Don't need to do anything, just use personality's probe score
    return(this);
}

bool I2CGPIO::start(IOService *provider)
{
	IOService				*parentDev;
	OSData					*regprop, *combined, *cfgData;
	UInt32					fullAddr, index, length;
	UInt32					*quadlet;
	OSCollectionIterator	*siblings;
	bool					found;
	IORegistryEntry			*entry;
	mach_timespec_t			waitTimeout;

	// We have two power states - off and on
	static const IOPMPowerState powerStates[kI2CGPIONumPowerStates] = {
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

	if (provider->getProperty(kI2CGPIOCombined))
	{
		return(false);
	}

    if (!super::start(provider)) return(false);

	// Extract bus number and address from reg property
	if ((regprop = OSDynamicCast(OSData, provider->getProperty("reg"))) == 0)
		return(false);
	else
	{
		fullAddr = *((UInt32 *)regprop->getBytesNoCopy());
		fI2CBus = (UInt8)((fullAddr >> 8) & 0x000000ff);
		fI2CAddress = (UInt8)(fullAddr & 0x000000fe);
		
		DLOG("I2CGPIO::start fI2CBus = %02x fI2CAddress = %02x\n",
				fI2CBus, fI2CAddress);
	}

	// Find the driver attached to the pmu-i2c device nub
	parentDev = OSDynamicCast(IOService,
			provider->getParentEntry(gIODTPlane));

	if (!parentDev) return(false);

	// [3994436] Wait for our provider PPCI2CInterface driver instance to publish an IOResource...
	// The resource key is formatted: "PPCI2CInterface.xxxx" where xxxx is the node name.
	OSData *data;
	if ( data = OSDynamicCast( OSData, parentDev->getProperty("name") ) )
	{
		char resourceName[256] = "PPCI2CInterface.";
		strncat( resourceName, (char *)data->getBytesNoCopy(), data->getLength() );
		IOService *resource;
		waitTimeout.tv_sec = 30;
		waitTimeout.tv_nsec = 0;
		if ( NULL == ( resource = OSDynamicCast( IOService, waitForService( resourceMatching( resourceName ), &waitTimeout ) ) ) )
		{
			kprintf( "I2CGPIO::start timeout waiting for IOResources \"%s\" property\n", resourceName );
			return false;
		}
		if ( NULL == ( fI2CInterface = OSDynamicCast( IOService, resource->getProperty( resourceName ) ) ) )
		{
			kprintf( "I2CGPIO::start failed to find IOResources \"%s\" property\n", resourceName );
			return false;
		}
	}
	else
	{
		kprintf( "I2CGPIO::start failed to find PPCI2CInterface provider node name\n" );
		return false;
	}

	// Figure out what i2c device our interrupts are associated with
	fullAddr <<= 8;

	if ((siblings =
			IODTFindMatchingEntries(parentDev, kIODTExclusive, 0)) == 0)
		return(false);

	found = false;

	while ((entry = OSDynamicCast(IORegistryEntry,
			siblings->getNextObject())) != 0)
	{
		//DLOG("I2CGPIO::start %x scanning %s\n",
		//		fI2CAddress,entry->getName());

		if ((combined = OSDynamicCast(OSData,
				entry->getProperty(kI2CGPIOCombined))) == 0)
			continue;

		//DLOG("I2CGPIO::start %x found %s\n",
		//		fI2CAddress, kI2CGPIOCombined);

		length = combined->getLength() / sizeof(UInt32);
		quadlet = (UInt32 *)combined->getBytesNoCopy();

		for (index = 0; index < length; index++)
		{
			if (fullAddr == *quadlet)
			{
				const char *compat;
				if (cfgData = OSDynamicCast(OSData, entry->getProperty("compatible")))
					if (compat = (const char *)cfgData->getBytesNoCopy())
						if (0 == strcmp(compat, "PCA9554M"))
							fC3Mapping = true;

				fBayIndex = index;
				regprop = OSDynamicCast(OSData, entry->getProperty("reg"));
				fIntAddrInfo = *((UInt32 *)regprop->getBytesNoCopy());
				fIntAddrInfo <<= 8;		// shift in subaddress = 0x00
				fIntAddrInfo |= 0x00000100;		// set the i2c read bit
				found = true;
				DLOG("I2CGPIO %x fIntAddrInfo = %08x fBayIndex = %u\n",
						fI2CAddress, fIntAddrInfo, fBayIndex);
				break;
			}
			
			//DLOG("%x %08x != %08x\n", fI2CAddress, fullAddr, *quadlet);
			quadlet++;
		}

		if (found) break;
	}

	if (siblings) siblings->release();

	if (!found)
	{
		DLOG("I2CGPIO %x couldn't find combined gpio node, bailing...\n",
			fI2CAddress);
		return(false);
	}

	// Get a reference to the KeySwitch driver
	waitTimeout.tv_sec = 30;
	waitTimeout.tv_nsec = 0;

	fKeyswitch = waitForService(serviceMatching("AppleKeyswitch"),
			&waitTimeout);

	// initialize current state to On
	fCurrentPowerState = kI2CGPIOPowerOn;

	// find out how to program the config register
	cfgData = OSDynamicCast(OSData, provider->getProperty("config-reg"));
	if (cfgData)
		fConfigReg = (UInt8)*((UInt32 *)cfgData->getBytesNoCopy());

	// find out how to program the polarity register
	cfgData = OSDynamicCast(OSData, provider->getProperty("polarity-reg"));
	if (cfgData)
		fPolarityReg = (UInt8)*((UInt32 *)cfgData->getBytesNoCopy());

	DLOG("I2CGPIO %x fConfigReg = %u fPolarityReg = %u\n", fI2CAddress, fConfigReg, fPolarityReg);

	// power management - join the tree
	PMinit();
	provider->joinPMtree(this);

	// register as controlling driver
	DLOG("I2CGPIO::start %u registering as power controller\n", fI2CAddress);
	if (registerPowerDriver(this, (IOPMPowerState *)powerStates,
			kI2CGPIONumPowerStates) != kIOReturnSuccess)
	{
		DLOG("I2CGPIO::start PM init failed\n");
		return(false);
	}
    
	// allocate my lock
	fRegisteredWithI2CLock = IOLockAlloc();

	// Allocate our constant callPlatformFunction symbols so we can be called at interrupt time.

    fSymOpenI2CBus = OSSymbol::withCString(kOpenI2CBus);
    fSymReadI2CBus = OSSymbol::withCString(kReadI2CBus);
    fSymWriteI2CBus = OSSymbol::withCString(kWriteI2CBus);
    fSymCloseI2CBus = OSSymbol::withCString(kCloseI2CBus);
    fSymSetCombinedMode = OSSymbol::withCString(kSetCombinedMode);
    fSymSetStandardSubMode = OSSymbol::withCString(kSetStandardSubMode);
    fSymRegisterForInts = OSSymbol::withCString(kRegisterForInts);
    fSymDeRegisterForInts = OSSymbol::withCString(kDeRegisterForInts);

	// start matching on children
	publishBelow(provider);
	
	DLOG("I2CGPIO::start %u succeeded\n", fI2CAddress);
	
	return(true);
}

void I2CGPIO::stop(IOService *provider)
{
	UInt32 i;
	
	for (i=0; i<kNumGPIOs; i++)
	{
		if (fClients[i] != 0)
		{
			IOFree(fClients[i], sizeof(I2CGPIOCallbackInfo));
			fClients[i] = 0;
		}
	}

	unregisterWithI2C();
	IOLockFree(fRegisteredWithI2CLock);
	fRegisteredWithI2CLock = 0;
	
	PMstop();

    super::stop(provider);
}

// Write to a 9554 on the i2c bus
IOReturn I2CGPIO::doI2CWrite(UInt8 busNo, UInt8 addr, UInt8 subAddr, UInt8 value, UInt8 mask)
{
	UInt8 eightBitAddr, tmp, data, retries;
    unsigned tmpInt1, tmpInt2;
	IOReturn retval, result = kIOReturnSuccess;

#ifdef I2CGPIO_DEBUG
	UInt32 myCookie = fWriteCookie++;
#endif

	eightBitAddr = addr;

	DLOG("+I2CGPIO::doI2CWrite(%s) bus:%x addr:%x sub:%x cookie:%u value:%x mask:%x\n",
			getProvider()->getName(), busNo, eightBitAddr, subAddr, myCookie, value, mask);

	// I2C driver expects the addres as a 7 bit value
	addr >>= 1;

	// The simple act of holding the I2C bus acts as a lock for this
	// read-modify-write cycle...

	// Open the bus
    tmpInt1 = (unsigned)busNo;
	if ((retval = fI2CInterface->callPlatformFunction(fSymOpenI2CBus,
			false, (void *)tmpInt1, 0, 0, 0)) != kIOReturnSuccess)
		return(retval);		// bail on error opening bus

	/* this is a fake do..while() loop to aid in easily bailing to the bus
	 * closure call if an error occurs.  Nobody wants to be the guy who uses
	 * goto.
	 */

	do {

		// Use combined mode for read
		// This call always succeeds if the caller is holding the i2c bus
		fI2CInterface->callPlatformFunction(fSymSetCombinedMode, false,
				0, 0, 0, 0);

		// Read the current register state
        tmpInt1 = (unsigned)addr;
		tmpInt2 = (unsigned)subAddr;
		retries = kI2CReadRetryCount;
		do {
			if (retries < kI2CReadRetryCount)
				IOLog("I2CGPIO::doI2CWrite I2C read failed, retrying...\n");

			retval = fI2CInterface->callPlatformFunction(fSymReadI2CBus, false,
					(void *)tmpInt1, (void *)tmpInt2, (void *)&data,
					(void *)1);
			retries--;
		} while ((retval != kIOReturnSuccess) && (retries > 0));

		if (retval != kIOReturnSuccess)
		{
			result = retval;
			break;
		}

		// Apply mask and value
		tmp = (data & ~mask) | (value & mask);

		// prepare the bus - 9554 requires subaddress for write access
		// This call always succeeds if the caller is holding the i2c bus
		fI2CInterface->callPlatformFunction(fSymSetStandardSubMode, false,
				0, 0, 0, 0);

		// write the result
        tmpInt1 = (unsigned)addr;
		tmpInt2 = (unsigned)subAddr;
		retries = kI2CWriteRetryCount;
		do
		{
			if (retries < kI2CWriteRetryCount)
				IOLog("I2CGPIO::doI2CWrite I2C write failed, retrying...\n");

			retval = fI2CInterface->callPlatformFunction(fSymWriteI2CBus, false,
					(void *)tmpInt1, (void *)tmpInt2, (void *)&tmp,
					(void *)1);
			retries--;
		} while ((retval != kIOReturnSuccess) && (retries > 0));

		if (retval != kIOReturnSuccess)
		{
			result = retval;
			break;
		}

	} while(false);

	// Close the bus
	fI2CInterface->callPlatformFunction(fSymCloseI2CBus, false, 0, 0, 0, 0);

#ifdef I2CGPIO_DEBUG
	const char *debug_status;

	if (result == kIOReturnSuccess)
	{
		debug_status = "-I2CGPIO::doI2CWrite(%s) bus:%x addr:%x sub:%x cookie:%u\n";
	}
	else
	{
		debug_status = "-I2CGPIO::doI2CWrite(%s) FAILED!! bus:%x addr:%x sub:%x cookie:%u\n";
	}
#endif

	DLOG(debug_status, getProvider()->getName(), busNo, eightBitAddr, subAddr, myCookie);

	if (result != kIOReturnSuccess)
	{
		IOLog("I2CGPIO::doI2CWrite I2C bus transaction failed!!\n");
	}

	return(result);
}

// Read from a 9554 on the i2c bus
IOReturn I2CGPIO::doI2CRead(UInt8 busNo, UInt8 addr, UInt8 subAddr, UInt8 *value)
{
	IOReturn retval, result = kIOReturnSuccess;
	UInt8 retries, eightBitAddr;
    unsigned tmpInt1, tmpInt2;

#ifdef I2CGPIO_DEBUG
	UInt32 myCookie = fReadCookie++;
#endif

	eightBitAddr = addr;

	DLOG("+I2CGPIO::doI2CRead(%s) bus:%x addr:%x sub:%x cookie:%u\n",
			getProvider()->getName(), busNo, eightBitAddr, subAddr, myCookie);

	// I2C driver wants a 7-bit address
	addr >>= 1;

	// Open the bus
    tmpInt1 = (unsigned)busNo;
	if ((retval = fI2CInterface->callPlatformFunction(fSymOpenI2CBus,
			false, (void *)tmpInt1, 0, 0, 0)) != kIOReturnSuccess)
		return(retval);		// bail on open bus error

	// The PCA9554 uses a combined mode transaction for register reads
	// This call always succeeds if the caller is holding the i2c bus
	fI2CInterface->callPlatformFunction(fSymSetCombinedMode, false, 0, 0, 0, 0);

	// Read the current register state
    tmpInt1 = (unsigned)addr;
    tmpInt2 = (unsigned)subAddr;
	retries = kI2CReadRetryCount;
	do {
		if (retries < kI2CReadRetryCount)
			IOLog("I2CGPIO::doI2CRead I2C read failed, retrying...\n");

		retval = fI2CInterface->callPlatformFunction(fSymReadI2CBus, false,
				(void *)tmpInt1, (void *)tmpInt2, (void *)value,
				(void *)1);
		retries--;
	} while ((retval != kIOReturnSuccess) && (retries > 0));

	if (retval != kIOReturnSuccess) result = retval;

	// Close the bus
	fI2CInterface->callPlatformFunction(fSymCloseI2CBus, false, 0, 0, 0, 0);

#ifdef I2CGPIO_DEBUG
	const char *debug_status;

	if (result == kIOReturnSuccess)
	{
		debug_status = "-I2CGPIO::doI2CRead(%s) bus:%x addr:%x sub:%x cookie:%u value:%x\n";
	}
	else
	{
		debug_status = "-I2CGPIO::doI2CRead(%s) FAILED!! bus:%x addr:%x sub:%x cookie:%u value:%x\n";
	}
#endif

	DLOG(debug_status, getProvider()->getName(), busNo, eightBitAddr, subAddr, myCookie, *value);

	if (result != kIOReturnSuccess)
	{
		IOLog("I2CGPIO::doI2CRead I2C bus transaction failed!!\n");
	}

	return(result);
}

bool I2CGPIO::registerClient(void *param1, void *param2,
		void *param3, void *param4)
{
	UInt32 id;

	//DLOG("I2CGPIO::registerClient %08lx %08lx %08lx %08lx\n",
	//		(UInt32)param1, (UInt32)param2, (UInt32)param3, (UInt32)param4);

	id = (UInt32)param1;

	// Make sure the id is valid and not already registered
	if ((id >= kNumGPIOs) || (fClients[id] != 0)) return(false);

	// Make sure we're registered with pmu-i2c to get interrupts
	if (!registerWithI2C()) return(false);

	if ((fClients[id] =
			(I2CGPIOCallbackInfo *)IOMalloc(sizeof(I2CGPIOCallbackInfo))) == 0)
		return(false);

	// param2 is the client's provider, not needed for i2c-gpio interrupts
	fClients[id]->handler	= (GPIOEventHandler)param3;
	fClients[id]->self		= param4;
	fClients[id]->isEnabled	= false;

	return(true);
}

bool I2CGPIO::unregisterClient(void *param1, void *param2,
		void *param3, void *param4)
{
	UInt32 id, count;
	bool isClient;

	DLOG("I2CGPIO::unregisterClient %08lx %08lx %08lx %08lx\n",
			(UInt32)param1, (UInt32)param2, (UInt32)param3, (UInt32)param4);
	
	id = (UInt32)param1;

	if ((id >= kNumGPIOs) || (fClients[id] == 0))
	{
		return(false);
	}

	IOFree(fClients[id], sizeof(I2CGPIOCallbackInfo));
	fClients[id] = 0;

	// if there are no clients left, unregister with i2c driver
	isClient = false;
	for (count=0; count<kNumGPIOs; count++)
	{
		if (fClients[count] != 0)
		{
			isClient = true;
			break;
		}
	}
	
	if (!isClient) unregisterWithI2C();
	
	return(true);
}

bool I2CGPIO::enableClient(void *param1, void *param2, void *param3,
				void *param4)
{
	UInt32 id = (UInt32)param1;

	if ((id >= kNumGPIOs) || (fClients[id] == 0))
	{
		return(false);
	}

	if (fClients[id]->isEnabled)
	{
		DLOG("I2CGPIO::enableClient events already enabled\n");
	}
	else
	{
		fClients[id]->isEnabled = true;
		DLOG("I2CGPIO::enableClient enabled events\n");
	}
	
	return(true);
}

bool I2CGPIO::disableClient(void *param1, void *param2, void *param3,
				void *param4)
{
	UInt32 id = (UInt32)param1;

	if ((id >= kNumGPIOs) || (fClients[id] == 0))
	{
		return(false);
	}

	if (fClients[id]->isEnabled)
	{
		fClients[id]->isEnabled = false;
		DLOG("I2CGPIO::disableClient disabled events\n");
	}
	else
	{
		DLOG("I2CGPIO::disableClient events already disabled\n");
	}
	
	return(true);
}

/* interrupt event handler
 * This method is called when we get an i2c-related interrupt.
 */

void I2CGPIO::handleEvent(UInt32 addressInfo, UInt32 length, UInt8 *buffer)
{
	UInt8 newState, diff;
	UInt32 value;
	GPIOEventHandler client;
	OSBoolean		*locked = NULL;

	if (length == 0) return;	// panic if there's no data

	newState = *buffer;

	if (newState == fIntRegState)
	{
		// This is possibly a duplicate notification or...  who knows.
		// In any case I can't do anything with it.

		DLOG("I2CGPIO::handleEvent 0x%02x orig = 0x%02x new = 0x%02x, disregarding...\n",
				fI2CAddress, newState, fIntRegState);
		return;
	}

	diff = newState ^ fIntRegState;

	DLOG("I2CGPIO::handleEvent 0x%02x orig = 0x%02x new = 0x%02x xor = 0x%02x\n",
			fI2CAddress, fIntRegState, newState, diff);

	// Store the new value as the last known state
	fIntRegState = newState;

	int bitOffset;
	bitOffset = (fC3Mapping) ? (fBayIndex * 2) : fBayIndex;

	// figure out if one of my own bits changed
	if (diff & (1 << bitOffset))
	{
		DLOG("I2CGPIO 0x%02x got button event\n", fI2CAddress);

		// I got a button event.  Pass it up.
		if ((fClients[kSwitch] == 0) || (!fClients[kSwitch]->isEnabled))
			return;	// nobody registered, or events disabled

		// If the keyswitch is locked, ignore the event
		if (fKeyswitch)
			locked = OSDynamicCast(OSBoolean, fKeyswitch->getProperty("Keyswitch"));

		if (locked != NULL && locked->isTrue())
			return;

		// Put the bit in question in bit position zero and pass it to AppleGPIO.
		value = (UInt32)((newState >> bitOffset) & 0x01);

		// correct for active-low nature of this signal
		value ^= 0x1;

		client = fClients[kSwitch]->handler;
		client((void *)fClients[kSwitch]->self, (void *)value, 0, 0);

	}

	bitOffset = (fC3Mapping) ? ((fBayIndex * 2) + 1) : (4 + fBayIndex);
	if (diff & (1 << bitOffset))
	{
		DLOG("I2CGPIO 0x%02x got insertion/removal event\n", fI2CAddress);

		// I got an insertion event.  pass it up.
		if ((fClients[kPresent] == 0) || (!fClients[kPresent]->isEnabled))
			return; // nobody registered, or events disabled
		
		value = (UInt32)((newState >> (bitOffset)) & 0x01);

		// correct for active-low nature of this signal
		value ^= 0x1;

		client = fClients[kPresent]->handler;
		client((void *)fClients[kPresent]->self, (void *)value, 0, 0);
	}
}

IOReturn I2CGPIO::callPlatformFunction(const OSSymbol *functionName,
                                         bool waitForFunction,
                                         void *param1, void *param2,
                                         void *param3, void *param4)
{
	const char	*functionNameStr;
	UInt8		data = 0, mask, value;
	IOReturn	result = kIOReturnUnsupported;
    unsigned	tmpInt;

	if (functionName == 0) return kIOReturnBadArgument;
	functionNameStr = functionName->getCStringNoCopy();

	DLOG("I2CGPIO::callPlatformFunction %s %s %08lx %08lx %08lx %08lx\n",
	      functionNameStr, waitForFunction ? "TRUE" : "FALSE",
	      (UInt32)param1, (UInt32)param2, (UInt32)param3, (UInt32)param4);

	if (fCurrentPowerState == kI2CGPIOPowerOff)
	{
		result = kIOReturnNotReady;
	}
	else if (strcmp(functionNameStr, kSymGPIOParentIntCapable) == 0)
	{
		*((UInt32 *)param1) = 1;
		result = kIOReturnSuccess;
	}
	else if (strcmp(functionNameStr, kSymGPIOParentWriteGPIO) == 0)
	{
        tmpInt = (unsigned)param2;
		value = (UInt8)tmpInt;
        tmpInt = (unsigned)param3;
		mask  = (UInt8)tmpInt;

		result = doI2CWrite(fI2CBus, fI2CAddress, k9554OutputPort, value, mask);
	}
	else if (strcmp(functionNameStr, kSymGPIOParentReadGPIO) == 0)
	{
		result = doI2CRead(fI2CBus, fI2CAddress, k9554InputPort, &data);

		*(UInt32 *)param2 = (UInt32)data;
	}
	else if (strcmp(functionNameStr, kSymGPIOParentRegister) == 0)
	{
		if (registerClient(param1, param2, param3, param4))
			result = kIOReturnSuccess;
		else
			result = kIOReturnError;
	}
	else if (strcmp(functionNameStr, kSymGPIOParentUnregister) == 0)
	{
		if (unregisterClient(param1, param2, param3, param4))
			result = kIOReturnSuccess;
		else
			result = kIOReturnBadArgument;
	}
	else if (strcmp(functionNameStr, kSymGPIOParentEvtEnable) == 0)
	{
		if (enableClient(param1, param2, param3, param4))
			result = kIOReturnSuccess;
		else
			result = kIOReturnBadArgument;
	}
	else if (strcmp(functionNameStr, kSymGPIOParentEvtDisable) == 0)
	{
		if (disableClient(param1, param2, param3, param4))
			result = kIOReturnSuccess;
		else
			result = kIOReturnBadArgument;
	}
	else
	{
		result = super::callPlatformFunction(functionName, waitForFunction,
					    param1, param2, param3, param4);
	}

	return result;
}

/* act as though this method was never implemented */
IOReturn I2CGPIO::callPlatformFunction( const char *functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 )
{
	return(super::callPlatformFunction(functionName,
			waitForFunction, param1, param2, param3, param4));
}

void I2CGPIO::sI2CEventOccured(I2CGPIO *client, UInt32 addressInfo,
				UInt32 length, UInt8 *buffer)
{
	if (client == 0) return;

	client->handleEvent(addressInfo, length, buffer);
}

bool I2CGPIO::registerWithI2C(void)
{
	// register for interrupts from pmu-i2c and get initial state of
	// interrupt register

	UInt8		tmpBus;
	UInt8		tmpAddr;
	IOReturn	retval;

	DLOG("I2CGPIO::registerWithI2C registering for %08lx...\n", fIntAddrInfo);

	IOLockLock(fRegisteredWithI2CLock);

	if (fRegisteredWithI2C)
	{
		DLOG("I2CGPIO::registerWithI2C already registered\n");
		IOLockUnlock(fRegisteredWithI2CLock);
		return(true);	// already registered
	}

	tmpBus   = (UInt8)(fIntAddrInfo >> 16);
	tmpAddr  = (UInt8)(fIntAddrInfo >> 8);

	if ((retval = doI2CRead(tmpBus, tmpAddr, k9554InputPort, &fIntRegState))
			!= kIOReturnSuccess)
	{
		DLOG("I2CGPIO::registerWithI2C failed to fetch initial state\n");
		IOLockUnlock(fRegisteredWithI2CLock);
		return(false);
	}

	if ((retval = fI2CInterface->callPlatformFunction(fSymRegisterForInts, false,
			(void *)fIntAddrInfo, (void *)&sI2CEventOccured, (void *)this, 0))
			!= kIOReturnSuccess)
	{
		DLOG("I2CGPIO::registerWithI2C failed to register\n");
		IOLockUnlock(fRegisteredWithI2CLock);
		return(false);			
	}
	
	fRegisteredWithI2C = true;

	IOLockUnlock(fRegisteredWithI2CLock);

	DLOG("I2CGPIO::registerWithI2C succeeded\n");

	return(true);
}

void I2CGPIO::unregisterWithI2C(void)
{

	//DLOG("I2CGPIO::unregisterWithI2C unregistering...\n");

	IOLockLock(fRegisteredWithI2CLock);

	if (!fRegisteredWithI2C)
	{
		DLOG("I2CPGIO::unregisterWithI2C not registered\n");
		IOLockUnlock(fRegisteredWithI2CLock);
		return;
	}

	fI2CInterface->callPlatformFunction(fSymDeRegisterForInts, false,
			(void *)fIntAddrInfo, (void *)this, 0, 0);

	fRegisteredWithI2C = false;

	IOLockUnlock(fRegisteredWithI2CLock);
}

IOService *I2CGPIO::createNub( IORegistryEntry * from )
{
	IOService *nub;

    nub = new I2CGPIODevice;

	if (nub && !nub->init( from, gIODTPlane ))
	{
		nub->free();
		nub = 0;
    }

    return(nub);
}

void I2CGPIO::processNub(IOService *myNub)
{
}

void I2CGPIO::publishBelow(IORegistryEntry *root)
{
	OSCollectionIterator	*kids;
	IORegistryEntry			*next;
	IOService				*nub;
	OSData					*compat;
	bool					gpio;
	int						strLen;
	const char				*strStart, *strCur;

	// publish everything below, minus excludeList
	kids = IODTFindMatchingEntries( root, kIODTRecursive | kIODTExclusive, 0);

	if (kids)
	{
		while((next = (IORegistryEntry *)kids->getNextObject()) != 0)
		{
			// make sure "gpio" is listed in the compatible property
			compat = OSDynamicCast(OSData, next->getProperty("compatible"));
			if (!compat)
			{
				DLOG("Failed to get kid's compatible property!!\n");
				continue;
			}

			strLen = compat->getLength();
			strStart = strCur = (const char *)compat->getBytesNoCopy();

			gpio = false;

			while ((strCur - strStart) < strLen)
			{
				if (strcmp(strCur, "gpio") == 0)
				{
					gpio = true;
					break;  // stop iterating on inner loop
				}

				strCur += strlen(strCur) + 1;
			}

			if(!gpio || ((nub = createNub(next)) == 0))
			{
				DLOG("Not creating nub for %s\n", next->getName());
				continue;
			}

			nub->attach( this );
			processNub(nub);
			nub->registerService();
		}
		kids->release();
	}
}

/*------

IOReturn I2CGPIO::powerStateWillChangeTo(IOPMPowerFlags flags,
				unsigned long stateNumber, IOService *whatDevice)
{
	DLOG("I2CGPIO %x powerStateWillChangeTo %u\n", fI2CAddress,
			stateNumber);

	return(IOPMAckImplied);
}
	
IOReturn I2CGPIO::powerStateDidChangeTo(IOPMPowerFlags flags,
				unsigned long stateNumber, IOService *whatDevice)
{
	DLOG("I2CGPIO %x powerStateDidChangeTo %u\n", fI2CAddress,
			stateNumber);

	return(IOPMAckImplied);
}

-----*/

IOReturn I2CGPIO::setPowerState(unsigned long powerStateOrdinal,
				IOService *whatDevice)
{
	DLOG("I2CGPIO::setPowerState current = %u new = %u\n",
		fCurrentPowerState, powerStateOrdinal);

	if ((powerStateOrdinal == fCurrentPowerState) ||
	    (powerStateOrdinal >= kI2CGPIONumPowerStates))
	{
		DLOG("I2CGPIO::setPowerState new state is invalid\n");
	}
	else
	{
		switch (powerStateOrdinal)
		{
			case kI2CGPIOPowerOff:
				doPowerDown();
				break;

			case kI2CGPIOPowerOn:
				doPowerUp();
				break;

			default:
				break;
		}

	}

	DLOG("I2CGPIO::setPowerState %x finished\n", fI2CAddress);
	return(IOPMAckImplied);
}

void I2CGPIO::doPowerDown(void)
{
	DLOG("I2CGPIO::doPowerDown %x\n", fI2CAddress);

	fCurrentPowerState = kI2CGPIOPowerOff;

#if 1
	// Save the output register state
	if (doI2CRead(fI2CBus, fI2CAddress, k9554OutputPort, &fOutputReg) != kIOReturnSuccess)
	{
		// if the read failed, assume all bits are at logic zero
		fOutputReg = 0x00;

		DLOG("I2CGPIO::doPowerDown failed to save output state\n");
	}
#endif
}

void I2CGPIO::doPowerUp(void)
{
	DLOG("I2CGPIO::doPowerUp %x\n", fI2CAddress);

#if 0
	// Zero the output register
	if (doI2CWrite(fI2CBus, fI2CAddress, k9554OutputPort, 0x00 /* value */, 0xFF /* mask */)
			!= kIOReturnSuccess)
	{
		DLOG("I2CGPIO::doPowerUp failed to zero outputs\n");
	}
#endif

#if 1
	// Restore the output register
	if (doI2CWrite(fI2CBus, fI2CAddress, k9554OutputPort, fOutputReg /* value */, 0xFF /* mask */)
			!= kIOReturnSuccess)
	{
		DLOG("I2CGPIO::doPowerUp failed to restore outputs\n");
	}
#endif

	// program the polarity inversion register
	if (doI2CWrite(fI2CBus, fI2CAddress, k9554PolarityInv, fPolarityReg, 0xFF)
			!= kIOReturnSuccess)
	{
		DLOG("I2CGPIO::doPowerUp failed to program polarity inversion reg\n");
	}

	// configure 9554 (direction bits)
	if (doI2CWrite(fI2CBus, fI2CAddress, k9554Config, fConfigReg, 0xFF)
			!= kIOReturnSuccess)
	{
		DLOG("I2CGPIO::doPowerUp failed to configure gpio\n");
	}

	// cache the monitor register's state in case it changed
	if (doI2CRead((UInt8)(fIntAddrInfo >> 16),	// bus
	              (UInt8)(fIntAddrInfo >> 8),	// addr
				  k9554InputPort,				// subaddr
	              &fIntRegState) != kIOReturnSuccess)
	{
		DLOG("I2CGPIO::doPowerUp failed to cache monitor state\n");
	}

	fCurrentPowerState = kI2CGPIOPowerOn;
}

/*---  I2CGPIODevice nub class ---*/

#ifdef super
#undef super
#endif

#define super IOService

OSDefineMetaClassAndStructors(I2CGPIODevice, IOService)

// make sure AppleGPIO will passive match these device nubs using their
// compatible property.
bool I2CGPIODevice::compareName(OSString *name, OSString **matched = 0) const
{
	return(IODTCompareNubName(this, name, matched)
			|| super::compareName(name, matched));
}

