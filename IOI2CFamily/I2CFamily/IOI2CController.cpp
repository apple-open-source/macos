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
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CController.cpp,v 1.8 2005/07/01 16:09:52 bwpang Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CController.cpp,v $
 *		Revision 1.8  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.7  2004/12/15 02:02:15  jlehrer
 *		[3905559] Disable mac-io/i2c power management for AOA.
 *		[3917744,3917697] Added newUserClient method.
 *		
 *		Revision 1.6  2004/11/04 20:16:35  jlehrer
 *		Added isRemoving argument to registerPowerStateInterest method.
 *		
 *		Revision 1.5  2004/09/28 01:45:26  jlehrer
 *		[3814717] Check for initialized before calling PMstop.
 *		
 *		Revision 1.4  2004/09/17 20:35:41  jlehrer
 *		Removed APSL headers.
 *		Added code to modify the device-tree to the i2c/i2c-bus model.
 *		Added DLOGPWR to log power state changes.
 *		Clean up unused code fragments.
 *		
 *		Revision 1.3  2004/07/03 00:07:05  jlehrer
 *		Added support for dynamic max-i2c-data-length.
 *		
 *		Revision 1.2  2004/06/08 23:45:15  jlehrer
 *		Added ERRLOG, disabled DLOG, changed DLOGI2C to use runtime cmd.option flag.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOUserClient.h>

#include "IOI2CController.h"
#include "IOI2CService.h"
#include "IOI2CDefs.h"

//#define I2C_DEBUG 1

#if (defined(I2C_DEBUG) && I2C_DEBUG)
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define I2C_ERRLOG 1

#if (defined(I2C_ERRLOG) && I2C_ERRLOG)
#define ERRLOG(fmt, args...)  do{IOLog(fmt, ## args);kprintf(fmt, ## args);}while(0)
#else
#define ERRLOG(fmt, args...)
#endif

#define I2C_DLOGPWR 1

#if (defined(I2C_DLOGPWR) && I2C_DLOGPWR)
#define DLOGPWR(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOGPWR(fmt, args...)
#endif

#define I2C_DEBUG_VERBOSE 1

#if (defined(I2C_DEBUG_VERBOSE) && I2C_DEBUG_VERBOSE)
#define DLOGI2C(opt, fmt, args...)	do{if(opt&kI2COption_VerboseLog)kprintf(fmt, ## args);}while(0)
#else
#define DLOGI2C(opt, fmt, args...)
#endif

// Define kUSE_IOLOCK for IOLock, Undefine for semaphore
//#define kUSE_IOLOCK 0
#ifdef kUSE_IOLOCK
	#define I2CLOCK		IOLockLock(fClientLock)
	#define I2CUNLOCK	{IOLockUnlock(fClientLock);IOSleep(0);}
	// IOSleep lets other threads have a chance to run.
#else
	#define I2CLOCK		semaphore_wait(fClientSem)
	#define I2CUNLOCK	semaphore_signal(fClientSem)
#endif


#pragma mark  
#pragma mark *** IOI2CController class ***
#pragma mark  

#define super IOService
OSDefineMetaClassAndAbstractStructors( IOI2CController, IOService )

