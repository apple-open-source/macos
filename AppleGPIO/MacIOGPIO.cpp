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
#include <IOKit/IODeviceTreeSupport.h>
#include "MacIOGPIO.h"

#define super IOService

OSDefineMetaClassAndStructors(MacIOGPIO, IOService)

bool MacIOGPIO::init(OSDictionary *dict)
{
	fKeyLargoDrv		= 0;

	fGPIOBaseAddress	= 0;

	fClients = 0;
	fClientLock = 0;
	fWorkLoop = 0;
	
    return super::init(dict);
}

void MacIOGPIO::free(void)
{
    super::free();
}

IOService *MacIOGPIO::probe(IOService *provider, SInt32 *score)
{
	// Nothing to do -- name match is sufficient.
	// Personality specifies probe score of 10000.
    return(this);
}

bool MacIOGPIO::start(IOService *provider)
{
	OSData *regprop;
	
	if (!super::start(provider)) return(false);

	// Get GPIO register base address
	if ((regprop = OSDynamicCast(OSData, provider->getProperty("reg"))) == 0)
		return(false);
	else
	{
		fGPIOBaseAddress  = *(UInt32 *)regprop->getBytesNoCopy();
	}
	
	// Find keylargo
	fKeyLargoDrv = waitForService(serviceMatching("KeyLargo"));
	if (!fKeyLargoDrv) return(false);

	// grab a pointer to provider's workloop
	fWorkLoop = getWorkLoop();

	// allocate the client lock
	fClientLock = IOLockAlloc();
	if (!fClientLock) return(false);

	// Allocate our constant callPlatformFunction symbols so we can be called at interrupt time.

    fSymKeyLargoSafeWriteRegUInt8 = OSSymbol::withCString(kKeyLargoSafeWriteRegUInt8);
    fSymKeyLargoSafeReadRegUInt8 = OSSymbol::withCString(kKeyLargoSafeReadRegUInt8);

	// start matching on gpios
	publishBelow(provider);

	return(true);
}

void MacIOGPIO::stop(IOService *provider)
{
	MacIOGPIOCallbackInfo *thisClient, *nextClient;

	IOLockLock(fClientLock);

	// FREE fClients LIST MEMORY AND DISABLE/DESTROY EVENT SOURCES
	if (fClients)
	{
		thisClient = fClients;
		while (thisClient)
		{
			nextClient = thisClient->next;
			thisClient->eventSource->disable();
			fWorkLoop->removeEventSource(thisClient->eventSource);
			thisClient->eventSource->release();
			IOFree(thisClient, sizeof(MacIOGPIOCallbackInfo));
			thisClient = nextClient;
		}
	}
	
	fClients = 0;
	
	IOLockUnlock(fClientLock);
	IOLockFree(fClientLock);
	fClientLock = 0;
	fWorkLoop = 0;

	fGPIOBaseAddress = 0;
	
    super::stop(provider);
}

IOReturn MacIOGPIO::callPlatformFunction(const OSSymbol *functionName,
                                         bool waitForFunction,
                                         void *param1, void *param2,
                                         void *param3, void *param4)
{
	const char 	*functionNameStr;
	IOReturn	result = kIOReturnUnsupported;
	UInt32		offset;
	UInt8		regval;

	if (functionName == NULL) return kIOReturnBadArgument;
	functionNameStr = functionName->getCStringNoCopy();

	DLOG("MacIOGPIO::callPlatformFunction %s %s %08lx %08lx %08lx %08lx\n",
			functionNameStr, waitForFunction ? "TRUE" : "FALSE",
			(UInt32)param1, (UInt32)param2, (UInt32)param3, (UInt32)param4);

	if (strcmp(functionNameStr, kSymGPIOParentIntCapable) == 0)
	{
		*((UInt32 *)param1) = 1;
		result = kIOReturnSuccess;
	}
	else if (strcmp(functionNameStr, kSymGPIOParentWriteGPIO) == 0)
	{
		offset = (UInt32)param1;

//		DLOG("MacIOGPIO::callPlatformFunction reading keylargo addr 0x%08lx\n", offset);

		offset += fGPIOBaseAddress;

		// This call does a locked read-modify-write on key largo
		result = fKeyLargoDrv->callPlatformFunction(fSymKeyLargoSafeWriteRegUInt8,
				false, (void *)offset, (void *)param3, (void *)param2, 0);
	}
	else if (strcmp(functionNameStr, kSymGPIOParentReadGPIO) == 0)
	{
		offset = (UInt32)param1;

		offset += fGPIOBaseAddress;

		result = fKeyLargoDrv->callPlatformFunction(fSymKeyLargoSafeReadRegUInt8,
				false, (void *)offset, (void *)&regval, 0, 0);

		// if the read succeeded, return
		if (result == kIOReturnSuccess)
		{
			DLOG("MacIOGPIO::callPlatformFunction read value 0x%02x\n", regval);
			*(UInt32 *)param2 = (UInt32)regval;
		}
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
		DLOG("MacIOGPIO::callPlatformFunction didn't recognize function\n");

		result = super::callPlatformFunction(functionName, waitForFunction,
				param1, param2, param3, param4);
	}
	
	return result;
}

