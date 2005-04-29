/*
 * Copyright (c) 2002-2003 Apple Computer, Inc.  All rights reserved.
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
#include <IOKit/IOKitKeys.h>
#include <IOKit/IODeviceTreeSupport.h>
#include "AppleGPIO.h"

#define super IOService

OSDefineMetaClassAndStructors(AppleGPIO, IOService)

bool AppleGPIO::init(OSDictionary *dict)
{
	bool ret;

	ret = super::init(dict);

	// Initialize instance variables
	fParent = 0;
	fGPIOID = kGPIOIDInvalid;

	fIntGen = false;

	fPlatformFuncArray = 0;

	fAmRegistered = 0;
	fAmRegisteredLock = 0;

	fAmEnabled = 0;
	fAmEnabledLock = 0;
	
	fClients = 0;
	fClientsLock = 0;

	fSymIntRegister 	= 0;
	fSymIntUnRegister 	= 0;
	fSymIntEnable 		= 0;
	fSymIntDisable		= 0;

#ifdef OLD_STYLE_COMPAT

	fRegisterStrings = 0;
	fUnregisterStrings = 0;
	fEnableStrings = 0;
	fDisableStrings = 0;

#endif	

	return(ret);
}

void AppleGPIO::free(void)
{
    super::free();
}

IOService *AppleGPIO::probe(IOService *provider, SInt32 *score)
{
	*score = 5000;
	return(this);
}

bool AppleGPIO::start(IOService *provider)
{
	bool					doSleepWake;
	UInt32					i, flags, intCapable;
	IOPlatformFunction		*func;
	const OSSymbol			*functionSymbol = OSSymbol::withCString("InstantiatePlatformFunctions");
	IOReturn				retval;
	IOService				*parentDev;
	OSData					*data;

	if (!super::start(provider)) return(false);
	
	// set my id
	data = OSDynamicCast(OSData, provider->getProperty("reg"));
	if (!data) return(false);
	
	fGPIOID = *(UInt32 *)data->getBytesNoCopy();
	
	// find the gpio parent
	parentDev = OSDynamicCast(IOService, provider->getParentEntry(gIODTPlane));
	if (!parentDev) return(false);

	fParent = OSDynamicCast(IOService, parentDev->getChildEntry(gIOServicePlane));
	if (!fParent) return(false);

	// Create the interrupt register/enable symbols
	fSymIntRegister 	= OSSymbol::withCString(kIOPFInterruptRegister);
	fSymIntUnRegister 	= OSSymbol::withCString(kIOPFInterruptUnRegister);
	fSymIntEnable 		= OSSymbol::withCString(kIOPFInterruptEnable);
	fSymIntDisable		= OSSymbol::withCString(kIOPFInterruptDisable);

	// Allocate our constant callPlatformFunction symbols so we can be called at interrupt time.

	fSymGPIOParentWriteGPIO = OSSymbol::withCString(kSymGPIOParentWriteGPIO);
	fSymGPIOParentReadGPIO = OSSymbol::withCString(kSymGPIOParentReadGPIO);

	// Scan for platform-do-xxx functions
	fPlatformFuncArray = NULL;

	DLOG("AppleGPIO::start(%s@%lx) - calling InstantiatePlatformFunctions\n",
			provider->getName(), fGPIOID);

	retval = provider->getPlatform()->callPlatformFunction(functionSymbol, false,
			(void *)provider, (void *)&fPlatformFuncArray, (void *)0, (void *)0);

	DLOG("AppleGPIO::start(%s@%lx) - InstantiatePlatformFunctions returned %ld, pfArray %sNULL\n",
			provider->getName(), fGPIOID, retval, fPlatformFuncArray ? "NOT " : "");

	if (retval == kIOReturnSuccess && (fPlatformFuncArray != NULL))
	{
		// Find out if the GPIO parent supports interrupt events
		if (fParent->callPlatformFunction(kSymGPIOParentIntCapable, false, &intCapable, 0, 0, 0)
				!= kIOReturnSuccess)
		{
			intCapable = 0;
		}

		DLOG("AppleGPIO::start(%s@%lx) - iterating platformFunc array, count = %ld\n",
			provider->getName(), fGPIOID, fPlatformFuncArray->getCount());

		doSleepWake = false;

		UInt32 count = fPlatformFuncArray->getCount();
		for (i = 0; i < count; i++)
		{
			if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
			{
				flags = func->getCommandFlags();

				DLOG ("AppleGPIO::start(%s@%lx) - functionCheck - got function, flags 0x%lx, pHandle 0x%lx\n", 
					provider->getName(), fGPIOID, flags, func->getCommandPHandle());

				// If this function is flagged to be performed at initialization, do it
				if (flags & kIOPFFlagOnInit)
				{
					performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
				}

				if ((flags & kIOPFFlagOnDemand) || ((flags & kIOPFFlagIntGen) && intCapable))
				{
					// Set the flag to indicate whether any of the platform functions are using
					// interrupts -- this is used to allocate some locks that are needed for this
					// functionality
					if ((flags & kIOPFFlagIntGen) && (fIntGen == false))
					{
						fIntGen = true;

						// Allocate event-related state variable locks
						fClientsLock = IOSimpleLockAlloc();
						fAmRegisteredLock = IOLockAlloc();
						fAmEnabledLock = IOSimpleLockAlloc();
					}

					// On-Demand and IntGen functions need to have a resource published
					func->publishPlatformFunction(this);
				}

				// If we need to do anything at sleep/wake time, we'll need to set this
				// flag so we know to register for notifications
				if (flags & (kIOPFFlagOnSleep | kIOPFFlagOnWake))
					doSleepWake = true;
			}
			else
			{
				// This function won't be used -- generate a warning
				IOLog("AppleGPIO::start(%s@%lx) - functionCheck - not an IOPlatformFunction object\n",
						getProvider()->getName(), fGPIOID);
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

			if (pmRootDomain != 0)
			{
				DLOG("AppleGPIO::start to acknowledge power changes\n");
				pmRootDomain->registerInterestedDriver(this);
			}
			else
			{
				IOLog("AppleGPIO failed to register PM interest!");
			}
		}
	}


#ifdef OLD_STYLE_COMPAT
	/*
	 * Legacy Support
	 *
	 * In the initial implementation, extra strings were published for event registration
	 * and deregistration as well as enable/disable functions.  In the IOPlatformFunction
	 * implementation, the client access these functions by calling the platform-xxx function
	 * and passing one of four OSSymbol objects (see fSymIntRegister, fSymIntUnRegister,
	 * fSymIntEnable, fSymIntDisable).  For now, we need to continue to support the old
	 * implementation.  The following code will generate and publish resources for these
	 * functions.
	 */

	if (fIntGen)
	{
		const OSSymbol 	*functionSym, *aKey;
		char			funcNameWithPrefix[160];
		const char		*funcName;

		UInt32 count = fPlatformFuncArray->getCount();
		for (i = 0; i < count; i++)
		{
			// Only publish strings for on-demand and int-gen functions
			if ((func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i))) != NULL &&
				(func->getCommandFlags() & (kIOPFFlagOnDemand | kIOPFFlagIntGen)) != NULL)
			{
				functionSym = func->getPlatformFunctionName();
				if (!functionSym)
					continue;
				else
					funcName = functionSym->getCStringNoCopy() + strlen(kFunctionRequiredPrefix);
			
				// register string list
				strcpy(funcNameWithPrefix, kFunctionRegisterPrefix);
				strcat(funcNameWithPrefix, funcName);

				if ((aKey = OSSymbol::withCString(funcNameWithPrefix)) == 0)
					continue;

				ADD_OBJ_TO_SET(aKey, fRegisterStrings);

				// unregister string list
				strcpy(funcNameWithPrefix, kFunctionUnregisterPrefix);
				strcat(funcNameWithPrefix, funcName);
				
				if ((aKey = OSSymbol::withCString(funcNameWithPrefix)) == 0)
					continue;
				
				ADD_OBJ_TO_SET(aKey, fUnregisterStrings);

				// register string list
				strcpy(funcNameWithPrefix, kFunctionEvtEnablePrefix);
				strcat(funcNameWithPrefix, funcName);

				if ((aKey = OSSymbol::withCString(funcNameWithPrefix)) == 0)
					continue;

				ADD_OBJ_TO_SET(aKey, fEnableStrings);

				// register string list
				strcpy(funcNameWithPrefix, kFunctionEvtDisablePrefix);
				strcat(funcNameWithPrefix, funcName);

				if ((aKey = OSSymbol::withCString(funcNameWithPrefix)) == 0)
					continue;

				ADD_OBJ_TO_SET(aKey, fDisableStrings);
			}
		}

		publishStrings(fRegisterStrings);
		publishStrings(fUnregisterStrings);
		publishStrings(fEnableStrings);
		publishStrings(fDisableStrings);
	}