bool
IOI2CController::start(
	IOService	*provider)
{
	IOReturn	status;
	OSData		*data;

	DLOG("+IOI2CController::start\n");

	if (false == super::start(provider))
	{
		ERRLOG("-IOI2CController::start super::start failed\n");
		return false;
	}

	fProvider = provider;
	if (kIOReturnSuccess != (status = initI2CResources()))
	{
		ERRLOG("-IOI2CController::start initI2CResources returned: 0x%lx\n", (UInt32)status);
		freeI2CResources();
		return false;
	}

	if (data = OSDynamicCast(OSData, provider->getProperty("max-i2c-data-length")))
		fMaxI2CDataLength = *((UInt32*)data->getBytesNoCopy());
	else
		fMaxI2CDataLength = 4;
	
	// Determine if this controller is configured for multi-bus or single-bus operation...
	if (data = OSDynamicCast(OSData, provider->getProperty("AAPL,i2c-bus")))
	{
		fI2CBus = *((UInt32*)data->getBytesNoCopy());	// single-bus: this is the bus number used for all transactions.
		DLOG("IOI2CController::start found \"AAPL,i2c-bus\" property:0x%lx\n",fI2CBus);
	}
	else
		fI2CBus = kIOI2CMultiBusID;						// multi-bus: bus number is set by child i2c-bus node drivers.


	// If the fDisablePowerManagement flag is set?
	if (fDisablePowerManagement)
	{
		// Do not initialize power management and allow all transactions until the system shuts down.
		// Set a flag in each device nub to disable its power management also.
		OSIterator		*busIter, *devIter;
		IORegistryEntry	*bus, *dev;

		if (busIter = fProvider->getChildIterator(gIODTPlane))
		{
			while (bus = OSDynamicCast(IORegistryEntry, busIter->getNextObject()))
			{
				if (devIter = bus->getChildIterator(gIODTPlane))
				{
					while (dev = OSDynamicCast(IORegistryEntry, devIter->getNextObject()))
						dev->setProperty("AAPL,no-power", true);
					devIter->release();
				}
			}
			busIter->release();
		}
		
		DLOGPWR("Controller Is Usable\n-------------------------------\n");
		fDeviceIsUsable = TRUE;
	}
	else
		InitializePowerManagement();

	DLOG("-IOI2CController::start\n");
	return true;
}

void
IOI2CController::stop (
	IOService	*provider)
{
	DLOG("IOI2CController::stop\n");
	fDeviceIsUsable = FALSE;
	if (initialized)
		PMstop();

	if (fIOSyncThreadCall)
	{
		thread_call_cancel(fIOSyncThreadCall);
		thread_call_free(fIOSyncThreadCall);
		fIOSyncThreadCall = 0;
	}

	super::stop(provider);
}

void IOI2CController::free ( void )
{
	freeI2CResources();
	super::free();
}

#pragma mark  
#pragma mark *** I2C resource init and cleanup... ***
#pragma mark  

IOReturn
IOI2CController::initI2CResources(void)
{
//	if (0 == (reserved = (ExpansionData *)IOMalloc(sizeof(struct ExpansionData))))
//		return kIOReturnNoMemory;

	// Create some symbols for later use
	symLockI2CBus = OSSymbol::withCStringNoCopy(kLockI2Cbus);
	symUnlockI2CBus = OSSymbol::withCStringNoCopy(kUnlockI2Cbus);
	symWriteI2CBus = OSSymbol::withCStringNoCopy(kWriteI2Cbus);
	symReadI2CBus = OSSymbol::withCStringNoCopy(kReadI2Cbus);
	symPowerInterest = OSSymbol::withCStringNoCopy("IOI2CPowerStateInterest");
	symPowerClient = OSSymbol::withCStringNoCopy("client");
	symPowerAcked = OSSymbol::withCStringNoCopy("acked");
	symGetMaxI2CDataLength = OSSymbol::withCStringNoCopy(kIOI2CGetMaxI2CDataLength);

	if (!symLockI2CBus || !symUnlockI2CBus || !symWriteI2CBus || !symReadI2CBus ||
		!symPowerInterest || !symPowerClient || !symPowerAcked || !symGetMaxI2CDataLength)
		return kIOReturnNoMemory;

#ifdef kUSE_IOLOCK
	if (NULL == (fClientLock = IOLockAlloc()))
		return kIOReturnNoMemory;
#else
	IOReturn status;
	if (kIOReturnSuccess != (status = semaphore_create(current_task(), (semaphore**)&fClientSem, SYNC_POLICY_FIFO, 1)))
		return status;
#endif

	if (NULL == (fPowerLock = IOLockAlloc()))
		return kIOReturnNoMemory;

	fClientLockKey = kIOI2C_CLIENT_KEY_VALID;

	return kIOReturnSuccess;
}

