/*
 * Copyright (c) 2004-2007 Apple Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CDriveBayMGPIO.cpp,v 1.4 2007/07/07 19:37:03 raddog Exp $
 *
 *		$Log: IOI2CDriveBayMGPIO.cpp,v $
 *		Revision 1.4  2007/07/07 19:37:03  raddog
 *		[5127355]9A410: Q63: Panic in PowerMac11_2_PlatformPlugin3.0.0d0 during cold boot test.
 *		
 *		Revision 1.3  2005/08/01 23:37:34  galcher
 *		Removed unused variable 'cstr' from ::start.
 *		
 *		Revision 1.2  2005/02/09 02:21:41  jlehrer
 *		Updated interrupt processing for mac-io gpio-16
 *		
 *		Revision 1.1  2004/09/18 00:27:51  jlehrer
 *		Initial checkin
 *		
 *		
 *
 */
 
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include "IOI2CDriveBayMGPIO.h"
#include "IOI2CDriveBayGPIO.h"
#include "GPIOParent.h"
#include "IOPlatformFunction.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
#define IOI2CPCA9554_DEBUG 1

#ifdef IOI2CPCA9554_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define LOCK	IOLockLock(fClientLock)
#define UNLOCK	IOLockUnlock(fClientLock)

#define super IOI2CDevice

OSDefineMetaClassAndStructors(IOI2CDriveBayMGPIO, IOI2CDevice)

