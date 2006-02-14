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
 *	File: $Id: IOI2CDevice.cpp,v 1.11 2006/02/02 00:24:46 hpanther Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CDevice.cpp,v $
 *		Revision 1.11  2006/02/02 00:24:46  hpanther
 *		Replace flawed IOLock synchronization with semaphores.
 *		A bit of cleanup on the logging side.
 *		
 *		Revision 1.10  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.9  2005/04/29 22:24:10  tsherman
 *		4103531 - IOI2CLM7x driver fails to loads intermittenly on Q63.
 *		
 *		Revision 1.8  2005/02/08 21:04:29  jlehrer
 *		[3987457] zero init reserved buffer.
 *		Added explicit flag to indicate we called PMinit.
 *		In lockI2CBus don't try to take the lock if offline.
 *		Added debug log flags.
 *		
 *		Revision 1.7  2004/12/17 00:51:01  jlehrer
 *		[3867728] Force PM to power off before calling PMstop in freeI2CResources.
 *		
 *		Revision 1.6  2004/12/15 02:16:58  jlehrer
 *		[3905559] Disable mac-io/i2c power management for AOA.
 *		[3917744,3917697] Require root privilges for IOI2CUserClient class.
 *		[3867728] Add support for teardown in freeI2CResources and callPlatformFunction.
 *		
 *		Revision 1.5  2004/11/04 20:20:28  jlehrer
 *		Unregisters for IOI2CPowerStateInterest in freeI2CResources.
 *		
 *		Revision 1.4  2004/09/28 01:47:37  jlehrer
 *		Added separate DLOGPWR macro.
 *		
 *		Revision 1.3  2004/09/17 21:05:24  jlehrer
 *		Removed APSL headers.
 *		Added support for 10-bit addresses.
 *		Added external client read/write interface.
 *		Fixed: removed semaphore_wait(fClientSem) from wakeup event.
 *		Added: publish all on demand and interrupt flagged PlatformFunctions.
 *		Changed: readI2C/writeI2C to not recursive call when default key is used.
 *		
 *		Revision 1.2  2004/06/08 23:45:15  jlehrer
 *		Added ERRLOG, disabled DLOG, changed DLOGI2C to use runtime cmd.option flag.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */


#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/pwr_mgt/IOPMPrivate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOPlatformExpert.h>

#include "IOPlatformFunction.h"
#include <IOI2C/IOI2CDevice.h>
#include <IOKit/IOUserClient.h>

//#define I2C_DEBUG 1

#if (defined(I2C_DEBUG) && I2C_DEBUG)
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define I2C_ERRLOG 1

#if (defined(I2C_ERRLOG) && I2C_ERRLOG)
#define ERRLOG(fmt, args...)  do{kprintf(fmt, ## args);IOLog(fmt, ## args);}while(0)
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
#define DLOGI2C(opt, fmt, args...)	do{if((opt&kI2COption_VerboseLog)||(fStateFlags&kStateFlags_kprintf))kprintf(fmt, ## args);if((opt&kI2COption_VerboseLog)||(fStateFlags&kStateFlags_IOLog))IOLog(fmt, ## args);}while(0)
//#define DLOGI2C(opt, fmt, args...) do{if((opt&kI2COption_VerboseLog)||(fStateFlags&kStateFlags_kprintf)){mach_timespec_t absTime; IOGetTime(&absTime);kprintf("%d.%d " fmt, (int)absTime.tv_sec, (int)absTime.tv_nsec, ## args);}}while(0)
#else
#define DLOGI2C(opt, fmt, args...)
#endif

// Define kUSE_IOLOCK for IOLock, undefine for semaphore
//#define kUSE_IOLOCK
#ifdef kUSE_IOLOCK
	#define I2CLOCK		IOLockLock(fClientLock)
	#define I2CUNLOCK	{IOLockUnlock(fClientLock);IOSleep(0);}
	// IOSleep lets other threads have a chance to run.
#else
	#define I2CLOCK		semaphore_wait(fClientSem)
	#define I2CUNLOCK	semaphore_signal(fClientSem)
#endif



/*******************************************************************************
 * IOI2CDevice
 *******************************************************************************/

#pragma mark  
#pragma mark *** IOI2CDevice class ***
#pragma mark  

#define super IOService
OSDefineMetaClassAndStructors( IOI2CDevice, IOService )

bool IOI2CDevice::init(
	OSDictionary	*dict)
{
	DLOG("IOI2CDevice::init\n");
	if (!super::init(dict))
		return false;

	fDeviceOffline = TRUE;
	fPlatformFuncArray = 0;

	if (0 == (reserved = (ExpansionData *)IOMalloc(sizeof(struct ExpansionData))))
		return false;
	bzero( reserved, sizeof(struct ExpansionData) );	// [3987457]

	return true;
}

bool
IOI2CDevice::start(
	IOService	*provider)
{
	OSData		*regprop;
	IOReturn	status;

	DLOG("+IOI2CDevice::start\n");

	if (false == super::start(provider))
		return false;

	fProvider = provider;

	// Get I2C slave address. (required)
	if (regprop = OSDynamicCast(OSData, fProvider->getProperty("reg")))
	{
		fI2CAddress = *((UInt32 *)regprop->getBytesNoCopy());
		if (false == isI2C10BitAddress(fI2CAddress))
			fI2CAddress &= 0xff;
	}
	else
	{
		ERRLOG("-IOI2CDevice::start no \"reg\" property\n");
		return false;
	}

	if (fProvider->getProperty("AAPL,no-power"))
		fStateFlags |= kStateFlags_DISABLE_PM;

	// Initailize I2C resources.
	if (kIOReturnSuccess != (status = initI2CResources()))
	{
		ERRLOG("-IOI2CDevice@%lx::start initI2CResources failed:0x%lx\n", fI2CAddress, (UInt32)status);
		freeI2CResources();
		return false;
	}

	AbsoluteTime deadline, currentTime;

	// The original deadline was for 3 seconds, but this proved to short in some systems with densely populated I2C busses [4103531]
	clock_interval_to_deadline(15, kSecondScale, &deadline);

	while (isI2COffline())
	{
		IOSleep(10);
		clock_get_uptime(&currentTime);
		if ( CMP_ABSOLUTETIME(&currentTime, &deadline) > 0 )
		{
			ERRLOG("-IOI2CDevice@%lx::start timed out waiting to power on\n", fI2CAddress);
			freeI2CResources();
			return false;
		}
	}

	// Is this is a non-subclassed IOI2CDevice instance? Then enable on-demand platform functions and call registerService.
	// Otherwise the subclass driver can choose to enable on-demand PFs by setting the fEnableOnDemandPlatformFunctions flag.
	const char *name;
	if ( (name = getName()) && (0 == strcmp(name, "IOI2CDevice")) )
	{
		DLOG("%s@%lx::start enabling on demand PF functions\n", name, fI2CAddress);
		fEnableOnDemandPlatformFunctions = true;
		registerService();
	}

	DLOG("-IOI2CDevice@%lx::start\n",fI2CAddress);
	return true;
}