void
IOI2CController::freeI2CResources(void)
{
	fDeviceIsUsable = FALSE;
	if (initialized)
		PMstop();
	if (fIOSyncThreadCall)
	{
		thread_call_cancel(fIOSyncThreadCall);
		thread_call_free(fIOSyncThreadCall);
		fIOSyncThreadCall = 0;
	}

	if (fPowerLock)				{ IOLockFree(fPowerLock);			fPowerLock = 0; }
#ifdef kUSE_IOLOCK
	if (fClientLock)			{ IOLockFree(fClientLock);			fClientLock = 0; }
#else
	if (fClientSem)				{ semaphore_destroy(current_task(), fClientSem);	fClientSem = 0; }
#endif
	if (symLockI2CBus)			{ symLockI2CBus->release();			symLockI2CBus = 0; }
	if (symUnlockI2CBus)		{ symUnlockI2CBus->release();		symUnlockI2CBus = 0; }
	if (symWriteI2CBus)			{ symWriteI2CBus->release();		symWriteI2CBus = 0; }
	if (symReadI2CBus)			{ symReadI2CBus->release();			symReadI2CBus = 0; }
	if (symPowerInterest)		{ symPowerInterest->release();		symPowerInterest = 0; }
	if (symPowerClient)			{ symPowerClient->release();		symPowerClient = 0; }
	if (symPowerAcked)			{ symPowerAcked->release();			symPowerAcked = 0; }
	if (symGetMaxI2CDataLength)	{ symGetMaxI2CDataLength->release(); symGetMaxI2CDataLength = 0; }
//	if (reserved)	{		IOFree(reserved, sizeof(struct ExpansionData));		reserved = 0;	}
}


IOReturn
IOI2CController::publishChildren(void)
{
	OSIterator		*iter;
	IORegistryEntry	*next;
	IOService		*nub;

    // publish children...
	if (iter = fProvider->getChildIterator(gIODTPlane))
	{
		while (next = OSDynamicCast(IORegistryEntry, iter->getNextObject()))
		{
			if (nub = new IOI2CService) //OSDynamicCast(IOService, OSMetaClass::allocClassWithName(name)))
			{
				if (nub->init(next, gIODTPlane))
				{
					nub->attach(this);
					nub->registerService();
				}
				else
					nub->free();
			}
		}
		iter->release();
	}
	return kIOReturnSuccess;
}

#pragma mark  
#pragma mark *** Power Management ***
#pragma mark  

/*******************************************************************************
 * Power Management Initialization
 * Power state info:
 * IOPMPowerFlags	capabilityFlags;	// bits that describe (to interested drivers) the capability of the device in this state 
 * IOPMPowerFlags	outputPowerCharacter;	// description (to power domain children) of the power provided in this state 
 * IOPMPowerFlags	inputPowerRequirement;	// description (to power domain parent) of input power required in this state
 *******************************************************************************/

IOReturn
IOI2CController::InitializePowerManagement(void)
{
	IOReturn	status;
	static const IOPMPowerState ourPowerStates[kIOI2CPowerState_COUNT] = 
	{
	//	version	capabilityFlags			outputPowerCharacter	inputPowerRequirement
		{1,		0,						0,						0,				0, 0, 0, 0, 0, 0, 0, 0},
		{1,		kIOPMSleepCapability,	kIOPMSleep,				kIOPMSleep,		0, 0, 0, 0, 0, 0, 0, 0},
		{1,		kIOPMDeviceUsable,		kIOPMDoze,				kIOPMDoze,		0, 0, 0, 0, 0, 0, 0, 0},
		{1,		kIOPMDeviceUsable,		kIOPMPowerOn,			kIOPMPowerOn,	0, 0, 0, 0, 0, 0, 0, 0}
	};

	// Initialize Power Management superclass variables from IOService.h
	PMinit();

	// Join the Power Management tree from IOService.h
	fProvider->joinPMtree( this);

	// Register ourselves as the power controller.
	if (kIOReturnSuccess != (status = registerPowerDriver( this, (IOPMPowerState *) ourPowerStates, kIOI2CPowerState_COUNT )))
	{
		ERRLOG("%s Failed to registerPowerDriver.\n", getName());
		return status;
	}

	changePowerStateTo(kIOI2CPowerState_ON);

	// Create a thread call for synchronizing IO transactions before powerdown and restarts.
	if (NULL == (fIOSyncThreadCall = thread_call_allocate(&IOI2CController::sIOSyncCallback, (thread_call_param_t) this)))
		return kIOReturnNoResources;

	// Install power change handler (for restart notification)
	if (NULL == registerPrioritySleepWakeInterest(&sSysPowerDownHandler, this))
		return kIOReturnNoResources;

	return kIOReturnSuccess;
}