bool
IOI2CDriveBayMGPIO::start(IOService *provider)
{
	IOReturn				status = kIOReturnError;
	OSData					*data;
	mach_timespec_t			timeout;

	fSymIntRegister 	= OSSymbol::withCString(kIOPFInterruptRegister);
	fSymIntUnRegister 	= OSSymbol::withCString(kIOPFInterruptUnRegister);
	fSymIntEnable 		= OSSymbol::withCString(kIOPFInterruptEnable);
	fSymIntDisable		= OSSymbol::withCString(kIOPFInterruptDisable);

	// Determine GPIO register mapping...
	// G4 Xserve mapping is PCA9554 compatible. (not supported by this driver :p we only match on PCA9554M)
	// G5 Xserve mapping is PCA9554M compatible.
	fC3Mapping = true;

	// Format the reg property into a client interrupt address info...
	if (0 == (data = OSDynamicCast(OSData, provider->getProperty("reg"))))
	{
		DLOG ("IOI2CDriveBayMGPIO::start - reg property not found\n");
		return false;
	}

	fIntAddrInfo = *((UInt32 *)data->getBytesNoCopy());
	fIntAddrInfo <<= 8;		// shift in subaddress = 0x00
	fIntAddrInfo |= 0x00000100;		// set the i2c read bit

	fConfigReg = 0xFF;		// default to all input pins
	fPolarityReg = 0x00;	// no polarity inversion

	// find out how to program the config register
	if (data = OSDynamicCast(OSData, provider->getProperty("config-reg")))
		fConfigReg = (UInt8)*((UInt32 *)data->getBytesNoCopy());

	// find out how to program the polarity register
	if (data = OSDynamicCast(OSData, provider->getProperty("polarity-reg")))
		fPolarityReg = (UInt8)*((UInt32 *)data->getBytesNoCopy());

	DLOG("IOI2CDriveBayMGPIO@%lx::start fConfigReg = %02x fPolarityReg = %02x\n", fIntAddrInfo, fConfigReg, fPolarityReg);

	fClientCount = 0;
	for (int i = 0; i < kCLIENT_MAX; i++)
		fClient[i].reg = 0x4badbeef;

	// allocate my lock
	if (0 == (fClientLock = IOLockAlloc()))
		return false;

	// Register for mac-io gpio interrupt... Our interrupt.
	// Clients will enable/disable the interrupts as needed.

	if (data = OSDynamicCast(OSData, provider->getProperty("platform-drivebay-sense")))
	{
		DLOG ("IOI2CDriveBayMGPIO::start - got platform-drivebay-sense property!\n");
		if (data = OSDynamicCast(OSData, provider->getProperty("AAPL,phandle")))
		{
			DLOG ("IOI2CDriveBayMGPIO::start - got AAPL,phandle property!\n");
			char cstr[256];
			UInt32 ph;
			ph = *(UInt32 *)data->getBytesNoCopy();
			snprintf( cstr, sizeof(cstr), "platform-drivebay-sense-%08lx", ph );
			if (fDrivebaySenseSym = OSSymbol::withCString(cstr))
			{
				timeout.tv_sec = 30;
				timeout.tv_nsec = 0;

				if (fDrivebaySense = waitForService(resourceMatching(fDrivebaySenseSym), &timeout))
				{
					fDrivebaySense = OSDynamicCast( IOService, fDrivebaySense->getProperty(fDrivebaySenseSym) );

					if (fDrivebaySense)
					{
						status = fDrivebaySense->callPlatformFunction(fDrivebaySenseSym, TRUE, (void *)&IOI2CDriveBayMGPIO::sProcessGPIOInterrupt, (void *)this, (void *)0, (void *)fSymIntRegister);
						DLOG ("IOI2CDriveBayMGPIO::start - \"%s\" service returned: (%x)\n", fDrivebaySenseSym->getCStringNoCopy(), (int)status);
					}
				}
				else
					DLOG ("IOI2CDriveBayMGPIO::start - timeout waiting for \"%s\"\n", fDrivebaySenseSym->getCStringNoCopy());
			}
			else
				DLOG ("IOI2CDriveBayMGPIO::start - could not create platform-drivebay-sense symbol\n");
		}
		else
			DLOG ("IOI2CDriveBayMGPIO::start - AAPL,phandle property not found\n");
	}
	else
		DLOG ("IOI2CDriveBayMGPIO::start - platform-drivebay-sense property not found\n");

	if (status != kIOReturnSuccess)
	{
		DLOG ("IOI2CDriveBayMGPIO::start - PF interrupt provider not found. status=(%x)\n", (int)status);
		return false;
	}

	fFlags |= kFlag_InterruptsRegistered;

/*
	else
	{
		timeout.tv_sec = 30;
		timeout.tv_nsec = 0;

		// Find the ApplePMU...
		if (0 == (fApplePMU = IOService::waitForService(IOService::serviceMatching("ApplePMU"), &timeout)))
		{
			DLOG ("IOI2CDriveBayMGPIO::start - timeout waiting for ApplePMU\n");
			return false;
		}

		if (kIOReturnSuccess != (status = fApplePMU->callPlatformFunction("registerForPMUInterrupts", true,
					(void*)0x01, (void*)sProcessApplePMUInterrupt, (void*)this, NULL )))
		{
			DLOG ("IOI2CDriveBayMGPIO::start -  ApplePMU registerForPMUInterrupts failed: 0x%08x\n", status);
			return false;
		}
	}
*/

	// Publish client interface.
	if (false == super::start(provider))
		return false;

	if (kIOReturnSuccess != readI2C(k9554OutputPort, &fOutputReg, 1))
	{
		DLOG("IOI2CDriveBayGPIO::start -- super::start returned error\n");
		freeI2CResources();
		return false;
	}

	registerService();

	// Get a reference to the KeySwitch driver...
	timeout.tv_sec = 30;
	timeout.tv_nsec = 0;
	fKeyswitch = waitForService(serviceMatching("AppleKeyswitch"), &timeout);

	DLOG("IOI2CDriveBayMGPIO@%lx::start succeeded\n", (fIntAddrInfo >> 8));

	return true;
}