#endif

	if (fPlatformFuncArray && fPlatformFuncArray->getCount() > 0)
	{
		registerService();
		return(true);
	}
	else
	{
		// No reason for me to be here
		return(false);
	}
}

void AppleGPIO::stop(IOService *provider)
{
	UInt32 flags, i;
	IOPlatformFunction *func;
	AppleGPIOCallbackInfo *thisClient, *nextClient;

	// Execute any functions flagged as "on termination"
	UInt32 count = fPlatformFuncArray->getCount();
	for (i = 0; i < count; i++)
	{
		if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
		{
			flags = func->getCommandFlags();

			if (flags & kIOPFFlagOnTerm) 
				performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
		}
	}

	// Unregister for interrupts
	if (fIntGen && amRegistered())
	{
		disableWithParent();
		unregisterWithParent();
		fIntGen = false;
	}
	
	IOSimpleLockLock(fClientsLock);
	
	if (fClients)
	{
		thisClient = fClients;
		while (thisClient)
		{
			nextClient = thisClient->next;
			IOFree(thisClient, sizeof(AppleGPIOCallbackInfo));
			thisClient = nextClient;
		}

		fClients = 0;
	}
	
	IOSimpleLockUnlock(fClientsLock);

	IOSimpleLockFree(fClientsLock); fClientsLock = 0;
	IOLockFree(fAmRegisteredLock); fAmRegisteredLock = 0;
	IOSimpleLockFree(fAmEnabledLock); fAmEnabledLock = 0;

	fParent = 0;
	fGPIOID = kGPIOIDInvalid;

	if (fSymIntRegister)	{ fSymIntRegister->release(); fSymIntRegister = 0; }
	if (fSymIntUnRegister)	{ fSymIntUnRegister->release(); fSymIntUnRegister = 0; }
	if (fSymIntEnable)		{ fSymIntEnable->release(); fSymIntEnable = 0; }
	if (fSymIntDisable)		{ fSymIntDisable->release(); fSymIntDisable = 0; }

#ifdef OLD_STYLE_COMPAT

	releaseStrings();
	
#endif

	super::stop(provider);
}