/*******************************************************************************
 * Power Management callbacks and handlers
 *******************************************************************************/

IOReturn
IOI2CController::setPowerState(
	unsigned long	newPowerState,
	IOService		*target)
{
	DLOGPWR("\n----set power------------------\n");
	DLOGPWR("IOI2CController::setPowerState called with state:%lu\n", newPowerState);

	if (fCurrentPowerState == newPowerState)
		return IOPMAckImplied;

	switch (newPowerState)
	{
		case kIOI2CPowerState_ON:
		case kIOI2CPowerState_DOZE:
			DLOGPWR("Controller Is Usable\n-------------------------------\n");
			fDeviceIsUsable = TRUE;
			break;

		case kIOI2CPowerState_SLEEP:
		case kIOI2CPowerState_OFF:
			fDeviceIsUsable = FALSE;
			DLOGPWR("Controller Is Unusable\n-------------------------------\n");
			break;
	}

	fCurrentPowerState = newPowerState;

	return IOPMAckImplied;
}

IOReturn
IOI2CController::sSysPowerDownHandler(
	void		*target,
	void		*refCon,
	UInt32		messageType,
	IOService	*service,
	void		*messageArgument,
	vm_size_t	argSize)
{
	IOReturn	status = kIOReturnUnsupported;
    IOPowerStateChangeNotification	*params;
	IOI2CController *self;

	DLOGPWR("\n---------------------------------------------------------------------------------------------\n");
	if (self = OSDynamicCast(IOI2CController, (OSMetaClassBase *)target))
	{
		switch (messageType)
		{
			case kIOMessageSystemWillSleep: // iokit_common_msg(0x280)
				break;

			case kIOMessageSystemWillPowerOff: // iokit_common_msg(0x250)
			case kIOMessageSystemWillRestart: // iokit_common_msg(0x310)
				DLOGPWR("IOI2CController::sSysPowerDownHandler System will %s\n", 
						(messageType == kIOMessageSystemWillPowerOff)?"Power off":"Restart");

				if (params = (IOPowerStateChangeNotification *) messageArgument)
				{
					params->returnValue = 20 * 1000 * 1000;
					thread_call_enter1(self->fIOSyncThreadCall, (thread_call_param_t)params->powerRef);
					IOSleep(10);
					status = kIOReturnSuccess;
				}
				break;

			default:
				DLOG("IOI2CController::sSysPowerDownHandler called with 0x%lx\n", messageType);
				status = kIOReturnUnsupported;
				break;
		}
	}

	return status;
}

void
IOI2CController::sIOSyncCallback(
	thread_call_param_t	p0,
	thread_call_param_t	pmRef)
{
	IOI2CController	*self;
	DLOGPWR("IOI2CController::sIOSyncCallback - ENTERED\n");
	if (self = OSDynamicCast(IOI2CController, (OSMetaClassBase *)p0))
	{
		// Notify all power state clients.
		// We do this synchronously on the callers thread because PM sucks.
		// notifyPowerStateInterest does not return until all clients have acked or we timeout.
		self->notifyPowerStateInterest();

		// If we timed out then too freakin bad because we're cuttin off all I2C IO now!
		self->fDeviceIsUsable = FALSE; // okaybye!
		DLOGPWR("IOI2CController::sIOSyncCallback OFFLINE\n-----------------------------\n");
	}
	DLOGPWR("IOI2CController::sIOSyncCallback - DONE\n");
	acknowledgeSleepWakeNotification(pmRef);
}

#pragma mark  
#pragma mark *** External Client Interface Methods ***
#pragma mark  



IOReturn
IOI2CController::newUserClient(
	task_t					owningTask,
	void					*securityID,
	UInt32					type,
	OSDictionary			*properties,
	IOUserClient			**handler)
{
	DLOG("%s::newUserClient\n", getName());

	if (IOUserClient::clientHasPrivilege(securityID, "root") != kIOReturnSuccess)
	{
		ERRLOG("%s::newUserClient: Can't create user client, not privileged\n", getName());
		return kIOReturnNotPrivileged;
	}

	if (type != kIOI2CUserClientType)
		return kIOReturnUnsupported;

	return super::newUserClient(owningTask, securityID, type, properties, handler);
}