void
IOI2CDriveBayMGPIO::free(void)
{
	UInt32 i;

	DLOG ("IOI2CDriveBayMGPIO::free\n");

	// Disable interrupt source...
	if (fDrivebaySense && fDrivebaySenseSym && fSymIntDisable && fSymIntUnRegister)
	{
		if (fFlags & kFlag_InterruptsEnabled)
			fDrivebaySense->callPlatformFunction(fDrivebaySenseSym, FALSE, (void *)&IOI2CDriveBayMGPIO::sProcessGPIOInterrupt, (void *)this, (void *)0, (void *)fSymIntDisable);
		if (fFlags & kFlag_InterruptsRegistered)
			fDrivebaySense->callPlatformFunction(fDrivebaySenseSym, FALSE, (void *)&IOI2CDriveBayMGPIO::sProcessGPIOInterrupt, (void *)this, (void *)0, (void *)fSymIntUnRegister);
	}
	fFlags = 0;

	// Deallocate clients...
	for (i = 0; i < fClientCount; i++)
		fClient[i].reg = 0x4badbeef;

	if (fClientLock)	{ IOLockFree(fClientLock);	fClientLock = NULL; }

	super::free();
}

IOReturn
IOI2CDriveBayMGPIO::callPlatformFunction(
	const OSSymbol	*functionName,
	bool			waitForFunction,
	void			*param1,
	void			*param2,
	void			*param3,
	void			*param4)
{
	const char		*functionNameStr;

	if (functionName == 0)
		return kIOReturnBadArgument;

	if (0 == (functionNameStr = functionName->getCStringNoCopy()))
		return kIOReturnBadArgument;

	if (0 == strcmp(functionNameStr, "register9554MInterruptClient"))
		return registerClient((UInt32)param1, (PCA9554ClientCallback)param2, (IOService*)param3, (bool)param4);
	else
	if (0 == strcmp(functionNameStr, "enable9554MInterruptClient"))
		return enableClient((UInt32)param1, (UInt32)param4);
	else
	if (0 == strcmp(functionNameStr, kSymGPIOParentIntCapable))
	{
		*((UInt32 *)param1) = 1;
		return kIOReturnSuccess;
	}

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

IOReturn
IOI2CDriveBayMGPIO::registerClient(
	UInt32					reg,
	PCA9554ClientCallback	handler,
	IOService				*client,
	bool					isRegister)
{
	IOReturn				status = kIOReturnSuccess;
	int						i;

	LOCK;

	// Make sure the id is valid and not already registered
	for (i = 0; i < kCLIENT_MAX; i++)
		if (fClient[i].reg == reg)
			break;

	if (isRegister)
	{
		if (i >= kCLIENT_MAX) // make sure we did not find the client
		{
			fClient[fClientCount].reg		= reg;
			fClient[fClientCount].handler	= handler;
			fClient[fClientCount].client	= client;
			fClient[fClientCount].isEnabled	= false;
			DLOG ("IOI2CDriveBayMGPIO::registerClient(%x) client %d added.\n", (int)reg, (int)fClientCount);
			fClientCount++;
		}
		else
		{
			DLOG ("IOI2CDriveBayMGPIO::registerClient(%x) already registered.\n", (int)reg);
		}
	}
	else // unregister client
	{
		if (i < kCLIENT_MAX)
		{
			fClient[i].isEnabled	= false;
			fClient[i].reg			= 0x4badbeef;
			fClient[i].client		= 0;
			fClient[i].handler		= 0;
			DLOG ("IOI2CDriveBayMGPIO::registerClient(%x) client removed.\n", (int)reg);
		}
		else
		{
			DLOG ("IOI2CDriveBayMGPIO::registerClient(%x) unregister not open\n", (int)reg);
			status = kIOReturnNotOpen;
		}
	}
	UNLOCK;

	return status;
}

IOReturn
IOI2CDriveBayMGPIO::enableClient(
	UInt32		reg,
	bool		isEnable)
{
	IOReturn	status = kIOReturnSuccess;
	int			i;

	LOCK;
	for (i = 0; i < kCLIENT_MAX; i++)
		if (fClient[i].reg == reg)
			break;

	if (i < kCLIENT_MAX)
	{
		DLOG ("IOI2CDriveBayMGPIO::enableClient reg:%lx %s\n", reg, isEnable?"enable":"disable");
		fClient[i].isEnabled = isEnable;

		if (isEnable)
		{
			// Enable interrupt source... (if needed)
			if (0 == (fFlags & kFlag_InterruptsEnabled))
			{
				if (kIOReturnSuccess == (status = fDrivebaySense->callPlatformFunction(fDrivebaySenseSym, FALSE, (void *)&IOI2CDriveBayMGPIO::sProcessGPIOInterrupt, (void *)this, (void *)0, (void *)fSymIntEnable)))
					fFlags |= kFlag_InterruptsEnabled;

				DLOG ("IOI2CDriveBayMGPIO::enableClient enable drivebay sense (%x)\n", (int)status);
			}
		}
		else
		{
			// Disable interrupt source... (if needed)
			if (fFlags & kFlag_InterruptsEnabled)
			{
				bool anyClients = 0;
				for (i = 0; i < kCLIENT_MAX; i++)
				{
					if (fClient[i].reg != 0x4badbeef)
					{
						anyClients = true;
						break;
					}
				}

				if ( anyClients == false )
				{
					if (kIOReturnSuccess == (status = fDrivebaySense->callPlatformFunction(fDrivebaySenseSym, FALSE, (void *)&IOI2CDriveBayMGPIO::sProcessGPIOInterrupt, (void *)this, (void *)0, (void *)fSymIntEnable)))
						fFlags &= ~kFlag_InterruptsEnabled;
					DLOG ("IOI2CDriveBayMGPIO::enableClient disable drivebay sense (%x)\n", (int)status);
				}
			}
		}
	}
	else
	{
		DLOG ("IOI2CDriveBayMGPIO::enableClient reg:%x not open\n", (int)reg);
		status = kIOReturnBadArgument;
	}
	UNLOCK;

	return status;
}


void
IOI2CDriveBayMGPIO::processPowerEvent(
	UInt32		eventType)
{
	UInt8	data;

	switch (eventType)
	{
		case kI2CPowerEvent_OFF:
		case kI2CPowerEvent_SLEEP:
			if (kIOReturnSuccess == readI2C(k9554OutputPort, &data, 1))
				fOutputReg = data;
			break;
		case kI2CPowerEvent_ON:
		case kI2CPowerEvent_WAKE:
		{
			// Restore the output register
			if (kIOReturnSuccess != writeI2C(k9554OutputPort, &fOutputReg, 1))
			{
				DLOG("IOI2CDriveBayGPIO::doPowerUp failed to restore outputs\n");
			}

			// program the polarity inversion register
			if (kIOReturnSuccess != writeI2C(k9554PolarityInv, &fPolarityReg, 1))
			{
				DLOG("IOI2CDriveBayGPIO::doPowerUp failed to program polarity inversion reg\n");
			}

			// configure 9554 (direction bits)
			if (kIOReturnSuccess != writeI2C(k9554Config, &fConfigReg, 1))
			{
				DLOG("IOI2CDriveBayGPIO::doPowerUp failed to configure gpio\n");
			}
			break;
		}
	}
}


// ApplePMU calls this (from secondary interrupt context) when the PCA9554M changes state.
// interruptMask should always equal 1.
// length is the number of bytes in the buffer... (always == 5)
// buffer[0] == type? is always = 0x00
// buffer[1] == bus
// buffer[2] == address
// buffer[3] == subAddress
// buffer[4] == 9554M I/O register state

void
IOI2CDriveBayMGPIO::sProcessApplePMUInterrupt(
	IOService	*client,
	UInt8		interruptMask,
	UInt32		length,
	UInt8		*buffer)
{
	IOI2CDriveBayMGPIO *self;

	DLOG("IOI2CDriveBayMGPIO::sProcessApplePMUInterrupt interruptMask:0x%02x length:%ld\n", interruptMask, length);

	if ((interruptMask != 0x01) || (length < 5))
		return;

	if (0 == (self = OSDynamicCast(IOI2CDriveBayMGPIO, client)))
	{
		DLOG("IOI2CDriveBayMGPIO::sProcessApplePMUInterrupt unknown instance type\n");
		return;
	}

	if (self->fIntAddrInfo == (*(UInt32 *)buffer)) // Works on big-endian only.
		self->processGPIOInterrupt(buffer[4]);
}

void
IOI2CDriveBayMGPIO::sProcessGPIOInterrupt(
	IOI2CDriveBayMGPIO		*self,
	void					*param2,
	void					*param3,
	UInt8					newData)
{
	IOReturn status;

	if (0 == OSDynamicCast(IOI2CDriveBayMGPIO, self))
	{
		DLOG("IOI2CDriveBayMGPIO::sProcessGPIOInterrupt unknown instance type\n");
		return;
	}

	UInt8	data;
	if (kIOReturnSuccess == (status = self->readI2C(k9554InputPort, &data, 1)))
		self->processGPIOInterrupt(data);
}

void
IOI2CDriveBayMGPIO::processGPIOInterrupt(
	UInt8		newState)
{
	UInt8		diff;
	unsigned	i;

	if (newState == fOutputReg)
	{
		DLOG("IOI2CDriveBayMGPIO::processGPIOInterrupt state: 0x%02x -> 0x%02x, disregarding...\n", newState, fOutputReg);
		return;
	}

	diff = newState ^ fOutputReg;

	DLOG("IOI2CDriveBayMGPIO::processGPIOInterrupt state: 0x%02x -> 0x%02x, xor = 0x%02x\n", fOutputReg, newState, diff);

	// Store the new value as the last known state
	fOutputReg = newState;

	// If the keyswitch is locked, ignore the event
	if (fKeyswitch)
	{
		OSBoolean *locked;
		locked = OSDynamicCast(OSBoolean, fKeyswitch->getProperty("Keyswitch"));
		if (locked != NULL && locked->isTrue())
			return;
	}

	// Map the interrupt event to each client...
	for (i = 0; i < fClientCount; i++)
	{
		if ( (fClient[i].reg != 0x4badbeef) && (fClient[i].isEnabled) )
		{
			UInt32 bitOffset;
			UInt8 mappedState = 0;
			UInt8 mappedMask = 0;

			bitOffset = (fC3Mapping) ? (i * 2) : i;
			if (diff & (1 << bitOffset))
			{
				if (fOutputReg & (1 << bitOffset))
					mappedState |= (1 << 4);
				mappedMask |= (1 << 4);
			}

			bitOffset = (fC3Mapping) ? ((i * 2) + 1) : (i + 4);
			if (diff & (1 << bitOffset))
			{
				if (fOutputReg & (1 << bitOffset))
					mappedState |= (1 << 3);
				mappedMask |= (1 << 3);
			}

			if (mappedMask)
			{
				DLOG("IOI2CDriveBayMGPIO::processGPIOInterrupt call client %d mask:0x%02x state:0x%02x\n", i, mappedMask, mappedState);
				fClient[i].handler(fClient[i].client, mappedMask, mappedState);
			}
		}
	}
}

/*
// G5 mapping:
drive-power@0
drive-fail@1
drive-inuse@2
drive-present@3 interrupt source
drive-switch@4 interrupt source
drive-3-3V@5
drive-not-used@6
drive-pre-emphasis@7

// 9554M
bay 0 handle
bay 0 present
bay 1 handle
bay 1 present
bay 2 handle
bay 2 present
N/A
N/A


// G4 mapping:
drive-power@0
drive-fail@1
drive-inuse@2
drive-present@3 interrupt source
drive-switch@4 interrupt source
drive-reset@5
drive-power@6
drive-inuse@7

// 9554M
bay 0 handle
bay 1 handle
bay 2 handle
bay 3 handle
bay 0 present
bay 1 present
bay 2 present
bay 3 present
*/