IOReturn AppleGPIO::callPlatformFunction(const OSSymbol *functionName,
                                         bool waitForFunction,
                                         void *param1, void *param2,
                                         void *param3, void *param4 )
{
	DLOG("AppleGPIO::callPlatformFunction %s %s %08lx %08lx %08lx %08lx\n",
			functionName->getCStringNoCopy(), waitForFunction ? "TRUE" : "FALSE",
			(UInt32)param1, (UInt32)param2, (UInt32)param3, (UInt32)param4);

	if (fPlatformFuncArray)
	{
		UInt32 i;
		IOPlatformFunction *pfFunc;
		
		UInt32 count = fPlatformFuncArray->getCount();
		for (i = 0; i < count; i++)
		{
			if (pfFunc = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
			{
				DLOG ("AppleGPIO::callPlatformFunction '%s' - got platformFunction object\n",
						functionName->getCStringNoCopy());

				// If param4 is an OSSymbol reference, check for interrupt event registration
				// or enable/disable symbols
				if (pfFunc->platformFunctionMatch(functionName, kIOPFFlagIntGen, NULL))
				{
					OSSymbol *subFunctionSym;
					
					DLOG ("AppleGPIO::callPlatformFunction '%s' - got platformFunction interrupt match\n",
							functionName->getCStringNoCopy());

					if (param4 && (subFunctionSym = OSDynamicCast(OSSymbol, (const OSMetaClassBase *)param4)))
					{
						DLOG ("AppleGPIO::callPlatformFunction '%s', subFunction '%s'\n",
								functionName->getCStringNoCopy(), subFunctionSym->getCStringNoCopy());

						if (subFunctionSym == fSymIntRegister)
							return (registerClient(param1, param2, param3) ? kIOReturnSuccess : kIOReturnBadArgument);
						if (subFunctionSym == fSymIntUnRegister)
							return (unregisterClient(param1, param2, param3) ? kIOReturnSuccess : kIOReturnBadArgument);
						if (subFunctionSym == fSymIntEnable)
							return (enableClient(param1, param2, param3) ? kIOReturnSuccess : kIOReturnBadArgument);
						if (subFunctionSym == fSymIntDisable)
							return (disableClient(param1, param2, param3) ? kIOReturnSuccess : kIOReturnBadArgument);
						return kIOReturnBadArgument;			// Unknown function
					}
					else
					{
						DLOG ("AppleGPIO::callPlatformFunction '%s', param4 not a OSSymbol\n",
								functionName->getCStringNoCopy());
					}
				}

				// Check for on-demand case
				if (pfFunc->platformFunctionMatch (functionName, kIOPFFlagOnDemand, NULL))
				{
					DLOG ("AppleGPIO::callPlatformFunction '%s', calling demand function\n",
							functionName->getCStringNoCopy());

					return (performFunction (pfFunc, param1, param2, param3, param4) ? kIOReturnSuccess : kIOReturnBadArgument);
				}

				DLOG ("AppleGPIO::callPlatformFunction '%s' - got no match\n", functionName->getCStringNoCopy());
			}
		}

#ifdef OLD_STYLE_COMPAT

		// Need to catch old-style register-, unregister-, enable-, disable- commands

		DLOG ("AppleGPIO::callPlatformFunction '%s' - handling old style\n", functionName->getCStringNoCopy());

		// Is it an interrupt notification registration request?
		if (fRegisterStrings &&
		    fRegisterStrings->containsObject(functionName))
		{
			return (registerClient(param1, param2, param3) ? kIOReturnSuccess : kIOReturnBadArgument);
		}
		// unregister?
		else if (fUnregisterStrings && 
		         fUnregisterStrings->containsObject(functionName))
		{
			return (unregisterClient(param1, param2, param3) ? kIOReturnSuccess : kIOReturnBadArgument);
		}
		else if (fEnableStrings &&
		         fEnableStrings->containsObject(functionName))
		{
			return (enableClient(param1, param2, param3) ? kIOReturnSuccess : kIOReturnBadArgument);
		}
		else if (fDisableStrings &&
		         fDisableStrings->containsObject(functionName))
		{
			return (disableClient(param1, param2, param3) ? kIOReturnSuccess : kIOReturnBadArgument);
		}

#endif

	}

	DLOG("AppleGPIO::callPlatformFunction unrecognized function\n");

	return super::callPlatformFunction(functionName, waitForFunction, param1, param2,
			param3, param4);

}

// Note that this is an overload of performFunction.  The other, extant performFunction
// should go away once this version is fully supported
bool AppleGPIO::performFunction(IOPlatformFunction *func, void *pfParam1 = 0,
			void *pfParam2 = 0, void *pfParam3 = 0, void *pfParam4 = 0)
{
	IOReturn					ret;
	IOPlatformFunctionIterator 	*iter;
	UInt32 						data, cmd, cmdLen, result, param1, param2, param3, param4, param5, 
									param6, param7, param8, param9, param10;
	
	DLOG ("AppleGPIO::performFunction - entered\n");
	if (!func)
		return false;
	
	if (!(iter = func->getCommandIterator()))
		return false;
	
	while (iter->getNextCommand (&cmd, &cmdLen, &param1, &param2, &param3, &param4, 
		&param5, &param6, &param7, &param8, &param9, &param10, &result)) {
		if (result != kIOPFNoError) {
			iter->release();
			return false;
		}
		DLOG ("AppleGPIO::performFunction - 1)0x%lx, 2)0x%lx, 3)0x%lx, 4)0x%lx, 5)0x%lx,"
				"6)0x%lx, 7)0x%lx, 8)0x%lx, 9)0x%lx, 10)0x%lx\n", param1, param2, param3,
				param4, param5, param6, param7, param8, param9, param10);

		switch (cmd) {
			case kCommandWriteGPIO:
				// if param is TRUE then use value, otherwise invert value
				if (pfParam1 == 0) param1 = ~param1;
				
				DLOG("AppleGPIO::performFunction writeGPIO value = %08lx mask = %08lx\n",
					param1, param2);

				// write the result to the GPIO register
				if ((ret = fParent->callPlatformFunction(fSymGPIOParentWriteGPIO,
					false, (void *)fGPIOID, (void *)param1, (void *)param2, 0)) != kIOReturnSuccess)
				{
					iter->release();
					return false;
				}
				break;
				
			case kCommandReadGPIO:
	
				// Is there a place to put the result?
				if ((pfParam1 == 0) || (pfParam1 == (void *)1)) return(false);
	
				// get the current register state
				if ((ret = fParent->callPlatformFunction(fSymGPIOParentReadGPIO,
						false, (void *)fGPIOID, (void *)&data, 0, 0))
						!= kIOReturnSuccess)
				{
					iter->release();
					return false;
				}
	
				//DLOG("AppleGPIO::performFunction got %08lx from read to parent\n",
				//		data);
	
				result   = (data & param1);
				result >>= param2;
				result  ^= param3;
				
				*(UInt32 *)pfParam1 = result;
	
				break;

			default:
				DLOG ("AppleGPIO::performFunction - bad command %ld\n", cmd);
				iter->release();
				return false;		        	    
		}
	}

	iter->release();

	DLOG ("AppleGPIO::performFunction - done\n");
	return true;
}

#pragma mark *** Parent Registration ***

bool AppleGPIO::registerWithParent(void)
{
	bool result;

	// grab the mutex
	IOLockLock(fAmRegisteredLock);

	// don't register twice
	if (fAmRegistered)
	{
		DLOG("AppleGPIO::registerWithParent already registered!!\n");
		IOLockUnlock(fAmRegisteredLock);
		return(true);
	}

	// register for notification from parent
	if (fParent->callPlatformFunction(kSymGPIOParentRegister, false,
			(void *)fGPIOID, (void *)getProvider(),
			(void *)&sGPIOEventOccured, (void *)this) == kIOReturnSuccess)
	{
		fAmRegistered = true;
		result = true;
	}
	else
	{
		DLOG("AppleGPIO::registerWithParent registration attempt failed\n");
		result = false;
	}

	// release mutex
	IOLockUnlock(fAmRegisteredLock);
	return(result);
}

void AppleGPIO::unregisterWithParent(void)
{
	// grab the mutex
	IOLockLock(fAmRegisteredLock);

	if (!fAmRegistered)
	{
		DLOG("AppleGPIO::unregisterWithParent not registered!!\n");
		IOLockUnlock(fAmRegisteredLock);
		return;
	}

	if (fParent->callPlatformFunction(kSymGPIOParentUnregister, false,
			(void *)fGPIOID, (void *)getProvider(),
			(void *)&sGPIOEventOccured,	(void *)this) == kIOReturnSuccess)
	{
		fAmRegistered = false;
	}
	else
	{
		DLOG("AppleGPIO::unregisterWithParent failed to unregister\n");
	}

	// release the mutex
	IOLockUnlock(fAmRegisteredLock);
}

bool AppleGPIO::amRegistered(void)
{
	bool result;

	IOLockLock(fAmRegisteredLock);

	result = fAmRegistered;

	IOLockUnlock(fAmRegisteredLock);

	return(result);
}

#pragma mark *** Parent Enable/Disable ***

void AppleGPIO::enableWithParent(void)
{
	DLOG("AppleGPIO::enableWithParent 0x%lx enabling notification\n",
			fGPIOID);

	// don't enable twice
	if (amEnabled())
	{
		DLOG("AppleGPIO::enableWithParent already enabled!!\n");
		return;
	}

	// enable notification from parent
	if ((!setAmEnabled(true)) ||
	    (fParent->callPlatformFunction(kSymGPIOParentEvtEnable, false,
			(void *)fGPIOID, (void *)getProvider(),
			(void *)&sGPIOEventOccured, (void *)this) != kIOReturnSuccess))
	{
		DLOG("AppleGPIO::enableWithParent enable attempt failed\n");
	}
}

void AppleGPIO::disableWithParent(void)
{
	if (!amEnabled())
	{
		DLOG("AppleGPIO::disableWithParent not enabled!!\n");
		return;
	}

	if ((!setAmEnabled(false)) ||
	    (fParent->callPlatformFunction(kSymGPIOParentEvtDisable, false,
			(void *)fGPIOID, (void *)getProvider(),
			(void *)&sGPIOEventOccured,	(void *)this) != kIOReturnSuccess))
	{
		DLOG("AppleGPIO::disableWithParent failed to disable\n");
	}
}

inline bool AppleGPIO::amEnabled(void)
{
	return(fAmEnabled);
}

// attempt to set the fAmEnabled flag.  If the flag is already set to
// the desired value, it indicates that another thread already changed
// it.  In this case, nothing is done and FALSE is returned.  If the change
// is successful, TRUE is returned and the flag is changed.
bool AppleGPIO::setAmEnabled(bool enabled)
{
	bool result = false;
	
	IOSimpleLockLock(fAmEnabledLock);

	if (enabled != fAmEnabled)
	{
		fAmEnabled = enabled;
		result = true;
	}
	
	IOSimpleLockUnlock(fAmEnabledLock);
	
	return(result);
}

#pragma mark *** Client Registration ***

#ifdef CLIENT_MATCH
#undef CLIENT_MATCH
#endif

#define CLIENT_MATCH(client) \
(((client)->handler	== (GPIOEventHandler)param1) &&		\
 ((client)->param1	== param2) &&						\
 ((client)->param2	== param3))

// param1 is a GPIOEventHandler
// param2, param3, param4 are anything the caller wants to give me, they'll
// be passed back exactly as given to me
bool AppleGPIO::registerClient(void *param1, void *param2,
		void *param3 = 0)
{
	AppleGPIOCallbackInfo *newClient, *tmpClient;

	DLOG("AppleGPIO::registerClient %08lx %08lx %08lx\n",
			(UInt32)param1, (UInt32)param2, (UInt32)param3);
	
	// verify the handler address 
	if (param1 == 0) return(false);

	// Make sure this isn't a dupe
	tmpClient = fClients;
	while (tmpClient)
	{
		if (CLIENT_MATCH(tmpClient)) return(false);
		tmpClient = tmpClient->next;
	}

	// Allocate memory for client
	newClient = (AppleGPIOCallbackInfo *)IOMalloc(sizeof(AppleGPIOCallbackInfo));
	if (!newClient) return(false);

	// store the client's data
	newClient->handler = (GPIOEventHandler)param1;
	newClient->param1 = param2;
	newClient->param2 = param3;
	newClient->isEnabled = true;	// events enabled upon registration
	newClient->next = 0;

	// grab the client list mutex
	IOSimpleLockLock(fClientsLock);

	// insert him into the list
	if (!fClients)
	{
		fClients = newClient;
	}
	else
	{
		tmpClient = fClients;
		while (tmpClient->next != 0) tmpClient = tmpClient->next;
		tmpClient->next = newClient;
	}
	
	// release the client list mutex
	IOSimpleLockUnlock(fClientsLock);

	// If necessary, register with GPIO parent for notification
	if (!amRegistered())
	{
		if (!registerWithParent())
		{
			// [3332930] If we can't register with the parent...
			// then return an error and leave the notifications disabled.
			return false;
		}
	}

	// Make sure parent is sending down events
	if (areEnabledClients() && !amEnabled())
		enableWithParent();

	return(true);
}

bool AppleGPIO::unregisterClient(void *param1, void *param2,
		void *param3 = 0)
{
	AppleGPIOCallbackInfo *prevClient, *thisClient;
	bool found = false;

	DLOG("AppleGPIO::unregisterClient %08lx %08lx %08lx\n",
			(UInt32)param1, (UInt32)param2, (UInt32)param3);

	// grab the client list mutex so nothing changes under our nose
	IOSimpleLockLock(fClientsLock);

	if (!areRegisteredClients())
	{
		IOSimpleLockUnlock(fClientsLock);
		DLOG("AppleGPIO::unregisterClient nobody is registered!!\n");
		return(false);
	}
	
	// Search for the calling client and remove if found
	// check for match against first client
	if (CLIENT_MATCH(fClients))
	{
		thisClient = fClients;
		fClients = fClients->next;
		found = true;
	}
	// walk through the list
	else
	{
		thisClient = fClients;
		while (thisClient->next != 0)
		{
			prevClient = thisClient;
			thisClient = thisClient->next;
			
			if (CLIENT_MATCH(thisClient))
			{
				prevClient->next = thisClient->next;
				found = true;
				break;
			}
		}
	}

	// release the client list mutex
	IOSimpleLockUnlock(fClientsLock);

	if (found)
	{
		IOFree(thisClient, sizeof(AppleGPIOCallbackInfo));

		// update notification relationship with parent
		if (!areEnabledClients() && amEnabled())
		{
			disableWithParent();
		}
		if (!areRegisteredClients())
		{
			unregisterWithParent();
		}
	}

	return(found);
}

inline bool AppleGPIO::areRegisteredClients(void)
{
	return(fClients != 0);
}

#pragma mark *** Client Enable/Disable ***

// This routine will often be called in interrupt context
bool AppleGPIO::enableClient(void *param1, void *param2, void *param3 = 0)
{
	AppleGPIOCallbackInfo *client;
	bool found = false;

	DLOG("AppleGPIO::enableClient %08lx %08lx %08lx\n",
			(UInt32)param1, (UInt32)param2, (UInt32)param3);

	//IOSimpleLockLock(fClientsLock);

	if (!areRegisteredClients())
	{
		//IOSimpleLockUnlock(fClientsLock);
		DLOG("AppleGPIO::enableClient nobody registered!!\n");
		return(false);
	}

	// Find the caller in the client list and change enabled state
	client = fClients;
	while (client)
	{
		if (CLIENT_MATCH(client))
		{
			found = true;
			if (client->isEnabled)
			{
				DLOG("AppleGPIO::enableClient already enabled\n");
			}
			else
			{
				client->isEnabled = true;
			}
			break;
		}
		client = client->next;
	}

	//IOSimpleLockUnlock(fClientsLock);

	// make sure we're getting notifications if necessary
	if (found)
	{
		if (!amEnabled())
		{
			enableWithParent();
		}
	}

	return(found);
}

// This routine will often be called in interrupt context
bool AppleGPIO::disableClient(void *param1, void *param2, void *param3 = 0)
{
	AppleGPIOCallbackInfo *client;
	bool found = false;

	DLOG("AppleGPIO::disableClient %08lx %08lx %08lx\n",
			(UInt32)param1, (UInt32)param2, (UInt32)param3);

	//IOSimpleLockLock(fClientsLock);

	if (!areRegisteredClients())
	{
		DLOG("AppleGPIO::disableClient nobody registered!!\n");
		//IOSimpleLockUnlock(fClientsLock);
		return(false);
	}

	// Find the caller in the client list and change enabled state
	client = fClients;
	while (client)
	{
		if (CLIENT_MATCH(client))
		{
			found = true;
			if (!client->isEnabled)
			{
				DLOG("AppleGPIO::disableClient already disabled\n");
			}
			else
			{
				client->isEnabled = false;
			}
			break;	
		}
		client = client->next;
	}

	//IOSimpleLockUnlock(fClientsLock);

	// if all clients are disabled, tell parent to stop notifying
	if (found)
	{
		if (!areEnabledClients() && amEnabled())
		{
			disableWithParent();
		}
	}

	return(found);
}

bool AppleGPIO::areEnabledClients(void)
{
	AppleGPIOCallbackInfo *client;
	bool enabled = false;

	if (!areRegisteredClients())
	{
		return(false);
	}
	else
	{
		client = fClients;
		while (client != 0)
		{
			if (client->isEnabled)
			{
				enabled = true;
				break;
			}
			client = client->next;
		}
		return(enabled);
	}
}

#pragma mark *** Event Callbacks ***

void AppleGPIO::handleEvent(void *newData, void *z1 = 0, void *z2 = 0)
{
	AppleGPIOCallbackInfo	*thisClient;
	GPIOEventHandler		handler;

	DLOG("AppleGPIO::handleEvent id=0x%02lx %08lx %08lx %08lx\n",
			fGPIOID, (UInt32)newData, (UInt32)z1, (UInt32)z2);

	// see if anyone wants to know about this event.
	if (fClients == 0) return;

	// notify those who do.
	thisClient = fClients;
	
	while (thisClient)
	{
		if (thisClient->isEnabled)
		{
			DLOG("AppleGPIO::handleEvent calling back to %08lx\n",
				(UInt32)(thisClient->handler));

			handler = thisClient->handler;
			handler(thisClient->param1, thisClient->param2, 0, newData);
		}
		thisClient = thisClient->next;
	}
}

// This will be called by the GPIO parent driver when an interrupt event
// occurs.  param1 is an AppleGPIO*
void AppleGPIO::sGPIOEventOccured(void *param1, void *param2,
		void *param3 = 0, void *param4 = 0)
{
	AppleGPIO *me;

	if (param1 == 0) return;

	me = (AppleGPIO *)param1;
	if (me) me->handleEvent(param2, param3, param4);
}

#pragma mark *** Power Management ***

IOReturn AppleGPIO::powerStateWillChangeTo (IOPMPowerFlags theFlags, unsigned long, IOService*)
{	
    if ( ! (theFlags & IOPMPowerOn) ) {
        // Sleep sequence:
		DLOG("AppleGPIO::powerStateWillChangeTo - sleep\n");
		doSleep();
   } else {
        // Wake sequence:
		DLOG("AppleGPIO::powerStateWillChangeTo - wake\n");
		doWake();
    }
	
    return IOPMAckImplied;
}

void AppleGPIO::doSleep(void)
{
	UInt32 flags, i;
	IOPlatformFunction *func;

	// Execute any functions flagged as "on termination"
	UInt32 count = fPlatformFuncArray->getCount();
	for (i = 0; i < count; i++)
	{
		if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
		{
			flags = func->getCommandFlags();

			if (flags & kIOPFFlagOnSleep) 
				performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
		}
	}
}

void AppleGPIO::doWake(void)
{
	UInt32 flags, i;
	IOPlatformFunction *func;

	// Execute any functions flagged as "on termination"
	UInt32 count = fPlatformFuncArray->getCount();
	for (i = 0; i < count; i++)
	{
		if (func = OSDynamicCast(IOPlatformFunction, fPlatformFuncArray->getObject(i)))
		{
			flags = func->getCommandFlags();

			if (flags & kIOPFFlagOnWake) 
				performFunction(func, (void *)1, (void *)0, (void *)0, (void *)0);
		}
	}
}

#ifdef OLD_STYLE_COMPAT

#pragma mark *** Old Style Compatibility Routines ***

/* These routines can go away completely once compatibility doesn't need to be
maintained anymore */

void AppleGPIO::publishStrings(OSCollection *strings)
{
	OSCollectionIterator	*strIter;
	OSSymbol				*key;

	if (!strings) return;

	strIter = OSCollectionIterator::withCollection(strings);

	if (strIter)
	{
		while ((key = OSDynamicCast(OSSymbol, strIter->getNextObject())) != 0)
		{
			//DLOG("AppleGPIO::publishStrings 0x%x %s\n",
			//	fGPIOID, key->getCStringNoCopy());
			publishResource(key, this);
		}

		strIter->release();
	}
}

void AppleGPIO::releaseStrings(void)
{
	if (fRegisterStrings)
	{
		fRegisterStrings->release();
		fRegisterStrings = 0;
	}

	if (fUnregisterStrings)
	{
		fUnregisterStrings->release();
		fUnregisterStrings = 0;
	}

	if (fEnableStrings)
	{
		fEnableStrings->release();
		fEnableStrings = 0;
	}

	if (fDisableStrings)
	{
		fDisableStrings->release();
		fDisableStrings = 0;
	}
}

#endif