void
IOI2CDevice::stop (IOService * provider)
{
	DLOG("IOI2CDevice@%lx::stop\n",fI2CAddress);
	freeI2CResources();
	super::stop(provider);
}

void
IOI2CDevice::free ( void )
{
	DLOG("IOI2CDevice@%lx::free\n",fI2CAddress);
	freeI2CResources();
	if (reserved)	{ IOFree(reserved, sizeof(struct ExpansionData));		reserved = 0; }
	super::free();
}

/*******************************************************************************
 * initI2CResources
 *******************************************************************************/

#pragma mark  
#pragma mark *** I2C resources init and free methods ***
#pragma mark  

IOReturn
IOI2CDevice::initI2CResources(void)
{
	IOReturn	status;
	DLOG("+IOI2CDevice@%lx::initI2CResources\n",fI2CAddress);

	// Create callPlaformFunction symbols.
	symLockI2CBus = OSSymbol::withCStringNoCopy(kLockI2Cbus);
	symUnlockI2CBus = OSSymbol::withCStringNoCopy(kUnlockI2Cbus);
	symWriteI2CBus = OSSymbol::withCStringNoCopy(kWriteI2Cbus);
	symReadI2CBus = OSSymbol::withCStringNoCopy(kReadI2Cbus);
	symClientWrite = OSSymbol::withCStringNoCopy(kIOI2CClientWrite);
	symClientRead = OSSymbol::withCStringNoCopy(kIOI2CClientRead);
	symPowerInterest = OSSymbol::withCStringNoCopy("IOI2CPowerStateInterest");

#ifdef kUSE_IOLOCK
	fClientLock = IOLockAlloc();
#else
	if (kIOReturnSuccess != (status = semaphore_create(current_task(), (semaphore**)&fClientSem, SYNC_POLICY_FIFO, 1)))
		return status;
#endif
	if (!symLockI2CBus || !symUnlockI2CBus || !symWriteI2CBus || !symReadI2CBus
#ifdef kUSE_IOLOCK
		|| !fClientLock
#else
		|| !fClientSem
#endif
		)
		return kIOReturnNoMemory;

	if (kIOReturnSuccess != (status = InitializePlatformFunctions()))
		return status;

	if (fStateFlags & kStateFlags_DISABLE_PM)
		fDeviceOffline = FALSE;
	else
	{
		if (kIOReturnSuccess != (status = InitializePowerManagement()))
			return status;
	}

	DLOG("-IOI2CDevice@%lx::initI2CResources\n",fI2CAddress);
	return status;
}

/*******************************************************************************
 * freeI2CResources
 *******************************************************************************/

void
IOI2CDevice::freeI2CResources(void)
{
	if (fStateFlags & kStateFlags_TEARDOWN)
		return;
	fStateFlags |= kStateFlags_TEARDOWN;
	DLOG("+IOI2CDevice@%lx::freeI2CResources %x\n",fI2CAddress, fStateFlags);

#ifdef kUSE_IOLOCK
	if(fClientLock)
#else
	if(fClientSem)
#endif
	I2CLOCK;

	fDeviceOffline = TRUE;

#ifdef kUSE_IOLOCK
	if(fClientLock)
#else
	if(fClientSem)
#endif
	I2CUNLOCK;

	DLOG("+IOI2CDevice@%lx::freeI2CResources\n",fI2CAddress);
	if (fStateFlags & kStateFlags_PMInit)	// Don't rely on initialized flag to identify if PMinit was called.
	{
		DLOGPWR("+IOI2CDevice@%lx::freeI2CResources requesting power OFF\n",fI2CAddress);
		changePowerStateTo(kIOI2CPowerState_OFF);

		AbsoluteTime deadline, currentTime;
		clock_interval_to_deadline(20, kSecondScale, &deadline);

		while (fCurrentPowerState != kIOI2CPowerState_OFF)
		{
			IOSleep(10);
			clock_get_uptime(&currentTime);
			if ( CMP_ABSOLUTETIME(&currentTime, &deadline) > 0 )
			{
				ERRLOG("IOI2CDevice@%lx::freeI2CResources timed out waiting to power off\n", fI2CAddress);
				break;
			}
		}

		DLOGPWR("+IOI2CDevice@%lx::freeI2CResources calling PMStop\n",fI2CAddress);
		if (fStateFlags & kStateFlags_PMInit)
		{
			fStateFlags &= ~kStateFlags_PMInit;
			PMstop();
		}
	}
	DLOG("IOI2CDevice@%lx::freeI2CResources 1\n",fI2CAddress);

	if (fPowerStateThreadCall)
	{
		thread_call_cancel(fPowerStateThreadCall);
		thread_call_free(fPowerStateThreadCall);
		fPowerStateThreadCall = 0;
	}

	if (fProvider)
		fProvider->callPlatformFunction("IOI2CPowerStateInterest", FALSE, (void *)this, (void *)false, 0, 0);

	DLOG("IOI2CDevice@%lx::freeI2CResources 2\n",fI2CAddress);
	if (symLockI2CBus)		{ symLockI2CBus->release();		symLockI2CBus = 0; }
	if (symUnlockI2CBus)	{ symUnlockI2CBus->release();	symUnlockI2CBus = 0; }
	if (symWriteI2CBus)		{ symWriteI2CBus->release();	symWriteI2CBus = 0; }
	if (symReadI2CBus)		{ symReadI2CBus->release();		symReadI2CBus = 0; }

	DLOG("IOI2CDevice@%lx::freeI2CResources 3\n",fI2CAddress);

#ifdef kUSE_IOLOCK
	if (fClientLock)		{ IOLockFree(fClientLock);		fClientLock = 0; }
#else
	if (fClientSem)			{ semaphore_destroy(current_task(), fClientSem);	fClientSem = 0; }
#endif

	DLOG("IOI2CDevice@%lx::freeI2CResources 4\n",fI2CAddress);
	if (reserved)
	{
		if (symClientRead)		{ symClientRead->release();		symClientRead = 0; }
		if (symClientWrite)		{ symClientWrite->release();	symClientWrite = 0; }
		if (symPowerInterest)	{ symPowerInterest->release();	symPowerInterest = 0; }
	}

	DLOG("-IOI2CDevice@%lx::freeI2CResources\n",fI2CAddress);
}

