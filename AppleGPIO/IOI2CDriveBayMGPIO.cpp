/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CDriveBayMGPIO.cpp,v 1.1 2004/09/18 00:27:51 jlehrer Exp $
 *
 *		$Log: IOI2CDriveBayMGPIO.cpp,v $
 *		Revision 1.1  2004/09/18 00:27:51  jlehrer
 *		Initial checkin
 *		
 *		
 *
 */
 
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include "IOI2CDriveBayMGPIO.h"
#include "GPIOParent.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
//#define IOI2CPCA9554_DEBUG 1

#ifdef IOI2CPCA9554_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif


#define super IOService

OSDefineMetaClassAndStructors(IOI2CDriveBayMGPIO, IOService)

bool
IOI2CDriveBayMGPIO::start(IOService *provider)
{
	IOReturn				status;
	OSData					*data;
	mach_timespec_t			timeout;
	const char				*cstr;

	if (false == super::start(provider))
		return false;

	// Determine GPIO register mapping...
	// G4 Xserve mapping is PCA9554 compatible. (not supported by this driver :p we only match on PCA9554M)
	// G5 Xserve mapping is PCA9554M compatible.
	if (data = OSDynamicCast(OSData, provider->getProperty("compatible")))
		if (cstr = (const char *)data->getBytesNoCopy())
			if (0 == strcmp(cstr, "PCA9554M"))
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

	// Get a count of 9554 clients...
	if (0 == (data = OSDynamicCast(OSData, provider->getProperty("i2c-combined"))))
	{
		DLOG ("IOI2CDriveBayMGPIO::start - combined property not found\n");
		return false;
	}

	if (0 == (fClientCount = data->getLength() / sizeof(UInt32)))
	{
		DLOG ("IOI2CDriveBayMGPIO::start - combined property count == 0\n");
		return false;
	}

	if (0 == (fClient = (PCA9554CallbackInfo **)IOMalloc(fClientCount * sizeof(PCA9554CallbackInfo *))))
	{
		DLOG ("IOI2CDriveBayMGPIO::start - malloc %d clients failed\n", fClientCount);
		return false;
	}

	bzero(fClient, (fClientCount * sizeof(PCA9554CallbackInfo *)));

	// allocate my lock
	if (0 == (fClientLock = IOLockAlloc()))
		return false;

	// Find the ApplePMU...
	timeout.tv_sec = 30;
	timeout.tv_nsec = 0;
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

	if (fClient)
	{
		for (i = 0; i < fClientCount; i++)
		{
			if (fClient[i])
			{
				IOFree((void *)fClient[i], sizeof(PCA9554CallbackInfo));
				fClient[i] = 0;
			}
		}
		IOFree((void *)fClient, fClientCount * sizeof(PCA9554CallbackInfo *));
		fClient = 0;
	}

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
	UInt32				id,
	PCA9554ClientCallback	handler,
	IOService			*client,
	bool				isRegister)
{
	PCA9554CallbackInfo	*clientInfo;

	DLOG ("IOI2CDriveBayMGPIO::registerClient id:%d %s\n", id, isRegister?"":"unregister");

	// Make sure the id is valid and not already registered
	if (id >= fClientCount)
		return kIOReturnBadArgument;

	if (isRegister)
	{
		if (fClient[id] != 0)
		{
			DLOG ("IOI2CDriveBayMGPIO::registerClient id:%d already open\n", id);
			return kIOReturnStillOpen;
		}

		if (0 == (clientInfo = (PCA9554CallbackInfo *)IOMalloc(sizeof(PCA9554CallbackInfo))))
		{
			DLOG ("IOI2CDriveBayMGPIO::registerClient id:%d malloc err\n", id);
			return kIOReturnNoMemory;
		}

		clientInfo->handler		= handler;
		clientInfo->client		= client;
		clientInfo->isEnabled	= false;
		fClient[id] = clientInfo;
	}
	else // unregister client
	{
		if ((fClient[id] == 0) || (fClient[id]->client != client) || (fClient[id]->handler != handler))
		{
			DLOG ("IOI2CDriveBayMGPIO::registerClient unregister id:%d not open\n", id);
			return kIOReturnNotOpen;
		}

		clientInfo = fClient[id];
		fClient[id] = 0;

		IOFree(clientInfo, sizeof(PCA9554CallbackInfo));
	}

	return kIOReturnSuccess;
}

IOReturn
IOI2CDriveBayMGPIO::enableClient(
	UInt32		id,
	bool		isEnable)
{
	if ((id >= fClientCount) || (fClient[id] == 0))
	{
		DLOG ("IOI2CDriveBayMGPIO::enableClient id:%d not open\n", id);
		return kIOReturnBadArgument;
	}

	DLOG ("IOI2CDriveBayMGPIO::enableClient id:%d %s\n", id, isEnable?"enable":"disable");
	fClient[id]->isEnabled = isEnable;

	return kIOReturnSuccess;
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

	DLOG("IOI2CDriveBayMGPIO::sProcessApplePMUInterrupt interruptMask:0x%02x length:%d\n", interruptMask, length);

	if ((interruptMask != 0x01) || (length < 5))
		return;

	if (0 == (self = OSDynamicCast(IOI2CDriveBayMGPIO, client)))
	{
		DLOG("IOI2CDriveBayMGPIO::sProcessApplePMUInterrupt unknown instance type\n");
		return;
	}

	if (self->fIntAddrInfo == (*(UInt32 *)buffer)) // Works on big-endian only.
		self->processApplePMUInterrupt(buffer[4]);
}

void
IOI2CDriveBayMGPIO::processApplePMUInterrupt(
	UInt8		newState)
{
	UInt8		diff;
	unsigned	i;

	if (newState == fIntRegState)
	{
		DLOG("IOI2CDriveBayMGPIO::processApplePMUInterrupt state: 0x%02x -> 0x%02x, disregarding...\n", newState, fIntRegState);
		return;
	}

	diff = newState ^ fIntRegState;

	DLOG("IOI2CDriveBayMGPIO::processApplePMUInterrupt state: 0x%02x -> 0x%02x, xor = 0x%02x\n", fIntRegState, newState, diff);

	// Store the new value as the last known state
	fIntRegState = newState;

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
		if (fClient[i])
		{
			UInt32 bitOffset;
			UInt8 mappedState = 0;
			UInt8 mappedMask = 0;

			bitOffset = (fC3Mapping) ? (i * 2) : i;
			if (diff & (1 << bitOffset))
			{
				if (fIntRegState & (1 << bitOffset))
					mappedState |= (1 << 4);
				mappedMask |= (1 << 4);
			}

			bitOffset = (fC3Mapping) ? ((i * 2) + 1) : (i + 4);
			if (diff & (1 << bitOffset))
			{
				if (fIntRegState & (1 << bitOffset))
					mappedState |= (1 << 3);
				mappedMask |= (1 << 3);
			}

			if (mappedMask)
			{
				DLOG("IOI2CDriveBayMGPIO::processApplePMUInterrupt call client %d mask:0x%02x state:0x%02x\n", i, mappedMask, mappedState);
				fClient[i]->handler(fClient[i]->client, mappedMask, mappedState);
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