/*******************************************************************************
 * I2C Client Interface: callPlatformFunction
 *******************************************************************************/

IOReturn
IOI2CController::callPlatformFunction(
	const OSSymbol	*functionName,
	bool			waitForFunction,
	void			*param1,
	void			*param2,
	void			*param3,
	void			*param4)
{
	if (symReadI2CBus->isEqualTo(functionName))
		return clientReadI2C((IOI2CCommand *)param1, (UInt32)param2);
	else
	if (symWriteI2CBus->isEqualTo(functionName))
		return clientWriteI2C((IOI2CCommand *)param1, (UInt32)param2);
	else
	if (symLockI2CBus->isEqualTo(functionName))
		return clientLockI2C((UInt32)param1, (UInt32 *)param2);
	else
	if (symUnlockI2CBus->isEqualTo(functionName))
		return clientUnlockI2C((UInt32)param1, (UInt32)param2);
	else
	if (symPowerInterest->isEqualTo(functionName))
		return registerPowerStateInterest((IOService *)param1, (bool)param2);	// target = client instance
	else
	if (symGetMaxI2CDataLength->isEqualTo(functionName))
	{
		if (param1 == 0)
			return kIOReturnBadArgument;

		*(UInt32 *)param1 = fMaxI2CDataLength;
		return kIOReturnSuccess;
	}

    return super::callPlatformFunction (functionName, waitForFunction, param1, param2, param3, param4);
}

IOReturn
IOI2CController::registerPowerStateInterest(
	IOService		*client,
	bool			isRegistering)
{
	IOReturn		status = kIOReturnSuccess;
	OSArray			*array;
	OSDictionary	*dict;

	IOLockLock(fPowerLock);

	// If we don't already have an interest array property then make one...
	if (0 == (array = (OSArray *) getProperty( symPowerInterest )))
	{
		array = OSArray::withCapacity( 1 );
		if (array)
		{
			setProperty( symPowerInterest, array );
			array->release();
		}
	}

	if (isRegistering == false)
	{
		status = kIOReturnNotFound;

		if (array)
		{
			int i, count = array->getCount();
			for (i = 0; i < count; i++)
			{
				if (dict = OSDynamicCast(OSDictionary, array->getObject( i )))
				{
					if (client == OSDynamicCast(IOService, dict->getObject(symPowerClient)))
					{
						dict->removeObject(symPowerClient);
						dict->removeObject(symPowerAcked);
						array->removeObject(i);
						status = kIOReturnSuccess;
						break;
					}
				}
			}
		}
	}
	else
	if (array)
	{
		dict = OSDictionary::withCapacity( 1 );
		if (dict)
		{
			dict->setObject(symPowerClient, client);
			dict->setObject(symPowerAcked, kOSBooleanFalse);
			array->setObject( dict );
			dict->release(); // Each client dictionary is retained only by the array.
		}
		else
			status = kIOReturnNoMemory;
	}
	else
		status = kIOReturnNoMemory;

	IOLockUnlock(fPowerLock);
	return status;
}

bool
IOI2CController::notifyPowerStateInterest(void)
{
	OSArray			*array;
	OSDictionary	*dict;
	IOService		*client;
	int				i, count;
	bool			allAcked = true;

	array = OSDynamicCast(OSArray , getProperty(symPowerInterest));
	if (array)
	{
		count = array->getCount();

		for (i = 0; i < count; i++)
		{
			if (dict = OSDynamicCast(OSDictionary, array->getObject( i )))
			{
				dict->setObject(symPowerAcked, kOSBooleanFalse);

				if (client = OSDynamicCast(IOService, dict->getObject(symPowerClient)))
				{
					if (kIOReturnSuccess != client->message(0x1012c, this, 0))
					{
						IOLockLock(fPowerLock);
						dict->setObject(symPowerAcked, kOSBooleanTrue);
						IOLockUnlock(fPowerLock);
					}
				}
			}
		}

		AbsoluteTime	currentTime, endTime;
		clock_interval_to_deadline(15, kSecondScale, &endTime);
		DLOGPWR("IOI2CController waiting for %d acks\n", count);

		for (;;)
		{
			allAcked = true;
			IOSleep(5);

			// Iterate through all clients: until all have acked 
			for (i = 0; i < count; i++)
			{
				if (dict = OSDynamicCast(OSDictionary, array->getObject( i )))
				{
					if (kOSBooleanTrue != OSDynamicCast(OSBoolean, dict->getObject(symPowerAcked)))
					{
//						DLOG("IOI2CController still waiting for ack:%d\n", i);
						allAcked = false;
						break;
					}
				}
			}
			if (allAcked == true)
				break;

			clock_get_uptime(&currentTime);
			if ( CMP_ABSOLUTETIME(&currentTime, &endTime) > 0 )
			{
				ERRLOG("IOI2CController::notifyPowerStateInterest timed out waiting for acks\n");
				break;
			}
		}

	}
	
	return allAcked;
}