/*******************************************************************************
 * newUserClient
 *******************************************************************************/

#pragma mark  
#pragma mark *** IOI2CDevice user client creation ***
#pragma mark  

IOReturn
IOI2CDevice::newUserClient(
	task_t			owningTask,
	void			*securityID,
	UInt32			type,
	OSDictionary	*properties,
	IOUserClient	**handler)
{
	IOUserClient	*client;
	OSObject		*temp;

	DLOG("%s::newUserClient\n", getName());

	if (type != kIOI2CUserClientType)
		return super::newUserClient(owningTask,securityID,type,properties,handler);

	if (IOUserClient::clientHasPrivilege(securityID, "root") != kIOReturnSuccess)
	{
		ERRLOG("%s::newUserClient: Can't create user client, not privileged\n", getName());
		return kIOReturnNotPrivileged;
	}

	temp = OSMetaClass::allocClassWithName("IOI2CUserClient");
	if (!temp)
		return kIOReturnNoMemory;

	if (OSDynamicCast(IOUserClient, temp))
		client = (IOUserClient *) temp;
	else
	{
		temp->release();
		return kIOReturnUnsupported;
	}

	if ( !client->initWithTask(owningTask, securityID, type, properties) )
	{
		client->release();
		return kIOReturnBadArgument;
	}

	if ( !client->attach(this) )
	{
		client->release();
		return kIOReturnUnsupported;
	}

	if ( !client->start(this) )
	{
		client->detach(this);
		client->release();
		return kIOReturnUnsupported;
	}

	*handler = client;
	return kIOReturnSuccess;
}


/*******************************************************************************
 * Power Management Initialization
 * Power state info:
 * IOPMPowerFlags	capabilityFlags;	// bits that describe (to interested drivers) the capability of the device in this state 
 * IOPMPowerFlags	outputPowerCharacter;	// description (to power domain children) of the power provided in this state 
 * IOPMPowerFlags	inputPowerRequirement;	// description (to power domain parent) of input power required in this state
 *******************************************************************************/

#pragma mark  
#pragma mark *** power management methods ***
#pragma mark  

IOReturn
IOI2CDevice::InitializePowerManagement(void)
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

	DLOG("IOI2CDevice@%lx::InitializePowerManagement\n", fI2CAddress);

	// Initialize Power Management superclass variables from IOService.h
	PMinit();
	fStateFlags |= kStateFlags_PMInit;

	// Join the Power Management tree from IOService.h
	fProvider->joinPMtree( this);

	if (0 == (fPowerStateThreadCall = thread_call_allocate(&IOI2CDevice::sPowerStateThreadCall, (thread_call_param_t) this)))
	{
		ERRLOG("IOI2CDevice Failed to allocate threadcall.\n");
		return kIOReturnNoResources;
	}

	// Register ourselves for power state interest notifications from the controller.
	if (kIOReturnSuccess != (status = fProvider->callPlatformFunction("IOI2CPowerStateInterest", FALSE, (void *)this, (void *)true, NULL, NULL)))
	{
		ERRLOG("IOI2CDevice register IOI2CPowerStateInterest falied\n");
		return status;
	}

	// Register ourselves as the power controller.
	if (kIOReturnSuccess != (status = registerPowerDriver( this, (IOPMPowerState *) ourPowerStates, kIOI2CPowerState_COUNT )))
	{
		ERRLOG("IOI2CDevice Failed to registerPowerDriver.\n");
		return status;
	}

	return status;
}

unsigned long
IOI2CDevice::maxCapabilityForDomainState(
	IOPMPowerFlags	domainState)
{
	unsigned long maxCapability = super::maxCapabilityForDomainState (domainState );
	DLOG("IOI2CDevice@%lx:maxCapabilityForDomainState(0x%lx) == 0x%lx\n", fI2CAddress, (UInt32)domainState, (UInt32)maxCapability);
	return maxCapability;
}

IOReturn
IOI2CDevice::message(
	UInt32		type,
	IOService	*provider,
	void		*argument)
{
	if (type == 0x1012c)
	{
//		DLOG("######################################\nIOI2CDevice@%lx::message called\n", fI2CAddress);
		if (provider == NULL)
			return kIOReturnBadArgument;

		if (fPowerStateThreadCall == 0)
			return kIOReturnUnsupported;

		if (fStateFlags & kStateFlags_TEARDOWN)
			return kIOReturnUnsupported;

		fSysPowerRef = provider;
		thread_call_enter1(fPowerStateThreadCall, (thread_call_param_t)(kIOI2CPowerState_OFF));
//		DLOG("IOI2CDevice@%lx::message processed\n######################################\n", fI2CAddress);
		return kIOReturnSuccess;
	}

	return super::message(type, provider, argument);
}


