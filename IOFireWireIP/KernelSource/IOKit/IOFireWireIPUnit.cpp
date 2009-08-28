/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#include "IOFireWireIPUnit.h"
#include "IOFireWireIP.h"
#include <IOKit/IOMessage.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOConfigDirectory.h>
#include "IOFWIPDefinitions.h"

OSDefineMetaClassAndStructors(IOFireWireIPUnit, IOService)
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 1);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 2);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 3);


bool IOFireWireIPUnit::start(IOService *provider)
{
	fIPLocalNode	= NULL;
	fFWBusInterface = NULL;
	fStarted		= false;
	
    fDevice = OSDynamicCast(IOFireWireNub, provider);
    
    if ( not fDevice )
        return false;

	fDrb = NULL;
    
    if ( not IOService::start(provider))
        return (false);

	IOFireWireController *control = fDevice->getController();
	if ( not control )
        return (false);
	
	if ( not configureFWBusInterface(control) )
	{
		IOLog("IOFireWireIPUnit - configureFWBusInterface failed \n");
        return (false);
	}

	UWIDE	eui64;
	
    CSRNodeUniqueID	fwuid = fDevice->getUniqueID();

	eui64.hi = (UInt32)(fwuid >> 32);
	eui64.lo = (UInt32)(fwuid & 0xffffffff);

	fDrb = fFWBusInterface->initDRBwithDevice(eui64, fDevice, false);

    if ( not fDrb )
	{
		IOLog("IOFireWireIPUnit - initDRBwithDevice failed \n");
        return (false);
	}

	fDrb->retain();

	if ( fFWBusInterface->updateARBwithDevice(fDevice, eui64) == NULL )
	{
		IOLog("IOFireWireIPUnit - updateARBwithDevice failed \n");
        return (false);
	}

	fFWBusInterface->fwIPUnitAttach();

	fTerminateNotifier = IOService::addMatchingNotification(gIOTerminatedNotification, 
													serviceMatching("IOFWIPBusInterface"), 
													&busInterfaceTerminate, this, (void*)fFWBusInterface, 0);

	fStarted = true;

    registerService();
	
    return true;
}

bool IOFireWireIPUnit::busInterfaceTerminate(void *target, void *refCon, IOService *newService, IONotifier * notifier)
{
	if(target == NULL || newService == NULL)
		return false;

	IOFireWireIPUnit	*unit = OSDynamicCast(IOFireWireIPUnit, (IOService *)target);
	
	if ( not unit )
		return false;

	if ( unit->fStarted )
	{
		if ( unit->fFWBusInterface != refCon)
			return false;
		
		unit->terminate();
	}

	return true;
}

bool IOFireWireIPUnit::finalize(IOOptionBits options)
{
	if ( fStarted )
	{
		if ( fTerminateNotifier != NULL )
			fTerminateNotifier->remove();
		
		fTerminateNotifier = NULL;

		if ( fFWBusInterface )
			fFWBusInterface->fwIPUnitTerminate();

		if ( fDrb )
		{
			fFWBusInterface->releaseARB(fDrb->fwaddr);

			fFWBusInterface->releaseDRB(fDrb->fwaddr);

			fDrb->release();
		}	

		fDrb = NULL;

		if ( fFWBusInterface )
		{
			fFWBusInterface->release();

			if ( fFWBusInterface->getUnitCount() == 0 )
				fFWBusInterface->terminate();
		}
		
		fFWBusInterface = NULL;

		fDevice = NULL;
	}
	fStarted = false;

	return IOService::finalize(options);
}

void IOFireWireIPUnit::free(void)
{
    IOService::free();
}

