/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CDriveBayGPIO.cpp,v 1.2 2005/02/09 02:21:40 jlehrer Exp $
 *
 *		$Log: IOI2CDriveBayGPIO.cpp,v $
 *		Revision 1.2  2005/02/09 02:21:40  jlehrer
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
#include "IOI2CDriveBayGPIO.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment for debug info
#define IOI2CDriveBayGPIO_DEBUG 1

#ifdef IOI2CDriveBayGPIO_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define LOCK	IOLockLock(fClientLock)
#define UNLOCK	IOLockUnlock(fClientLock)

#define super IOI2CDevice

OSDefineMetaClassAndStructors(IOI2CDriveBayGPIO, IOI2CDevice)

bool IOI2CDriveBayGPIO::start(IOService *provider)
{
	IOReturn				status;
	OSData					*data;
	mach_timespec_t			timeout;

	DLOG("IOI2CDriveBayGPIO::start\n");

	// Get our "reg" property.. I2C address.
	if (0 == (data = OSDynamicCast(OSData, provider->getProperty("reg"))))
	{
		DLOG("IOI2CDriveBayGPIO::start -- no reg property\n");
		return false;
	}
	fReg = *((UInt32 *)data->getBytesNoCopy());

	// Find the PCA9554M node. It provides our interrupt source...
	timeout.tv_sec = 30;
	timeout.tv_nsec = 0;
	if (0 == (fPCA9554M = waitForService(serviceMatching("IOI2CDriveBayMGPIO"), &timeout)))
	{
		DLOG("IOI2CDriveBayGPIO::start -- timeout waiting for IOI2CDriveBayMGPIO\n");
		return false;
	}

	fConfigReg = 0xFF;		// default to all input pins
	fPolarityReg = 0x00;	// no polarity inversion

	// find out how to program the config register
	if (data = OSDynamicCast(OSData, provider->getProperty("config-reg")))
		fConfigReg = (UInt8)*((UInt32 *)data->getBytesNoCopy());

	// find out how to program the polarity register
	if (data = OSDynamicCast(OSData, provider->getProperty("polarity-reg")))
		fPolarityReg = (UInt8)*((UInt32 *)data->getBytesNoCopy());

	DLOG("IOI2CDriveBayGPIO(%x)::start fConfigReg = %02x fPolarityReg = %02x\n", (int)fReg, fConfigReg, fPolarityReg);

	// allocate my lock
	fClientLock = IOLockAlloc();

	// Register with the combined PCA9554M
	DLOG("IOI2CDriveBayGPIO(%x)::start - register9554MInterruptClient\n", (int)fReg);
	if (kIOReturnSuccess != (status = fPCA9554M->callPlatformFunction("register9554MInterruptClient", false,
				(void *)fReg, (void *)&sProcess9554MInterrupt, (void *)this, (void *)true)))
	{
		DLOG("IOI2CDriveBayGPIO(%x)::start failed to register (%x)\n", (int)fReg, (int)status);
		return false;
	}

	// Start IOI2C services...
	if (false == super::start(provider))
	{
		DLOG("IOI2CDriveBayGPIO(%x)::start -- super::start returned error\n", (int)fReg);
		return false;
	}

	super::callPlatformFunction("IOI2CSetDebugFlags", false, (void *)kStateFlags_kprintf, (void *)true, (void *)NULL, (void *)NULL);

	if (kIOReturnSuccess != readI2C(k9554OutputPort, &fOutputReg, 1))
	{
		DLOG("IOI2CDriveBayGPIO(%x)::start -- super::start returned error\n", (int)fReg);
		freeI2CResources();
		return false;
	}

	// start matching on children
	publishChildren(provider);
	registerService();

	DLOG("IOI2CDriveBayGPIO@%lx::start succeeded\n", getI2CAddress());

	return true;
}

void IOI2CDriveBayGPIO::stop(IOService *provider)
{
	UInt32 i;

	DLOG("IOI2CDriveBayGPIO::stop\n");
	
	for (i = 0; i < kNumGPIOs; i++)
	{
		fClient[i].isEnabled = false;
		fClient[i].self = 0;
		fClient[i].handler = 0;
	}

	// Un-Register with the combined PCA9554M
	if (fPCA9554M)
		fPCA9554M->callPlatformFunction("register9554MInterruptClient", false,
				(void *)fReg, (void *)&sProcess9554MInterrupt, (void *)this, (void *)false);

	if (fClientLock)	{ IOLockFree(fClientLock);		fClientLock = 0; }

    super::stop(provider);
}