/*******************************************************************************
 * setPowerState
 * Power Management state change callback
 * We don't want to do work on the power thread so we just signal to change
 * to our own sPowerStateThreadCall thread.
 *******************************************************************************/

IOReturn IOI2CDevice::setPowerState(
	unsigned long	newPowerState,
	IOService		*dontCare)
{
	DLOGPWR("IOI2CDevice@%lx::setPowerState called with state:%lu\n", fI2CAddress, newPowerState);

	if (pm_vars == NULL)
		return IOPMAckImplied;

	if (fPowerStateThreadCall == 0)
		return IOPMAckImplied;

	if (fCurrentPowerState == newPowerState)
		return IOPMAckImplied;

	thread_call_enter1(fPowerStateThreadCall, (thread_call_param_t)newPowerState);
	return kSetPowerStateTimeout;
}

/*******************************************************************************
 * sPowerStateThreadCall
 * static class thread call entry point. Just calls to powerStateThreadCall.
 *******************************************************************************/

void
IOI2CDevice::sPowerStateThreadCall(
	thread_call_param_t	p0,
	thread_call_param_t	p1)
{
	IOI2CDevice		*self;
	unsigned long	newPowerState = (unsigned long)p1;

	self = OSDynamicCast(IOI2CDevice, (OSMetaClassBase *)p0);
	if (self == NULL)
		panic("IOI2CDevice::sPowerStateThreadCall with unknown \"this\" type\n");

	DLOGPWR("IOI2CDevice@%lx::sPowerStateThreadCall called with state:%lu\n", self->fI2CAddress, newPowerState);
	self->powerStateThreadCall(newPowerState);
}

/*******************************************************************************
 * powerStateThreadCall
 * Instance power state change thread call entry point.
 *******************************************************************************/

void
IOI2CDevice::powerStateThreadCall(
	unsigned long	newPowerState)
{
	if (newPowerState == kIOI2CPowerState_OFF)
	{
		DLOGPWR("IOI2CDevice@%lx transition to OFF\n", fI2CAddress);

		I2CLOCK;								// Wait for pending I2C requests to complete then block any new I2C requests...

		fPowerThreadID = current_thread();		// Setup to allow only requests from this thread to be processed.
		fClientIOBlocked = TRUE;				// All requests from other threads will return offline.

		I2CUNLOCK;								// Allow blocked threads to proceed so they return offline.

		if (fSysPowerRef)
		{
			DLOGPWR("IOI2CDevice@%lx process SHUTDOWN event\n", fI2CAddress);
			processPowerEvent(kI2CPowerEvent_SHUTDOWN);
		}
		else
		{
			DLOGPWR("IOI2CDevice@%lx process power OFF event\n", fI2CAddress);
			// If our teardown flag is set do not notify the subclass of the power OFF event.
			// The hardware was probably not responding and the subclass driver called freeI2CResources or failed to start.
			// In which case we are in the process of setting our power state to OFF before calling PMstop.
			if (0 == (fStateFlags & kStateFlags_TEARDOWN))
				processPowerEvent(kI2CPowerEvent_OFF);
		}

		fDeviceOffline = TRUE;					// set flag to reflect shutting down state.
		fClientIOBlocked = FALSE;
		DLOGPWR("IOI2CDevice@%lx device OFFLINE\n------------------\n", fI2CAddress);
	}
	else
	if (newPowerState == kIOI2CPowerState_SLEEP)
	{
		DLOGPWR("IOI2CDevice@%lx transition to SLEEP\n", fI2CAddress);

		I2CLOCK;							// Wait for pending I2C requests to complete then block any new I2C requests...

		fPowerThreadID = current_thread();	// Setup to allow only requests from this thread to be processed.
		fClientIOBlocked = TRUE;			// All requests from other threads will return offline.

		I2CUNLOCK;							// Allow blocked threads to proceed so they return offline.

		if (fCurrentPowerState == kIOI2CPowerState_ON)
		{
			DLOGPWR("IOI2CDevice@%lx process SLEEP event\n", fI2CAddress);
			performFunctionsWithFlags(kIOPFFlagOnSleep);
			processPowerEvent(kI2CPowerEvent_SLEEP);
		}

		fDeviceOffline = TRUE;				// set flag to reflect shutting down state.
		fClientIOBlocked = FALSE;
		DLOGPWR("IOI2CDevice@%lx device OFFLINE\n------------------\n", fI2CAddress);
	}
	else
	if (newPowerState == kIOI2CPowerState_DOZE)
	{
		DLOGPWR("IOI2CDevice@%lx transition to DOZE\n", fI2CAddress);
		processPowerEvent(kI2CPowerEvent_DOZE);
	}
	else
	if (newPowerState == kIOI2CPowerState_ON)
	{
		DLOGPWR("IOI2CDevice@%lx transition to ON\n", fI2CAddress);

		I2CLOCK;								// Wait for pending I2C requests to complete then block any new I2C requests...

		fPowerThreadID = current_thread();		// Setup to allow only requests from this thread to be processed.
		fClientIOBlocked = TRUE;				// All requests from other threads will return offline.
		fDeviceOffline = FALSE;					// set flag to reflect we are not shutting down.

		I2CUNLOCK;								// Allow blocked threads to proceed so they return offline.

		if (fCurrentPowerState == kIOI2CPowerState_SLEEP)
		{
			DLOGPWR("IOI2CDevice@%lx process WAKE event\n", fI2CAddress);
			performFunctionsWithFlags(kIOPFFlagOnWake);
			processPowerEvent(kI2CPowerEvent_WAKE);
		}
		else
		{
			// The first power state ON transition is processed as a STARTUP event.
			// The fStateFlags.kStateFlags_STARTUP gets set only once...
			if (0 == (fStateFlags & kStateFlags_STARTUP))
			{
				fStateFlags |= kStateFlags_STARTUP;
				DLOGPWR("IOI2CDevice@%lx process STARTUP event\n", fI2CAddress);
				// Perform any functions flagged on init.
				performFunctionsWithFlags(kIOPFFlagOnInit);
				processPowerEvent(kI2CPowerEvent_STARTUP);
			}
			else
			{
				DLOGPWR("IOI2CDevice@%lx process power ON event\n", fI2CAddress);
				processPowerEvent(kI2CPowerEvent_ON);
			}
		}
		DLOGPWR("IOI2CDevice@%lx::setPowerState device ONLINE\n------------------\n", fI2CAddress);
		fClientIOBlocked = FALSE;				// Allow all threads to access I2C through this device.
	}
	else
	{
		DLOGPWR("IOI2CDevice@%lx ERROR transition to invalid state:%lu\n", fI2CAddress, newPowerState);
		return;
	}

	fCurrentPowerState = newPowerState;

	if (fSysPowerRef)
	{
		IOService *service = OSDynamicCast(IOService, (OSMetaClassBase *)fSysPowerRef);
		if (service)
		{
			DLOG("IOI2CDevice@%lx acknowledgeNotification\n", fI2CAddress);
			service->acknowledgeNotification(this, 0);
		}

		fSysPowerRef = NULL;
	}
	else
	{
		DLOG("IOI2CDevice@%lx acknowledgeSetPowerState: %lu\n", fI2CAddress, fCurrentPowerState);
		acknowledgeSetPowerState();
	}
}