IOReturn IOFireWireIPUnit::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn res = kIOReturnSuccess;

	switch (type)
	{                
		case kIOMessageServiceIsTerminated:
		case kIOMessageServiceIsRequestingClose:
		case kIOMessageServiceIsSuspended:
			break;

		case kIOMessageServiceIsResumed:
			if( fStarted )
			{
				fIPLocalNode->closeIPoFWGate();
	
				updateDrb();

				fIPLocalNode->openIPoFWGate();
			}
			break;

		default: // default the action to return kIOReturnUnsupported
			res = kIOReturnUnsupported;
			break;
	}

    return res;
}

/*!
    @function updateDrb
    @abstract Updates the device reference block in the FireWire IP unit
*/
void IOFireWireIPUnit::updateDrb()
{
	if(fDrb)
	{
		fDrb->maxSpeed	 = fDevice->FWSpeed();
		fDrb->maxPayload = fDevice->maxPackLog(true);
		fFWBusInterface->updateBroadcastValues(true);
	}
}

bool IOFireWireIPUnit::configureFWBusInterface(IOFireWireController *controller)
{
	bool status = false; 
	
	fIPLocalNode = getIPNode(controller);

	if( fIPLocalNode )
	{
		fIPLocalNode->retain();
		
		if ( fIPLocalNode->clientStarting() == true )
		{
			OSDictionary *matchingTable;
			
			matchingTable = serviceMatching("IOFWIPBusInterface");
			
			if ( matchingTable )
			{	
				OSObject *prop = fIPLocalNode->getProperty(gFireWire_GUID);
				if( prop )
					matchingTable->setObject(gFireWire_GUID, prop);
			}

			waitForService( matchingTable );
		}

		fFWBusInterface = getIPTransmitInterface(fIPLocalNode) ;

		status = true;
		if( fFWBusInterface == NULL )
			fFWBusInterface = new IOFWIPBusInterface;
		
		status = fFWBusInterface->init(fIPLocalNode);
		if( status )
		{
			fFWBusInterface->retain();
		}
		else
		{
			fFWBusInterface->release();
			fFWBusInterface = 0;
		}

		fIPLocalNode->release();
	}

	return status;
}

IOFWIPBusInterface *IOFireWireIPUnit::getIPTransmitInterface(IOFireWireIP *fIPLocalNode)
{
	OSIterator	*childIterator = fIPLocalNode->getChildIterator(gIOServicePlane); 
	
	
	IORegistryEntry *child = NULL;
	if(childIterator)
	{
		while((child = (IORegistryEntry*)childIterator->getNextObject())) 
		{
			if(strncmp(child->getName(gIOServicePlane), "IOFWIPBusInterface", strlen("IOFWIPBusInterface")) == 0)
				break; 
		}	
		childIterator->release();
		childIterator = NULL;
	}
	
	IOFWIPBusInterface *fwBusInterface = NULL;
	
	if(child)
		fwBusInterface = OSDynamicCast(IOFWIPBusInterface, child);
	
	return fwBusInterface;
}

IOFireWireIP *IOFireWireIPUnit::getIPNode(IOFireWireController *control) 
{
	OSIterator	*iterator = getMatchingServices( serviceMatching("IOFireWireLocalNode") ); 

	IOFireWireNub *localNode = NULL;
	
	if( iterator ) 
	{
		IOService * obj = NULL; 
		while((obj = (IOService*)iterator->getNextObject())) 
		{ 
			localNode = OSDynamicCast(IOFireWireNub, obj); 
			if(localNode)
			{
				if(localNode->getController() == control)
					break;
				else
					localNode = NULL;
			} 
		} 
		iterator->release(); 
		iterator = NULL; 
	}

	IOFireWireIP *fwIP = NULL;
	
	if( localNode )
	{
		OSDictionary *matchingTable;
		
		matchingTable = serviceMatching("IOFireWireIP");
		
		if ( matchingTable )
		{	
			OSObject *prop = localNode->getProperty(gFireWire_GUID);
			if( prop )
				matchingTable->setObject(gFireWire_GUID, prop);
		}

		fwIP = OSDynamicCast(IOFireWireIP, waitForService( matchingTable ));
	}
	
	return fwIP;
}