IOReturn
IOI2CController::acknowledgeNotification(
	IONotificationRef	notification,
	IOOptionBits		response )
{
	OSArray				*array;
	OSDictionary		*dict;
	int					i, count;
	IOService			*target;
	IOService			*client;

	if (target = OSDynamicCast(IOService, (OSMetaClassBase *)notification))
	{
		array = OSDynamicCast(OSArray , getProperty(symPowerInterest));
		if (array)
		{
			count = array->getCount();
			for (i = 0; i < count; i++)
			{
				if (dict = OSDynamicCast(OSDictionary, array->getObject( i )))
				{
					if (client = OSDynamicCast(IOService, dict->getObject(symPowerClient)))
					{
						if (client == target)
						{
#if I2C_DLOGPWR
							const char *name;
							DLOGPWR("IOI2CController::acknowledgeNotification from %s\n", (name = client->getName())?name:"???");
#endif
							dict->setObject(symPowerAcked, kOSBooleanTrue);
							break;
						}
					}
				}
			}
		}
	}

	return kIOReturnSuccess;
}

#pragma mark  
#pragma mark *** IOI2C Transaction Methods ***
#pragma mark  

IOReturn
IOI2CController::clientReadI2C(
	IOI2CCommand	*cmd,
	UInt32			clientKey)
{
	IOReturn		status = kIOReturnSuccess;
	int				retries;
	AbsoluteTime	currentTime, endTime;

	if (cmd == NULL)
		return kIOReturnBadArgument;

	if (fI2CBus != kIOI2CMultiBusID)
		cmd->bus = fI2CBus;

//	DLOG("+IOI2CController::clientReadI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx\n",
//		clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode);
	if (fDeviceIsUsable == FALSE)
	{
		ERRLOG("-IOI2CController::clientReadI2C No Power\n");
		status = kIOReturnNoPower;
	}
	else
	if (clientKey == kIOI2C_CLIENT_KEY_DEFAULT)
	{
		if (kIOReturnSuccess == (status = clientLockI2C(cmd->bus, &clientKey)))
		{
			status = clientReadI2C(cmd, clientKey);
			clientUnlockI2C(cmd->bus, clientKey);
		}
	}
	else
	if (fClientLockKey != clientKey)
	{
		ERRLOG("-IOI2CController::clientReadI2C invalid key\n");
		status = kIOReturnNotOpen;
	}
	else
	{
		// This logic is true for all I2C controllers...
		// !data !len   00 OK	// if you don't have data then you can't have length!
		// !data  len   01 err
		//  data !len   10 err	// if you have data then you better have length too!
		//  data  len   11 OK

		if ((cmd->buffer == 0) ^ (cmd->count == 0))
		{
			ERRLOG("-IOI2CController::clientReadI2C bad buffer:%x length:%lx argument combination\n", (int)cmd->buffer, cmd->count);
			return kIOReturnBadArgument;
		}

		// TODO: Need to start a timer to ensure command.timeout_uS is not exceeded.
		if (cmd->timeout_uS)
			clock_interval_to_deadline(cmd->timeout_uS, kMicrosecondScale, &endTime);

		for (retries = (int)cmd->retries; retries >= 0; retries--)
		{
//			DLOG("IOI2CController::clientReadI2C calling processReadI2CBus\n");
			fTransactionInProgress = TRUE;
			DLOGI2C((cmd->options), "IOI2CController::clientReadI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx\n",
				clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode);
			status = processReadI2CBus (cmd);
			if (status)
			{
				ERRLOG("IOI2CController::clientReadI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx status:0x%x\n",
					clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode, status);
			}
			else
			{
				DLOGI2C((cmd->options), "IOI2CController::clientReadI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx status:0x%x\n",
					clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode, status);
			}
			fTransactionInProgress = FALSE;

			if (status == kIOReturnSuccess)
				break;

			if (fDeviceIsUsable == FALSE)
			{
				status = kIOReturnOffline;
				break;
			}
			if (cmd->timeout_uS)
			{
				clock_get_uptime(&currentTime);
				if ( CMP_ABSOLUTETIME(&currentTime, &endTime) > 0 )
				{
					status = kIOReturnTimeout;
					break;
				}
			}

			DLOG("IOI2CController::clientReadI2C retry:%lu status:0x%08x\n", cmd->retries - retries, status);
		}

		if (status)
			ERRLOG("-IOI2CController::clientReadI2C status = 0x%08x\n", status);
	}

	return status;
}