#pragma mark  
#pragma mark *** IOI2CDevice subclass client API methods ***
#pragma mark  

/*******************************************************************************
 * processPowerEvent
 * Default IOI2CDevice power event handler.
 *******************************************************************************/

void IOI2CDevice::processPowerEvent(UInt32 eventType) {}

/*******************************************************************************
 * isI2COffline
 * Returns true if I2C device is offline, false if online.
 *******************************************************************************/

bool
IOI2CDevice::isI2COffline(void)
{
	if (fDeviceOffline || (fClientIOBlocked && (fPowerThreadID != current_thread())))
		return true;
	return false;
}

/*******************************************************************************
 * getI2CAddress
 * Returns I2C bus address of this device.
 *******************************************************************************/

UInt32
IOI2CDevice::getI2CAddress(void)
{
	return fI2CAddress;
}

/*******************************************************************************
 * I2C Command Interface
 *******************************************************************************/

IOReturn
IOI2CDevice::lockI2CBus(
	UInt32	*clientKeyRef)
{
	IOReturn	status = kIOReturnSuccess;
	UInt32		clientLockKey;

//	DLOG("IOI2CDevice@%lx::lockI2CBus\n", fI2CAddress);

	if (clientKeyRef == NULL)
	{
		ERRLOG("IOI2CDevice@%lx::lockI2CBus bad args\n", fI2CAddress);
		return kIOReturnBadArgument;
	}

	if (ml_at_interrupt_context())
	{
		ERRLOG("IOI2CDevice@%lx::lockI2CBus from primary interrupt context not permitted\n", fI2CAddress);
		*clientKeyRef = kIOI2C_CLIENT_KEY_INVALID;
		return kIOReturnNotPermitted;
	}

	if (isI2COffline())
	{
		ERRLOG("IOI2CDevice@%lx::lockI2CBus device is offline\n", fI2CAddress);
		*clientKeyRef = kIOI2C_CLIENT_KEY_INVALID;
		return kIOReturnOffline;
	}

	I2CLOCK;

//	DLOG("IOI2CDevice@%lx::lockI2CBus - device LOCKED\n", fI2CAddress);

	// Cancel any pending clients if power has been dropped.
	if (isI2COffline())
	{
		ERRLOG("IOI2CDevice@%lx::lockI2CBus lock canceled: device is offline\n", fI2CAddress);

		I2CUNLOCK;

		*clientKeyRef = kIOI2C_CLIENT_KEY_INVALID;
		return kIOReturnOffline;
	}

	status = fProvider->callPlatformFunction(symLockI2CBus, false, (void *)0, (void *)&clientLockKey, (void *)0, (void *)0);
	if (kIOReturnSuccess != status)
	{
		ERRLOG("IOI2CDevice@%lx::lockI2CBus - lock canceled: controller lockI2CBus failed:0x%lx\n", fI2CAddress, (UInt32)status);

		I2CUNLOCK;

		*clientKeyRef = kIOI2C_CLIENT_KEY_INVALID;
		return status;
	}

	// Lock Succeeded. Return key.
	*clientKeyRef = clientLockKey;
//	DLOG("IOI2CDevice@%lx::lockI2C key: %lx\n", fI2CAddress, clientLockKey);

	return status;
}

IOReturn
IOI2CDevice::unlockI2CBus(
	UInt32	clientKey)
{
	IOReturn	status;

	if (ml_at_interrupt_context())
	{
		ERRLOG("IOI2CDevice@%lx::unlockI2CBus from primary interrupt context not permitted\n", fI2CAddress);
		return kIOReturnNotPermitted;
	}

	if (isI2COffline())
	{
		ERRLOG("IOI2CDevice@%lx::unlockI2CBus device is offline\n", fI2CAddress);
		return kIOReturnOffline;
	}

	status = fProvider->callPlatformFunction(symUnlockI2CBus, false, (void *)0, (void *)clientKey, (void *)0, (void *)0);
	if (kIOReturnSuccess != status)
	{
		ERRLOG("IOI2CDevice@%lx::unlockI2CBus controller unlock failed key:%lx status:0x%x\n", fI2CAddress, clientKey, status);
		return status;
	}

//	DLOG("IOI2CDevice@%lx::unlockI2CBus - device UNLOCKED key:%lx\n", fI2CAddress, clientKey);

	I2CUNLOCK;

	return status;
}

