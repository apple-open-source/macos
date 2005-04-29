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
#include "ip_firewire.h"

OSDefineMetaClassAndStructors(IOFireWireIPUnit, IOService)
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 1);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 2);
OSMetaClassDefineReservedUnused(IOFireWireIPUnit, 3);


bool IOFireWireIPUnit::start(IOService *provider)
{
	fIPLocalNode = NULL;
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
        return (false);

	UWIDE	eui64;
	
    CSRNodeUniqueID	fwuid = fDevice->getUniqueID();

	eui64.hi = (UInt32)(fwuid >> 32);
	eui64.lo = (UInt32)(fwuid & 0xffffffff);

	fDrb = fFWBusInterface->initDRBwithDevice(eui64, fDevice, false);

    if ( not fDrb )
        return (false);

	fDrb->retain();

	fFWBusInterface->updateARBwithDevice(fDevice, eui64);
	
	fFWBusInterface->fwIPUnitAttach();
	
    registerService();

    return true;
}

void IOFireWireIPUnit::free(void)
{
    if ( fFWBusInterface )
		fFWBusInterface->release();

	if ( fDrb )
		fDrb->release();
		
	fDrb = NULL;
	fFWBusInterface = NULL;
	
    IOService::free();
}

IOReturn IOFireWireIPUnit::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn res = kIOReturnUnsupported;

	switch (type)
	{                
		case kIOMessageServiceIsTerminated:
			if(fFWBusInterface != NULL)
			{
				fDrb->timer = kDeviceHoldSeconds;
				
				ARB *arb = fFWBusInterface->getARBFromEui64(fDrb->eui64);
				if ( arb )
					arb->timer = kDeviceHoldSeconds;

				fFWBusInterface->fwIPUnitTerminate();
			}
			res = kIOReturnSuccess;
			break;

		case kIOMessageServiceIsSuspended:
			res = kIOReturnSuccess;
			break;

		case kIOMessageServiceIsResumed:
			if(fFWBusInterface != NULL)
				updateDrb();
				
			res = kIOReturnSuccess;
			break;

		default: // default the action to return kIOReturnUnsupported
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
	fIPLocalNode = getIPNode(controller);

	if( not fIPLocalNode )
		return false;

	fIPLocalNode->closeIPGate();

	fFWBusInterface = getIPTransmitInterface(fIPLocalNode) ;
	
	if( fFWBusInterface == NULL )
	{
		fFWBusInterface = new IOFWIPBusInterface;

		if( fFWBusInterface && ( fFWBusInterface->init(fIPLocalNode) == false ) )
		{
			fFWBusInterface->release();
			fFWBusInterface = 0;
			fIPLocalNode->openIPGate();
			return false;
		}
	}
	else
		fFWBusInterface->retain();

	fIPLocalNode->openIPGate();

	return true;
}

IOFWIPBusInterface *IOFireWireIPUnit::getIPTransmitInterface(IOFireWireIP *fIPLocalNode)
{
	OSIterator	*childIterator = fIPLocalNode->getChildIterator(gIOServicePlane); 
	
	
	IORegistryEntry *child = NULL;
	if(childIterator)
	{
		while((child = (IORegistryEntry*)childIterator->getNextObject())) 
		{
			if(strcmp(child->getName(gIOServicePlane), "IOFWIPBusInterface") == 0)
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
	waitForService( serviceMatching( "IOFWInterface" ) );

	OSIterator	*iterator = getMatchingServices( serviceMatching("IOFireWireLocalNode") ); 

	IOFireWireIP *fwIP = NULL;

	if( iterator ) 
	{
		IOService * obj = NULL; 
		while((obj = (IOService*)iterator->getNextObject())) 
		{ 
			OSIterator	*childIterator = obj->getClientIterator();
			
			if(childIterator)
			{
				IORegistryEntry *child = NULL; 
				
				childIterator->reset();
				
				while((child = (IORegistryEntry*)childIterator->getNextObject())) 
				{
					if(strcmp(child->getName(gIOServicePlane), "IOFireWireIP") == 0)
					{
						fwIP = OSDynamicCast(IOFireWireIP, child); 
						if(fwIP)
						{
							if(fwIP->getController() == control)
								break;
							else
								fwIP = NULL;
						} 
					}	
				}	
				
				childIterator->release();
				childIterator = NULL;

				if(fwIP != NULL) // We found an IP node in this localnode, so break here !
					break;
			}
		} 
		iterator->release(); 
		iterator = NULL; 
	}

	return fwIP; 
}