void IOI2CDriveBayGPIO::free(void)
{
	DLOG("IOI2CDriveBayGPIO::free\n");
    super::free();
}


IOReturn IOI2CDriveBayGPIO::publishChildren(IOService *provider)
{
    IOReturn 			status = kIOReturnSuccess;
	OSIterator			*iter;
	IORegistryEntry		*next;
    IOService			*nub;

	if (iter = provider->getChildIterator(gIODTPlane))
	{
		// Iterate through children and create nubs
		while (next = (IORegistryEntry *)(iter->getNextObject()))
		{
			nub = OSDynamicCast(IOService, OSMetaClass::allocClassWithName("IOI2CService"));
			if (nub)
			{
				nub->init(next, gIODTPlane);
				nub->attach(this);
				nub->registerService();
			
//				DLOG("IOI2CDriveBayGPIO@%lx::publishChildren published child %s\n", getI2CAddress(), next->getName());
			}
		}

		iter->release();
	}

	return status;
}

IOReturn IOI2CDriveBayGPIO::callPlatformFunction(
	const OSSymbol *functionName,
	bool waitForFunction,
	void *param1, void *param2,
	void *param3, void *param4)
{
	const char	*functionNameStr;
	UInt8		data;
	IOReturn	status;

	if (functionName == 0)
		return kIOReturnBadArgument;

	functionNameStr = functionName->getCStringNoCopy();

	if (0 == strcmp(functionNameStr, kSymGPIOParentWriteGPIO))
	{
		return readModifyWriteI2C(k9554OutputPort, (UInt8)(UInt32)param2, (UInt8)(UInt32)param3);
	}
	else
	if (0 == strcmp(functionNameStr, kSymGPIOParentReadGPIO))
	{
		if (kIOReturnSuccess == (status = readI2C(k9554InputPort, &data, 1)))
			*(UInt32 *)param2 = (UInt32)data;
		return status;
	}
	else // GPIO interrupt client API...
	if (0 == strcmp(functionNameStr, kSymGPIOParentRegister))
		return registerGPIOClient((UInt32)param1, (GPIOEventHandler)param3, (IOService*)param4, true);
	else
	if (0 == strcmp(functionNameStr, kSymGPIOParentUnregister))
		return registerGPIOClient((UInt32)param1, (GPIOEventHandler)0, (IOService*)0, false);
	else
	if (0 == strcmp(functionNameStr, kSymGPIOParentEvtEnable))
		return enableGPIOClient((UInt32)param1, true);
	else
	if (0 == strcmp(functionNameStr, kSymGPIOParentEvtDisable))
		return enableGPIOClient((UInt32)param1, false);
	else
	if (0 == strcmp(functionNameStr, kSymGPIOParentIntCapable))
	{
		*((UInt32 *)param1) = 1;
		return kIOReturnSuccess;
	}

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

IOReturn
IOI2CDriveBayGPIO::readModifyWriteI2C(
	UInt8		subAddr,
	UInt8		value,
	UInt8		mask)
{
	IOReturn	status;
	UInt32		clientKey;
	UInt8		data;

	if (kIOReturnSuccess == (status = lockI2CBus(&clientKey)))
	{
		if (kIOReturnSuccess != (status = readI2C(subAddr, &data, 1, clientKey)))
		{
			DLOG("IOI2CDriveBayGPIO@%lx::rmw read S:0x%x error: 0x%08x\n", getI2CAddress(), subAddr, status);
		}
		else
		{
			// Apply mask and value
			data = ((data & ~mask) | (value & mask));

			if (kIOReturnSuccess != (status = writeI2C(subAddr, &data, 1, clientKey)))
			{
				DLOG("IOI2CDriveBayGPIO@%lx::rmw write S:0x%x error: 0x%08x\n", getI2CAddress(), subAddr, status);
			}
		}
		unlockI2CBus(clientKey);
	}

	return status;
}

IOReturn
IOI2CDriveBayGPIO::registerGPIOClient(
	UInt32				id,
	GPIOEventHandler	handler,
	IOService			*client,
	bool				isRegister)
{
	IOReturn status = kIOReturnSuccess;

	// Make sure the id is valid and not already registered
	if ( (id >= kNumGPIOs) || (handler == 0) )
		return kIOReturnBadArgument;

	LOCK;
	if (isRegister)
	{
		if (fClient[id].self != 0)
		{
			DLOG ("IOI2CDriveBayGPIO@%lx::registerGPIOClient id:%d already open\n", getI2CAddress(), id);
			status = kIOReturnStillOpen;
		}
		else
		{
			fClient[id].handler		= handler;
			fClient[id].isEnabled	= false;
			fClient[id].self		= client;
			DLOG ("IOI2CDriveBayGPIO@%lx::registerGPIOClient id:%d registered\n", getI2CAddress(), id);
		}
	}
	else // unregister
	{
		if (fClient[id].self == 0)
		{
			DLOG ("IOI2CDriveBayGPIO@%lx::registerGPIOClient - unregister id:%d not open\n", getI2CAddress(), id);
			status = kIOReturnNotOpen;
		}
		else
		{
			fClient[id].self = 0;
			fClient[id].isEnabled = false;
			fClient[id].handler = 0;
			DLOG ("IOI2CDriveBayGPIO@%lx::registerGPIOClient id:%d un-registered\n", getI2CAddress(), id);
		}
	}
	UNLOCK;

	return status;
}

IOReturn
IOI2CDriveBayGPIO::enableGPIOClient(
	UInt32		id,
	bool		isEnable)
{
	IOReturn status = kIOReturnSuccess;

	if ( (id >= kNumGPIOs) || (fClient[id].self == 0) )
	{
		DLOG("IOI2CDriveBayGPIO@%lx::enableClient - error %d not registered\n", getI2CAddress(), id);
		return kIOReturnBadArgument;
	}

	if (fClient[id].isEnabled == isEnable)
		return kIOReturnSuccess;

	LOCK;

	DLOG("IOI2CDriveBayGPIO@%lx::enableGPIOClient %s client %d\n", getI2CAddress(), isEnable?"enable":"disable", id);

	fClient[id].isEnabled = isEnable;
	if (isEnable)
	{
		if (fClientsEnabled == 0) // Register with the combined PCA9554M
			status = fPCA9554M->callPlatformFunction("enable9554MInterruptClient", false, (void *)fReg, (void *)0, (void *)0, (void *)true);

		fClientsEnabled |= (1 << id);
	}
	else // disable
	{
		fClientsEnabled &= ~(1 << id);
		if (fClientsEnabled == 0) // Un-Register with the combined PCA9554M
			status = fPCA9554M->callPlatformFunction("enable9554MInterruptClient", false, (void *)fReg, (void *)0, (void *)0, (void *)false);
	}

	DLOG("IOI2CDriveBayGPIO@%lx::enableGPIOClient %s client %d (%x)\n", getI2CAddress(), isEnable?"enable":"disable", id, (int)status);

	UNLOCK;

	return kIOReturnSuccess;
}

void
IOI2CDriveBayGPIO::sProcess9554MInterrupt(
	IOI2CDriveBayGPIO	*self,
	UInt8			eventMask,
	UInt8			newState)
{
	if (self)
		self->process9554MInterrupt(eventMask, newState);
}

void
IOI2CDriveBayGPIO::process9554MInterrupt(
	UInt8			eventMask,
	UInt8			newState)
{
	UInt8			value;

	if (eventMask & (1 << kSwitch))
	{
		DLOG("IOI2CDriveBayGPIO 0x%02x got switch event\n", getI2CAddress());

		// I got a button event.  Pass it up.
		if (fClient[kSwitch].self && fClient[kSwitch].handler && fClient[kSwitch].isEnabled)
		{
			value = ((newState >> kSwitch) & 1);
			value ^= 0x1; // correct for active-low nature of this signal
			fClient[kSwitch].handler((void *)fClient[kSwitch].self, (void *)(UInt32)value, 0, 0);
		}
	}

	if (eventMask & (1 << kPresent))
	{
		DLOG("IOI2CDriveBayGPIO 0x%02x got insertion/removal event\n", getI2CAddress());

		// I got a insertion event.  Pass it up.
		if (fClient[kPresent].self && fClient[kPresent].handler && fClient[kPresent].isEnabled)
		{
			value = ((newState >> kPresent) & 1);
			value ^= 0x1; // correct for active-low nature of this signal
			fClient[kPresent].handler((void *)fClient[kPresent].self, (void *)(UInt32)value, 0, 0);
		}
	}
}


void
IOI2CDriveBayGPIO::processPowerEvent(
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