/* act as though this method was never implemented */
IOReturn MacIOGPIO::callPlatformFunction(const char *functionName,
                                         bool waitForFunction,
                                         void *param1, void *param2,
                                         void *param3, void *param4 )
{
	return(super::callPlatformFunction(functionName,
			waitForFunction, param1, param2, param3, param4));
}

IOService *MacIOGPIO::createNub( IORegistryEntry * from )
{
	IOService *nub;

    nub = new MacIOGPIODevice;

	if (nub && !nub->init( from, gIODTPlane ))
	{
		nub->free();
		nub = 0;
    }

    return(nub);
}

void MacIOGPIO::processNub(IOService *myNub)
{
}

void MacIOGPIO::publishBelow(IOService *root)
{
	OSCollectionIterator	*kids;
	IORegistryEntry			*next;
	IOService				*nub;
	OSData					*compat;
	bool					gpio;
	int						strLen;
	const char				*strStart, *strCur;
    bool					publishAll;
    
    publishAll = root->getProperty("preserveIODeviceTree") != 0;

    if(publishAll)
        kids = IODTFindMatchingEntries( root, kIODTRecursive, NULL);
    else
	// publish generic gpios below us, minus excludeList
        kids = IODTFindMatchingEntries( root, kIODTRecursive | kIODTExclusive,
			"('programmer-switch')");

	if (kids)
	{
		while((next = (IORegistryEntry *)kids->getNextObject()) != 0)
		{
			// make sure "gpio" is listed in the compatible property
			gpio = false;
			compat = OSDynamicCast(OSData, next->getProperty("compatible"));
			if (compat)
			{
                strLen = compat->getLength();
                strStart = strCur = (const char *)compat->getBytesNoCopy();
                while ((strCur - strStart) < strLen)
                {
                    if (strcmp(strCur, "gpio") == 0)
                    {
                        gpio = true;
                        break;  // stop iterating on inner loop
                    }
    
                    strCur += strlen(strCur) + 1;
                }
            }
			if(!gpio || ((nub = createNub(next)) == 0))
			{
                if(publishAll)
                    root->callPlatformFunction("mac-io-publishChild", false, this, next, 0, 0);
                else
                    DLOG("Not creating nub for %s\n", next->getName());
                continue;
			}

			nub->attach(this);
			processNub(nub);
			nub->registerService();
		}

		kids->release();
	}
}

bool MacIOGPIO::registerClient(void *param1, void *param2,
		void *param3, void *param4)
{
	MacIOGPIOCallbackInfo	*newClient, *tmpClient;
	IOInterruptEventSource	*eventSource;
	int						intType;
	IOService				*gpio;
	IOReturn				status;

	DLOG("MacIOGPIO::registerClient %08lx %08lx %08lx %08lx\n",
			(UInt32)param1, (UInt32)param2, (UInt32)param3, (UInt32)param4);

	// verify args:
	// param1 is gpioid (don't care about this one)
	// param2 is provider
	// param3 is handler
	// param4 is self pointer
	if ((param2 == 0) || (param3 == 0) || (param4 == 0)) return(false);

	// check to make sure the provider has valid interrupt information encoded
	// in it -- creating an event source does not check this.
	if ((gpio = OSDynamicCast(IOService, (OSMetaClassBase *) param2)) == NULL)
		return(false);

	// IOService::getInterruptType attempts to decode the interrupt-related
	// properties and interrogate MPIC to find out the interrupt type.  It can
	// return a number of things depending on whether it succeeded or, if it,
	// failed, where/how it failed.  It doesn't look like it will ever return
	// kIOReturnSuccess, instead it returns "edge" or "level" triggered on success,
	// and something else on failure ("no interrupt", "no resources", etc.)
	status = gpio->getInterruptType( 0, &intType );
	if (status != kIOInterruptTypeEdge && status != kIOInterruptTypeLevel)
		return(false);

	// This call basically registers MacIOGPIO to receive the child gpio's
	// interrupts.  When MacIOGPIO receives interrupt events, it will look
	// through it's list of clients, find the one associated with the calling
	// event source, and notify it in the way AppleGPIO expects to be called.
	// We have to do this to generalize the event notification mechanism used
	// in AppleGPIO (although it does add a small bit of overhead).
	eventSource = IOInterruptEventSource::interruptEventSource(this,
			(IOInterruptEventAction) &MacIOGPIO::interruptOccurred,
			(IOService *)param2, 0);

	if (eventSource == 0) return(false);

	// add to workloop
	if (fWorkLoop->addEventSource(eventSource) != kIOReturnSuccess)
	{
		eventSource->release();
		return(false);
	}

	// create client entry
	if ((newClient =
		(MacIOGPIOCallbackInfo *)IOMalloc(sizeof(MacIOGPIOCallbackInfo))) == 0)
	{
		fWorkLoop->removeEventSource(eventSource);
		eventSource->release();
		return(false);
	}

	// set up client entry
	newClient->eventSource	= eventSource;
	newClient->handler		= (GPIOEventHandler)param3;
	newClient->self 		= param4;
	newClient->next			= 0;

	// add client entry to list
	IOLockLock(fClientLock);

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

	IOLockUnlock(fClientLock);

	DLOG("MacIOGPIO::registerClient succeeded!\n");

	return(true);
}