IOReturn
IOI2CDevice::readI2C(
	IOI2CCommand	*cmd,
	UInt32			clientKey)
{
	IOReturn		status;

	if (cmd == NULL)
		return kIOReturnBadArgument;

	if (isI2COffline())
	{
		ERRLOG("IOI2CDevice@%lx::readI2C device is offline\n", fI2CAddress);
		status = kIOReturnOffline;
	}
	else
	if (clientKey == kIOI2C_CLIENT_KEY_DEFAULT)
	{
		if (kIOReturnSuccess == (status = lockI2CBus(&clientKey)))
		{
//			status = readI2C(cmd, clientKey);
			cmd->address = getI2CAddress();
			DLOGI2C((cmd->options), "IOI2CDevice@%lx::readI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx\n",
				fI2CAddress, clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode);
			status = fProvider->callPlatformFunction(symReadI2CBus, false, (void *)cmd, (void *)clientKey, (void *)0, (void *)0);
			unlockI2CBus(clientKey);
		}
	}
	else
	{
		cmd->address = getI2CAddress();
		DLOGI2C((cmd->options), "IOI2CDevice@%lx::readI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx\n",
			fI2CAddress, clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode);
		status = fProvider->callPlatformFunction(symReadI2CBus, false, (void *)cmd, (void *)clientKey, (void *)0, (void *)0);
	}

	return status;
}

IOReturn
IOI2CDevice::writeI2C(
	IOI2CCommand	*cmd,
	UInt32			clientKey)
{
	IOReturn		status;

	if (cmd == NULL)
		return kIOReturnBadArgument;

	if (isI2COffline())
	{
		ERRLOG("IOI2CDevice@%lx::writeI2C device is offline\n", fI2CAddress);
		status = kIOReturnOffline;
	}
	else
	if (clientKey == kIOI2C_CLIENT_KEY_DEFAULT)
	{
		if (kIOReturnSuccess == (status = lockI2CBus(&clientKey)))
		{
			cmd->address = getI2CAddress();
			DLOGI2C((cmd->options), "IOI2CDevice@%lx::writeI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx\n",
				fI2CAddress, clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode);
			status = fProvider->callPlatformFunction(symWriteI2CBus, false, (void *)cmd, (void *)clientKey, (void *)0, (void *)0);
			unlockI2CBus(clientKey);
		}
	}
	else
	{
		cmd->address = getI2CAddress();
		DLOGI2C((cmd->options), "IOI2CDevice@%lx::writeI2C cmd key:%lx, B:%lx, A:%lx S:%lx, L:%lx, M:%lx\n",
			fI2CAddress, clientKey, cmd->bus, cmd->address, cmd->subAddress, cmd->count, cmd->mode);
		status = fProvider->callPlatformFunction(symWriteI2CBus, false, (void *)cmd, (void *)clientKey, (void *)0, (void *)0);
	}

	return status;
}

IOReturn
IOI2CDevice::writeI2C(
	UInt32	subAddress,
	UInt8	*data,
	UInt32	count,
	UInt32	clientKey,
	UInt32	mode,
	UInt32	retries,
	UInt32	timeout_uS,
	UInt32	options)
{
	IOI2CCommand cmd = {0};

	cmd.command = kI2CCommand_Write;
	cmd.mode = mode;
	cmd.subAddress = subAddress;
	cmd.count = count;
	cmd.buffer = data;
	cmd.retries = retries;
	cmd.timeout_uS = timeout_uS;
	cmd.options = options;

	return writeI2C(&cmd, clientKey);
}

IOReturn
IOI2CDevice::readI2C(
	UInt32	subAddress,
	UInt8	*data,
	UInt32	count,
	UInt32	clientKey,
	UInt32	mode,
	UInt32	retries,
	UInt32	timeout_uS,
	UInt32	options)
{
	IOI2CCommand cmd = {0};

	cmd.command = kI2CCommand_Read;
	cmd.mode = mode;
	cmd.subAddress = subAddress;
	cmd.count = count;
	cmd.buffer = data;
	cmd.retries = retries;
	cmd.timeout_uS = timeout_uS;
	cmd.options = options;

	return readI2C(&cmd, clientKey);
}


/*******************************************************************************
 * Public Call Platform Function Method...
 *******************************************************************************/

#pragma mark  
#pragma mark *** External client callPlatformFunction method ***
#pragma mark  