IOReturn
IOI2CController::clientWriteI2C(
	IOI2CCommand	*cmd,
	UInt32			clientKey)
{
	IOReturn		status = kIOReturnSuccess;
	int				retries;

	if (cmd == NULL)
		return kIOReturnBadArgument;

	if (fI2CBus != kIOI2CMultiBusID)
		cmd->bus = fI2CBus;

	if (fDeviceIsUsable == FALSE)
	{
		ERRLOG("-IOI2CController::clientWriteI2C No Power\n");
		status = kIOReturnNoPower;
	}
	else
	if (clientKey == kIOI2C_CLIENT_KEY_DEFAULT)
	{
		if (kIOReturnSuccess == (status = clientLockI2C(cmd->bus, &clientKey)))
		{
			status = clientWriteI2C(cmd, clientKey);
			clientUnlockI2C(cmd->bus, clientKey);
		}
	}
	else
	if (clientKey != fClientLockKey)
	{
		ERRLOG("-IOI2CController::clientWriteI2C invalid key\n");
		status = kIOReturnNotOpen;
	}
	else
	{
		// This logic is true for all I2C controllers...
		// !data !len   00 OK	// if you don't have data then you can't have length!
		// !data  len   01 err
		//  data !len   10 err	// if you have data then you better have length too!
		//  data  len   11 OK

		if ((cmd->buffer == 0) ^ (cmd->count == 0))
		{
			ERRLOG("-IOI2CController::clientWriteI2C bad buffer:%lx length:%lx argument combination (B:%lx, A:%lx S:%lx)\n", (UInt32)cmd->buffer, cmd->count,
				cmd->bus, cmd->address, cmd->subAddress);
			return kIOReturnBadArgument;
		}

		// TODO: Need to start a timer to ensure command.timeout_uS is not exceeded.
		AbsoluteTime	currentTime, endTime;
		if (cmd->timeout_uS)
			clock_interval_to_deadline(cmd->timeout_uS, kMicrosecondScale, &endTime);

		for (retries = (int)cmd->retries; retries >= 0; retries--)
		{
			fTransactionInProgress = TRUE;
			DLOGI2C((cmd->options), "IOI2CController::clientWriteI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx\n",
				clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode);
			status = processWriteI2CBus (cmd);
			if (status)
			{
				ERRLOG("IOI2CController::clientWriteI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx status:0x%x\n",
					clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode, status);
			}
			else
			{
				DLOGI2C((cmd->options), "IOI2CController::clientWriteI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx status:0x%x\n",
					clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode, status);
			}
			fTransactionInProgress = FALSE;

			if (status == kIOReturnSuccess)
				break;

			if (fDeviceIsUsable == FALSE)
			{
				status = kIOReturnOffline;
				break;
			}

			if (cmd->timeout_uS)
			{
				clock_get_uptime(&currentTime);
				if ( CMP_ABSOLUTETIME(&currentTime, &endTime) > 0 )
				{
					status = kIOReturnTimeout;
					break;
				}
			}
			DLOG("IOI2CController::clientWriteI2C retry:%lu status:0x%08x\n", cmd->retries - retries, status);
		}
		if (status)
			ERRLOG("-IOI2CController::clientWriteI2C status = 0x%08x\n", status);
	}

	return status;
}