#ifdef CLIENT_MATCH
#undef CLIENT_MATCH
#endif

#define CLIENT_MATCH(client) \
(((client)->handler	== (GPIOEventHandler)param3) &&		\
 ((client)->self	== param4))

bool MacIOGPIO::unregisterClient(void *param1, void *param2,
		void *param3, void *param4)
{
	MacIOGPIOCallbackInfo *prevClient, *thisClient;
	bool found = false;

	DLOG("MacIOGPIO::unregisterClient %08lx %08lx %08lx %08lx\n",
			(UInt32)param1, (UInt32)param2, (UInt32)param3, (UInt32)param4);
	
	if (fClients == 0) return(false);

	// search for the calling client and remove if found
	// check for match against first client in list
	if (CLIENT_MATCH(fClients))
	{
		thisClient = fClients;
		
		IOLockLock(fClientLock);
		fClients = fClients->next;
		IOLockUnlock(fClientLock);
		
		// thisClient points to client to be deleted
		found = true;
	}
	else
	{
		thisClient = fClients;
		while (thisClient->next != 0)
		{
			prevClient = thisClient;
			thisClient = thisClient->next;

			if (CLIENT_MATCH(thisClient))
			{
				IOLockLock(fClientLock);
				prevClient->next = thisClient->next;
				IOLockUnlock(fClientLock);
				
				// thisClient points to client to be deleted
				found = true;
				break;
			}
		}
	}
	
	if (found)
	{
		thisClient->eventSource->disable();
		fWorkLoop->removeEventSource(thisClient->eventSource);
		thisClient->eventSource->release();
		IOFree(thisClient, sizeof(MacIOGPIOCallbackInfo));
		DLOG("MacIOGPIO::unregisterClient succeeded!\n");
	}

	return(found);
}

bool MacIOGPIO::enableClient(void *param1, void *param2, void *param3,
				void *param4)
{
	MacIOGPIOCallbackInfo *client;
	bool found = false;

	client = fClients;

	while (client)
	{
                if (CLIENT_MATCH(client))
		{
			client->eventSource->enable();
			found = true;
			break;
		}
		client = client->next;
	}

	return(found);
}


bool MacIOGPIO::disableClient(void *param1, void *param2, void *param3,
				void *param4)
{
	MacIOGPIOCallbackInfo *client;
	bool found = false;

	client = fClients;

	while (client)
	{
                if (CLIENT_MATCH(client))
		{
			client->eventSource->disable();
			found = true;
			break;
		}
		client = client->next;
	}

	return(found);
}

void MacIOGPIO::handleInterrupt(IOInterruptEventSource *source, int count)
{
	MacIOGPIOCallbackInfo	*client;
	GPIOEventHandler		handler;

	DLOG("MacIOGPIO::handleInterrupt got event!!\n");

	// walk through client list, find matching event source, call back
	// to client passing self = fClients->self, newData = 0, and last two
	// params are zeros.  Note that AppleGPIO clients of MacIOGPIO get
	// event notification without any data - they have to read the register
	// themselves if they want to know the new register state.

	client = fClients;

	while (client != 0)
	{
		if (client->eventSource == source)
		{
			handler = client->handler;
			handler(client->self, 0, 0, 0);
			break;
		}
		client = client->next;
	}
}

void MacIOGPIO::interruptOccurred(OSObject *me, IOInterruptEventSource *source,
		int count)
{
	DLOG("MacIOGPIO::interruptOccurred got callback!!\n");

	MacIOGPIO *self = (MacIOGPIO *)me;
	
	if (self) self->handleInterrupt(source, count);
}

/*---  MacIOGPIO device nub class ---*/

#ifdef super
#undef super
#endif

#define super IOService

OSDefineMetaClassAndStructors(MacIOGPIODevice, IOService)

bool MacIOGPIODevice::compareName(OSString *name, OSString **matched = 0) const
{
	return(IODTCompareNubName(this, name, matched)
			|| super::compareName(name, matched));
}