IOReturn
IOI2CDevice::callPlatformFunction(
	const OSSymbol *functionName,
	bool waitForFunction,
	void *param1, void *param2,
	void *param3, void *param4 )
{
	IOReturn		status;

	if (0 == functionName)
		return kIOReturnBadArgument;

	if (0 == (fStateFlags & kStateFlags_TEARDOWN))
	{
		if (symReadI2CBus->isEqualTo(functionName))
			return readI2C((IOI2CCommand *)param1, (UInt32)param2);
		else
		if (symWriteI2CBus->isEqualTo(functionName))
			return writeI2C((IOI2CCommand *)param1, (UInt32)param2);
		else
		if (symLockI2CBus->isEqualTo(functionName))
			return lockI2CBus((UInt32 *)param2);
		else
		if (symUnlockI2CBus->isEqualTo(functionName))
			return unlockI2CBus((UInt32)param2);
		else
		if (symClientRead->isEqualTo(functionName))
			return readI2C((UInt32)param1, (UInt8 *)param2, (UInt32)param3, (UInt32)param4);
		else
		if (symClientWrite->isEqualTo(functionName))
			return writeI2C((UInt32)param1, (UInt8 *)param2, (UInt32)param3, (UInt32)param4);
		else
		if (functionName->isEqualTo("IOI2CSetDebugFlags"))
		{
			UInt32 flags = ( (UInt32)param1 & ( kStateFlags_IOLog | kStateFlags_kprintf ) );
			DLOG("IOI2CDevice@%lx IOI2CSetDebugFlags:%lx %s\n", (unsigned long int)getI2CAddress(), (unsigned long int)flags, ((UInt32)param2 == true)?"TRUE":"FALSE");
			if ((UInt32)param2 == true)
				fStateFlags |= flags;	// set the debug flags
			else
				fStateFlags &= ~flags;	// clear the debug flags
			return kIOReturnSuccess;
		}

		// If no other symbol matched - check for OnDemand platform function.
		if (fEnableOnDemandPlatformFunctions)
		{
			const char *cstr;
			if ((cstr = functionName->getCStringNoCopy()) && (0 == strncmp("platform-do-", cstr, strlen("platform-do-"))))
			{
				IOPlatformFunction *pfFunc;

				if (kIOReturnSuccess == (status = getPlatformFunction(functionName, &pfFunc, kIOPFFlagOnDemand)))
					return performFunction (pfFunc, param1, param2, param3, param4);

				// If the function wasn't found then forward the request to our provider...
				// But if some other error occurred then return the status now.
				if (kIOReturnNotFound != status)
					return status;
			}
		}

	} // kStateFlags_TEARDOWN

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

#pragma mark  
#pragma mark *** IOPlatformFunction API methods ***
#pragma mark  

/*******************************************************************************
 * IOPlatformFunction API Methods...
 *******************************************************************************/

IOReturn
IOI2CDevice::InitializePlatformFunctions(void)
{
	IOReturn status = kIOReturnSuccess;
	const OSSymbol *temp_sym;

	// if already initialized? return success.
	if (fPlatformFuncArray)
		return kIOReturnSuccess;

	IOPlatformExpert *pexpert = OSDynamicCast(IOPlatformExpert, getPlatform());
	if (0 == pexpert)
	{
		ERRLOG("IOI2CDevice::InitializePlatformFunctions ERROR no platform expert\n");
		return kIOReturnSuccess;
	}
		

	// Allocate a temporary symbol for the CPF..
	if (temp_sym = OSSymbol::withCString("InstantiatePlatformFunctions"))
	{
		// Have PE scan for platform-do-xxx functions...
		// Any IOPlatformFunctions found are returned in an OSArray.
		// The OSArray returned in fPlatformFuncArray has a retain count of 1.
		// Each IOPlatformFunction is retained by the OSArray.

		status = pexpert->callPlatformFunction(temp_sym, false, (void *)fProvider, (void *)&fPlatformFuncArray, NULL, NULL);

		temp_sym->release();
	}

	DLOG("IOI2CDevice::InitializePlatformFunctions returned from platform expert:%x\n", status);

	// Publish any IOPF(n)s with "on demand" or "interrupt" flags set...
	if ((status == kIOReturnSuccess) && fPlatformFuncArray)
	{
		IOPlatformFunction	*func;
		UInt32 i;
		UInt32 count;
		UInt32 flags;

		count = fPlatformFuncArray->getCount();
		for (i = 0; i < count; i++)
		{
			if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
			{
				flags = func->getCommandFlags();

				if ((flags & kIOPFFlagOnDemand) || (flags & kIOPFFlagIntGen))
					func->publishPlatformFunction(this);
			}
		}
	}

	return status;
}

IOReturn
IOI2CDevice::getPlatformFunction (
	const OSSymbol		*functionSym,
	IOPlatformFunction	**funcRef,
	UInt32				flags)
{
	IOReturn			status = kIOReturnNotFound;
	UInt32				count, i;
	IOPlatformFunction	*func;

	#define kIOI2CPFFlagsMask (kIOPFFlagOnInit|kIOPFFlagOnTerm|kIOPFFlagOnSleep|kIOPFFlagOnWake|kIOPFFlagOnDemand)
	if (flags == 0)
		flags = kIOI2CPFFlagsMask;

	if (fPlatformFuncArray)
	{
		count = fPlatformFuncArray->getCount();
		for (i = 0; i < count; i++)
		{
			if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
			{
				if (functionSym->isEqualTo(func->getPlatformFunctionName()))
				{
					if (func->getCommandFlags() & flags)
					{
						status = kIOReturnSuccess;
						*funcRef = func;
						break;
					}
				}
			}
		}
	}

	return status;
}

/*******************************************************************************
 * Perform any platform functions which contain the specified flags
 *******************************************************************************/

void
IOI2CDevice::performFunctionsWithFlags(
	UInt32				flags)
{
	UInt32				count, i;
	IOPlatformFunction	*func;

	if (0 == fPlatformFuncArray)
		return;

	// Execute any functions flagged as "on sleep"
	count = fPlatformFuncArray->getCount();
	for (i = 0; i < count; i++)
	{
		if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
		{
			if (func->getCommandFlags() & flags)
				performFunction(func);
		}
	}
}

/*******************************************************************************
 * Execute a platform function - performFunction()
 *******************************************************************************/
IOReturn
IOI2CDevice::performFunction (
	const char			*funcName,
	void				*pfParam1,
	void				*pfParam2,
	void				*pfParam3,
	void				*pfParam4)
{
	IOReturn			status;
	const OSSymbol		*funcSym;

	if (0 == (funcSym = OSSymbol::withCString(funcName)))
		return kIOReturnNoMemory;
	status = performFunction(funcSym, pfParam1, pfParam2, pfParam3, pfParam4);
	funcSym->release();
	return status;
}

IOReturn
IOI2CDevice::performFunction (
	const OSSymbol		*funcSym,
	void				*pfParam1,
	void				*pfParam2,
	void				*pfParam3,
	void				*pfParam4)
{
	IOReturn			status;
	IOPlatformFunction	*func;

	if (kIOReturnSuccess != (status = getPlatformFunction (funcSym, &func, kIOPFFlagOnDemand)))
		return status;
	return performFunction(func, pfParam1, pfParam2, pfParam3, pfParam4);
}

IOReturn
IOI2CDevice::performFunction(
	IOPlatformFunction			*func,
	void						*pfParam1,
	void						*pfParam2,
	void						*pfParam3,
	void						*pfParam4)
{
	IOReturn					status = kIOReturnSuccess;
	IOPlatformFunctionIterator 	*iter;
	UInt8						scratchBuffer[kI2CPF_READ_BUFFER_LEN] = {0};
	UInt8						readBuffer[kI2CPF_READ_BUFFER_LEN] = {0};
	UInt32						mode = kI2CMode_Unspecified;
	UInt8						*maskBytes, *valueBytes;
	unsigned					delayMS;
	UInt32 						cmd, cmdLen, param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;

	UInt32						key = 0;
	bool						i2cIsLocked = FALSE;
	bool						isI2CFunction = FALSE;

	DLOG ("IOI2CDevice::performFunction(%lx) - entered\n", fI2CAddress);

	if (!func)
		return kIOReturnBadArgument;

	if (!(iter = func->getCommandIterator()))
		return kIOReturnNotFound;

	// Check for I2C function...
	while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, (UInt32 *)&status)
		&& (status != kIOPFNoError))
	{
		if ((cmd == kCommandReadI2CSubAddr) || (cmd == kCommandWriteI2CSubAddr))
		{
			isI2CFunction = TRUE;
			break;
		}
	}

	if (status == kIOReturnSuccess)
	{
		if (isI2CFunction)
		{
			if (kIOReturnSuccess == (status = lockI2CBus(&key)))
				i2cIsLocked = TRUE;
		}
	}

	if (status == kIOReturnSuccess)
	{
		iter->reset();

		while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
			&param5, &param6, &param7, &param8, &param9, &param10, (UInt32 *)&status)
			&& (status != kIOPFNoError))
		{
			DLOG ("IOI2CDevice::performFunction(%lx) - 1)0x%lx, 2)0x%lx, 3)0x%lx, 4)0x%lx, 5)0x%lx,"
					"6)0x%lx, 7)0x%lx, 8)0x%lx, 9)0x%lx, 10)0x%lx\n", getI2CAddress(), param1, param2, param3,
					param4, param5, param6, param7, param8, param9, param10);
	
			switch (cmd)
			{
				case kCommandDelay:
					delayMS = param1 / 1000; // convert param1 from uS to mS.
					DLOG("IOI2CDevice::performFunction(%lx) delay %u\n", getI2CAddress(), delayMS);
					if (delayMS != 0)
						IOSleep(delayMS);
					break;

				case kCommandReadI2CSubAddr:
					if (param2 > kI2CPF_READ_BUFFER_LEN)
					{
						status = kIOPFBadCmdLength;
						ERRLOG("IOI2CDevice::performFunction(%lx) r-sub operation too big!\n", getI2CAddress());
						break;
					}
	
					if (mode == kI2CMode_Unspecified)
					{
						status = kIOReturnUnsupportedMode;
						ERRLOG("IOI2CDevice::performFunction(%lx) Rd Unspecified I2C mode\n", getI2CAddress());
						break;
					}

					status = readI2C(param1, readBuffer, param2, key, mode);
					break;

				case kCommandWriteI2CSubAddr:
					if (mode == kI2CMode_Unspecified)
					{
						status = kIOReturnUnsupportedMode;
						ERRLOG("IOI2CDevice::performFunction(%lx) Wt Unspecified I2C mode\n", getI2CAddress());
						break;
					}

					DLOG("IOI2CDevice::performFunction(%lx) w-sub %lx len %lx data", getI2CAddress(), param1, param2);

					status = writeI2C((UInt32) param1, (UInt8 *) param3, (UInt32) param2, key, mode);
					break;

				case kCommandI2CMode:
					switch (param1)
					{
						default:
						case kPFMode_Dumb:			status = kIOReturnUnsupportedMode;	break;
						case kPFMode_Standard:		mode = kI2CMode_Standard;			break;
						case kPFMode_Subaddress:	mode = kI2CMode_StandardSub;		break;
						case kPFMode_Combined:		mode = kI2CMode_Combined;			break;
					}

					DLOG("IOI2CDevice::performFunction(%lx) PF mode %lx\n", getI2CAddress(), mode);
					break;

				case kCommandRMWI2CSubAddr:
					// check parameters
					if ((param2 > kI2CPF_READ_BUFFER_LEN) ||	// number of mask bytes
						(param3 > kI2CPF_READ_BUFFER_LEN) ||	// number of value bytes
						(param4 > kI2CPF_READ_BUFFER_LEN) ||	// number of transfer bytes
						(param3 > param2))	// param3 is not actually used, we assume that
											// any byte that is masked also gets a value OR'ed in.
					{
						ERRLOG("IOI2CDevice::performFunction(%lx) invalid mw-sub cycle\n", getI2CAddress());
						status = kIOReturnAborted;
						break;
					}

					if (mode == kI2CMode_Unspecified)
					{
						status = kIOReturnUnsupportedMode;
						ERRLOG("IOI2CDevice::performFunction(%lx) RMW Unspecified I2C mode\n", getI2CAddress());
						break;
					}

					maskBytes = (UInt8 *) param5;
					valueBytes = (UInt8 *) param6;
	
					// Do the modify write operation on the previously read data buffer.
					for (unsigned int index = 0; index < param2; index++) // param2 = number of mask bytes
					{
						scratchBuffer[index] = ((valueBytes[index] & maskBytes[index]) |
												(readBuffer[index] & ~maskBytes[index]));
					}

					DLOG("IOI2CDevice::performFunction(%lx) mw-sub %lx len %lx data", getI2CAddress(), param1, param4);
					status = writeI2C((UInt8) param1, scratchBuffer, (UInt16) param4, key, mode);
					break;
	
				default:
					ERRLOG ("IOI2CDevice::performFunction - bad command %ld\n", cmd);
					status = kIOReturnAborted;
					break;
			}

			if (status != kIOReturnSuccess)
				break;
		}
	}

	if (iter)
		iter->release();

	if (i2cIsLocked)
		unlockI2CBus(key);

	DLOG ("IOI2CDevice::performFunction - done status: %x\n", status);
	return status;
}

#pragma mark  
#pragma mark *** Space reserved for future IOI2CDevice expansion ***
#pragma mark  

OSMetaClassDefineReservedUsed ( IOI2CDevice, 0 );

IOReturn
IOI2CDevice::getPlatformFunction (
		const char		*functionName,
		IOPlatformFunction	**funcRef,
		UInt32				flags)
{
	IOReturn status;
	const OSSymbol *functionSym = OSSymbol::withCStringNoCopy(functionName);
	status = getPlatformFunction(functionSym, funcRef, flags);
	functionSym->release();
	return status;
}

OSMetaClassDefineReservedUnused ( IOI2CDevice, 1 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 2 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 3 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 4 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 5 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 6 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 7 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 8 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 9 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 10 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 11 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 12 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 13 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 14 );
OSMetaClassDefineReservedUnused ( IOI2CDevice, 15 );