IOReturn
IOI2CController::clientLockI2C(
	UInt32			bus,
	UInt32			*clientKeyRef)
{
	IOReturn		status;

	if (clientKeyRef == NULL)
	{
		ERRLOG("IOI2CController::clientLockI2C invalid key ref\n");
		return kIOReturnBadArgument;
	}

	if (fDeviceIsUsable == FALSE)
	{
		ERRLOG("IOI2CController::clientLockI2C fDeviceIsUsable==0\n");
		return kIOReturnNoPower;
	}

	I2CLOCK;

	// Cancel any pending clients if power has been dropped.
	if (fDeviceIsUsable == FALSE)
	{
		ERRLOG("IOI2CController::clientLockI2C cancel fDeviceIsUsable==0\n");
		I2CUNLOCK;
		return kIOReturnNoPower;
	}

	if (fI2CBus != kIOI2CMultiBusID)
		bus = fI2CBus;

	// Client has exclusive access now.
	// Forward Lock bus request to subclass.
	status = processLockI2CBus(bus);
	if (kIOReturnSuccess != status)		// bus Lock failed so relinquish client exclusive access.
	{
		ERRLOG("IOI2CController::clientLockI2C LockBus failed:0x%lx\n", (UInt32)status);
		I2CUNLOCK;
		return status;
	}

	// Client Lock Succeeded. Return client access key.
	if (++fClientLockKey >= kIOI2C_CLIENT_KEY_RESERVED)
		fClientLockKey = kIOI2C_CLIENT_KEY_VALID | kIOI2C_CLIENT_KEY_LOCKED;
	*clientKeyRef = fClientLockKey;
//	DLOG("IOI2CController::clientLockI2C key: %lx\n", fClientLockKey);

	return status;
}

IOReturn
IOI2CController::clientUnlockI2C(
	UInt32			bus,
	UInt32			clientKey)
{
	IOReturn		status;

//	DLOG("IOI2CController::clientUnlockI2C fkey: %lx, ckey: %lx\n", fClientLockKey, clientKey);
	if (fClientLockKey != clientKey)
	{
		ERRLOG("IOI2CController::clientUnlockI2C invalid key\n");
		return kIOReturnExclusiveAccess;
	}

	if (0 == (fClientLockKey & kIOI2C_CLIENT_KEY_LOCKED))
	{
		ERRLOG("IOI2CController::clientUnlockI2C not locked\n");
		return kIOReturnNotOpen;
	}

	if (fI2CBus != kIOI2CMultiBusID)
		bus = fI2CBus;

	status = processUnlockI2CBus(bus);

	++fClientLockKey;
	I2CUNLOCK;

	return status;
}


// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOI2CController, 0 );
OSMetaClassDefineReservedUnused ( IOI2CController, 1 );
OSMetaClassDefineReservedUnused ( IOI2CController, 2 );
OSMetaClassDefineReservedUnused ( IOI2CController, 3 );
OSMetaClassDefineReservedUnused ( IOI2CController, 4 );
OSMetaClassDefineReservedUnused ( IOI2CController, 5 );
OSMetaClassDefineReservedUnused ( IOI2CController, 6 );
OSMetaClassDefineReservedUnused ( IOI2CController, 7 );
OSMetaClassDefineReservedUnused ( IOI2CController, 8 );
OSMetaClassDefineReservedUnused ( IOI2CController, 9 );
OSMetaClassDefineReservedUnused ( IOI2CController, 10 );
OSMetaClassDefineReservedUnused ( IOI2CController, 11 );
OSMetaClassDefineReservedUnused ( IOI2CController, 12 );
OSMetaClassDefineReservedUnused ( IOI2CController, 13 );
OSMetaClassDefineReservedUnused ( IOI2CController, 14 );
OSMetaClassDefineReservedUnused ( IOI2CController, 15 );
