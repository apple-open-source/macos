/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 8 June 1999 wgulland created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOMessage.h>

#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWIsochChannel.h>
#include <IOKit/firewire/IOFWLocalIsochPort.h>
#include <IOKit/firewire/IOFWIsoch.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOFireWireLink.h>

#include "IOFWUserClientPsdoAddrSpace.h"
#include "IOFWUserClientPhysAddrSpace.h"
#include "IOFWUserCommand.h"
#include "IOFireWireUserClient.h"
#include "IOFWUserIsochPort.h"
#include "IOFWUserIsochChannel.h"

#include <IOKit/IOBufferMemoryDescriptor.h>

#define MIN(a,b) ((a < b ? a : b))
#define DEBUGGING_LEVEL ((0))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFireWireUserClient, IOUserClient) ;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOFireWireUserClient *IOFireWireUserClient::withTask(task_t owningTask)
{
    IOFireWireUserClient*	me				= new IOFireWireUserClient;

    if (me)
		if (me->init())
		{
			me->fTask = owningTask;
			me->mappings = 0 ;
			
		}
		else
		{
			me->release();
			return NULL;
		}

    return me;
}


bool
IOFireWireUserClient::start( IOService * provider )
{
    if (!OSDynamicCast(IOFireWireNub, provider))
		return false ;

    if(!IOUserClient::start(provider))
        return false;
    fOwner = (IOFireWireNub *)provider;

	//
    // Got the owner, so initialize the call structures
	//
	initMethodTable() ;
	initIsochMethodTable() ;
	
	//
	// Set up async method table
	//
	initAsyncMethodTable() ;
	initIsochAsyncMethodTable() ;
	
	// initialize notification structures
	fBusResetAsyncNotificationRef[0] = 0 ;
	fBusResetDoneAsyncNotificationRef[0] = 0 ;

	bool result = true ;

	fSetLock = IOLockAlloc() ;
	if (!fSetLock)
		result = false ;

	if (result)
	{
		IOLockLock(fSetLock) ;
		
		result = (NULL != (fUserPseudoAddrSpaces = OSSet::withCapacity(0)) ) ;

		if (result)
			result = (NULL != (fUserPhysicalAddrSpaces = OSSet::withCapacity(0)) ) ;

		if (result)
			result = (NULL != (fUserIsochChannels = OSSet::withCapacity(0)) ) ;
		
		if (result)
			result = (NULL != (fUserIsochPorts = OSSet::withCapacity(0)) ) ;

		if (result)
			result = (NULL != (fUserCommandObjects = OSSet::withCapacity(0)) ) ;

		if (result)
			result = (NULL != (fUserUnitDirectories = OSSet::withCapacity(0)) ) ;

		#ifdef IOFIREWIREUSERCLIENTDEBUG
		if ( result )
			result = (NULL != (fStatistics = new IOFireWireUserClientStatistics ) ) ;
		
		if ( result )
			result = ( NULL != ( fStatistics->dict = OSDictionary::withCapacity(4)) ) ;
		#endif
		
/*		if (!result)
		{
			if (fUserPseudoAddrSpaces)
				fUserPseudoAddrSpaces->release() ;
			if (fUserPhysicalAddrSpaces)
				fUserPhysicalAddrSpaces->release() ;
			if (fUserIsochChannels)
				fUserIsochChannels->release() ;
			if (fUserIsochPorts)
				fUserIsochPorts->release() ;
			if (fUserCommandObjects)
				fUserCommandObjects->release() ;
			if (fUserUnitDirectories)
				fUserUnitDirectories->release() ;

			#ifdef IOFIREWIREUSERCLIENTDEBUG
			if ( fStatistics->dict )
				fStatistics->dict->release() ;
			if ( fStatistics )
				delete fStatistics ;
			#endif
		} */

		IOLockUnlock(fSetLock) ;
	}
	
	#ifdef IOFIREWIREUSERCLIENTDEBUG
	if ( result )
	{
		result = ( NULL != ( fStatistics->isochCallbacks = OSNumber::withNumber( (long long unsigned int) 0, 64 ) ) ) ;
		
		if ( result )
			result = ( NULL != (fStatistics->pseudoAddressSpaces = OSSet::withCapacity( 10 ) ) ) ;
		
		if ( result && setProperty( "Statistics", fStatistics->dict ) )
		{
			fStatistics->dict->setObject( "isoch-callbacks", fStatistics->isochCallbacks ) ;
			fStatistics->dict->setObject( "pseudo-addr-spaces", fStatistics->pseudoAddressSpaces ) ;
		}
	}
	#endif
	
    return result ;
}

void
IOFireWireUserClient::deallocateSets()
{
	IOLockLock(fSetLock) ;

	#if IOFIREWIREUSERCLIENTDEBUG > 0
	if ( fStatistics )
	{
		if ( fStatistics->dict )
			fStatistics->dict->release() ;
		
		if ( fStatistics->isochCallbacks )
			fStatistics->isochCallbacks->release() ;
		
		if ( fStatistics->pseudoAddressSpaces )
			fStatistics->pseudoAddressSpaces->release() ;		
	}
	
	delete fStatistics ;

	#endif
	
	OSCollectionIterator* 	iterator ;
	IOReturn				err		= kIOReturnSuccess ;

	if ( fUserPseudoAddrSpaces )
	{
		iterator	= OSCollectionIterator::withCollection( fUserPseudoAddrSpaces )  ;
		
		if ( !iterator )
			IOLog("%s %u: Couldn't get iterator to clean up pseudo address spaces\n", __FILE__, __LINE__) ;
		else
		{
			IOFWUserClientPseudoAddrSpace*	addrSpace ;
			while ( NULL != (addrSpace = OSDynamicCast( IOFWUserClientPseudoAddrSpace, iterator->getNextObject() ) ) )
				addrSpace->deactivate() ;
			
			iterator->release() ;
		}
		
		fUserPseudoAddrSpaces->release() ;
	}
	
	if (fUserPhysicalAddrSpaces)
	{
		iterator	= OSCollectionIterator::withCollection( fUserPhysicalAddrSpaces )  ;
		
		if ( !iterator )
			IOLog("%s %u: Couldn't get iterator to clean up physical address spaces\n", __FILE__, __LINE__) ;
		else
		{
			IOFWUserClientPhysicalAddressSpace*	physAddrSpace ;
			while ( NULL != (physAddrSpace = OSDynamicCast( IOFWUserClientPhysicalAddressSpace, iterator->getNextObject() ) ) )
				physAddrSpace->deactivate() ;
			
			iterator->release() ;
		}
		
		fUserPhysicalAddrSpaces->release() ;
	}
	
	if (fUserIsochChannels)
	{
		iterator	= OSCollectionIterator::withCollection(fUserIsochChannels) ;
		err			= kIOReturnSuccess ;

		if (!iterator)
			IOLog("IOFireWireUserClient::clientClose: Couldn't get iterator to stop and release channels!\n") ;
		else
		{
			IOFWUserIsochChannel*	channel ;
			while (NULL != (channel = OSDynamicCast(IOFWUserIsochChannel, iterator->getNextObject()) ))
			{
				err = channel->userReleaseChannelComplete() ;
				if ( err )
					IOLog("IOFireWireUserClient::clientClose: channel->userReleaseChannelComplete failed with error 0x%08lX\n", (UInt32) err) ;
			}

			iterator->release() ;
		}

		fUserIsochChannels->flushCollection() ;
		fUserIsochChannels->release() ;
	}
	
	if (fUserIsochPorts)
	{
		iterator	= OSCollectionIterator::withCollection(fUserIsochPorts) ;
		err			= kIOReturnSuccess ;

		if (!iterator)
			IOLog("IOFireWireUserClient::clientClose: Couldn't get iterator to stop and release ports!\n") ;
		else
		{
			IOFWUserIsochPortProxy*		portProxy ;
			while (NULL != (portProxy = OSDynamicCast(IOFWUserIsochPortProxy, iterator->getNextObject()) ))
			{
				err = portProxy->stop() ;
				if ( err )
					IOLog("IOFireWireUserClient::clientClose: port->stop() failed with error 0x%08lX\n", (UInt32) err) ;
				else
				{
					err = portProxy->releasePort() ;
					if ( err )
						IOLog("IOFireWireUserClient::clientClose: port->releaseChannel() failed with error 0x%08lX\n", (UInt32) err) ;
				}
			}
			
			iterator->release() ;
		}

		fUserIsochPorts->flushCollection() ;
		fUserIsochPorts->release() ;
	}
	
	if (fUserCommandObjects)
	{
		fUserCommandObjects->flushCollection() ;
		fUserCommandObjects->release() ;	// should we find a way to cancel these or wait for them to complete?
	}
	
	if (fUserUnitDirectories)
	{
		iterator	= OSCollectionIterator::withCollection(fUserUnitDirectories) ;
		err		= kIOReturnSuccess ;

		if (!iterator)
		{
			IOFireWireUserClientLogIfNil_(iterator, ("IOFireWireUserClient::clientClose: Couldn't get iterator to stop and release ports!\n")) ;
		}
		else
		{
			IOLocalConfigDirectory*		configDir ;
			while (NULL != (configDir = OSDynamicCast(IOLocalConfigDirectory, iterator->getNextObject()) ))
			{
				err = fOwner->getController()->RemoveUnitDirectory(configDir) ;
				if ( err )
					IOLog("IOFireWireUserClient::clientClose: port->stop() failed with error 0x%08lX\n", (UInt32) err) ;
			}
			
			iterator->release() ;
		}

		fUserUnitDirectories->flushCollection() ;
		fUserUnitDirectories->release() ;
	}
	
	IOLockUnlock(fSetLock) ;
}

void
IOFireWireUserClient::free()
{
	IOLockFree(fSetLock) ;	
	
	IOUserClient::free() ;
}

IOReturn
IOFireWireUserClient::clientClose()
{
	deallocateSets() ;

	IOReturn	result = userClose() ;

	if ( result == kIOReturnSuccess )
		IOLog("IOFireWireUserClient::clientClose(): client left user client open, should call close. Closing...\n") ;
	else if ( result == kIOReturnNotOpen )
		result = kIOReturnSuccess ;

	if ( !terminate() )
		IOLog("IOFireWireUserClient::clientClose: terminate failed!, fOwner->isOpen(this) returned %u\n", fOwner->isOpen(this)) ;

    return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient::clientDied( void )
{
	IOFireWireUserClientLog_(("+IOFireWireUserClient::clientDied: retain=%u\n", getRetainCount() )) ;

    return( clientClose() );
}

#pragma mark -
#pragma mark --- utils ----------

IOReturn
IOFireWireUserClient::addObjectToSet(OSObject* object, OSSet* set)
{
	IOReturn result = kIOReturnSuccess ;
	
	IOLockLock(fSetLock) ;
	
	UInt32 needCapacity = 1 + set->getCount() ;
	if (set->ensureCapacity(needCapacity) >= needCapacity)
	{
		set->setObject(object) ;
		object->release() ; // the OSSet has it...
	}
	else
		result = kIOReturnNoMemory ;
		
	IOLockUnlock(fSetLock) ;

	return result ;
}

void
IOFireWireUserClient::removeObjectFromSet(OSObject* object, OSSet* set)
{
	IOLockLock(fSetLock) ;
	
	set->removeObject(object) ;

	IOLockUnlock(fSetLock) ;
}

void 
IOFireWireUserClient::initMethodTable()
{
	fMethods[kFireWireOpen].object = this ;
	fMethods[kFireWireOpen].func = (IOMethod) & IOFireWireUserClient::userOpen ;
	fMethods[kFireWireOpen].count0 = 0 ;
	fMethods[kFireWireOpen].count1 = 0 ;
	fMethods[kFireWireOpen].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFireWireOpenWithSessionRef].object = this ;
	fMethods[kFireWireOpenWithSessionRef].func = (IOMethod) & IOFireWireUserClient::userOpenWithSessionRef ;
	fMethods[kFireWireOpenWithSessionRef].count0 = 1 ;
	fMethods[kFireWireOpenWithSessionRef].count1 = 0 ;
	fMethods[kFireWireOpenWithSessionRef].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFireWireClose].object = this ;
	fMethods[kFireWireClose].func = (IOMethod) & IOFireWireUserClient::userClose ;
	fMethods[kFireWireClose].count0 = 0 ;
	fMethods[kFireWireClose].count1 = 0 ;
	fMethods[kFireWireClose].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFireWireReadQuad].object = this;
	fMethods[kFireWireReadQuad].func =
		(IOMethod)&IOFireWireUserClient::readQuad;
	fMethods[kFireWireReadQuad].count0 = 4;
	fMethods[kFireWireReadQuad].count1 = 1;
	fMethods[kFireWireReadQuad].flags = kIOUCScalarIScalarO;
	
	fMethods[kFireWireReadQuadAbsolute].object = this;
	fMethods[kFireWireReadQuadAbsolute].func =
		(IOMethod)&IOFireWireUserClient::readQuadAbsolute;
	fMethods[kFireWireReadQuadAbsolute].count0 = 4;
	fMethods[kFireWireReadQuadAbsolute].count1 = 1;
	fMethods[kFireWireReadQuadAbsolute].flags = kIOUCScalarIScalarO;
	
	fMethods[kFireWireRead].object = this;
	fMethods[kFireWireRead].func =
		(IOMethod)&IOFireWireUserClient::read;
	fMethods[kFireWireRead].count0 = sizeof(FWReadWriteParams);
	fMethods[kFireWireRead].count1 = sizeof(UInt32) ;
	fMethods[kFireWireRead].flags = kIOUCStructIStructO;
	
	fMethods[kFireWireReadAbsolute].object = this;
	fMethods[kFireWireReadAbsolute].func =
		(IOMethod)&IOFireWireUserClient::readAbsolute;
	fMethods[kFireWireReadAbsolute].count0 = sizeof(FWReadWriteParams);
	fMethods[kFireWireReadAbsolute].count1 = sizeof(UInt32) ;
	fMethods[kFireWireReadAbsolute].flags = kIOUCStructIStructO;
	
	fMethods[kFireWireWriteQuad].object = this;
	fMethods[kFireWireWriteQuad].func =
		(IOMethod)&IOFireWireUserClient::writeQuad;
	fMethods[kFireWireWriteQuad].count0 = 5;
	fMethods[kFireWireWriteQuad].count1 = 0;
	fMethods[kFireWireWriteQuad].flags = kIOUCScalarIScalarO;
	
	fMethods[kFireWireWriteQuadAbsolute].object = this;
	fMethods[kFireWireWriteQuadAbsolute].func =
		(IOMethod)&IOFireWireUserClient::writeQuadAbsolute;
	fMethods[kFireWireWriteQuadAbsolute].count0 = 5;
	fMethods[kFireWireWriteQuadAbsolute].count1 = 0;
	fMethods[kFireWireWriteQuadAbsolute].flags = kIOUCScalarIScalarO;
	
	fMethods[kFireWireWrite].object = this;
	fMethods[kFireWireWrite].func =
		(IOMethod)&IOFireWireUserClient::write;
	fMethods[kFireWireWrite].count0 = sizeof(FWReadWriteParams) ;
	fMethods[kFireWireWrite].count1 = sizeof(IOByteCount) ;
	fMethods[kFireWireWrite].flags = kIOUCStructIStructO;
	
	fMethods[kFireWireWriteAbsolute].object = this;
	fMethods[kFireWireWriteAbsolute].func =
		(IOMethod)&IOFireWireUserClient::writeAbsolute;
	fMethods[kFireWireWriteAbsolute].count0 = sizeof(FWReadWriteParams) ;
	fMethods[kFireWireWriteAbsolute].count1 = sizeof(IOByteCount) ;
	fMethods[kFireWireWriteAbsolute].flags = kIOUCStructIStructO;
	
	fMethods[kFireWireCompareSwap].object = this;
	fMethods[kFireWireCompareSwap].func =
		(IOMethod)&IOFireWireUserClient::compareSwap;
	fMethods[kFireWireCompareSwap].count0 = 6;
	fMethods[kFireWireCompareSwap].count1 = 0;
	fMethods[kFireWireCompareSwap].flags = kIOUCScalarIScalarO;
	
	fMethods[kFireWireCompareSwapAbsolute].object = this;
	fMethods[kFireWireCompareSwapAbsolute].func =
		(IOMethod)&IOFireWireUserClient::compareSwapAbsolute;
	fMethods[kFireWireCompareSwapAbsolute].count0 = 6;
	fMethods[kFireWireCompareSwapAbsolute].count1 = 0;
	fMethods[kFireWireCompareSwapAbsolute].flags = kIOUCScalarIScalarO;
	
	fMethods[kFireWireBusReset].object = this;
	fMethods[kFireWireBusReset].func =
		(IOMethod)&IOFireWireUserClient::busReset;
	fMethods[kFireWireBusReset].count0 = 0;
	fMethods[kFireWireBusReset].count1 = 0;
	fMethods[kFireWireBusReset].flags = kIOUCScalarIScalarO;
	
	fMethods[kFireWireCycleTime].object = fOwner->getController()->getLink() ;
	fMethods[kFireWireCycleTime].func =
		(IOMethod)&IOFireWireLink::getCycleTime;
	fMethods[kFireWireCycleTime].count0 = 0;
	fMethods[kFireWireCycleTime].count1 = 1;
	fMethods[kFireWireCycleTime].flags = kIOUCScalarIScalarO;
	
	fMethods[kFireWireGetGenerationAndNodeID].object = this ;
	fMethods[kFireWireGetGenerationAndNodeID].func =
		(IOMethod) & IOFireWireUserClient::getGenerationAndNodeID ;
	fMethods[kFireWireGetGenerationAndNodeID].count0 = 0 ;
	fMethods[kFireWireGetGenerationAndNodeID].count1 = 2 ;
	fMethods[kFireWireGetGenerationAndNodeID].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFireWireGetLocalNodeID].object = this ;
	fMethods[kFireWireGetLocalNodeID].func =
		(IOMethod) & IOFireWireUserClient::getLocalNodeID ;
	fMethods[kFireWireGetLocalNodeID].count0 = 0 ;
	fMethods[kFireWireGetLocalNodeID].count1 = 1 ;
	fMethods[kFireWireGetLocalNodeID].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFireWireGetResetTime].object = this ;
	fMethods[kFireWireGetResetTime].func =
		(IOMethod) & IOFireWireUserClient::getResetTime ;
	fMethods[kFireWireGetResetTime].count0 = 0 ;
	fMethods[kFireWireGetResetTime].count1 = 2 ;	// this is 2 because we're returning an AbsoluteTime
	fMethods[kFireWireGetResetTime].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWGetOSStringData].object = this ;
	fMethods[kFWGetOSStringData].func = (IOMethod) & IOFireWireUserClient::getOSStringData ;
	fMethods[kFWGetOSStringData].count0 = 3 ;
	fMethods[kFWGetOSStringData].count1 = 1 ;
	fMethods[kFWGetOSStringData].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWGetOSDataData].object = this ;
	fMethods[kFWGetOSDataData].func = (IOMethod) & IOFireWireUserClient::getOSDataData ;
	fMethods[kFWGetOSDataData].count0 = 3 ;
	fMethods[kFWGetOSDataData].count1 = 1 ;
	fMethods[kFWGetOSDataData].flags = kIOUCScalarIScalarO ;
	
	// -- Config ROM Methods ----------
	
	fMethods[kFWUnitDirCreate].object			= this;
	fMethods[kFWUnitDirCreate].func 			= (IOMethod)&IOFireWireUserClient::unitDirCreate ;
	fMethods[kFWUnitDirCreate].count0 			= 0;
	fMethods[kFWUnitDirCreate].count1 			= 1;
	fMethods[kFWUnitDirCreate].flags 			= kIOUCScalarIScalarO ;
	
	fMethods[kFWUnitDirRelease].object			= this ;
	fMethods[kFWUnitDirRelease].func			= (IOMethod) & IOFireWireUserClient::unitDirRelease ;
	fMethods[kFWUnitDirRelease].count0			= 1 ;
	fMethods[kFWUnitDirRelease].count1			= 0 ;
	fMethods[kFWUnitDirRelease].flags			= kIOUCScalarIScalarO ;
	
	fMethods[kFWUnitDirAddEntry_Buffer].object	= this ;
	fMethods[kFWUnitDirAddEntry_Buffer].func	= (IOMethod)&IOFireWireUserClient::addEntry_Buffer ;
	fMethods[kFWUnitDirAddEntry_Buffer].count0	= 2 ;
	fMethods[kFWUnitDirAddEntry_Buffer].count1	= 0xFFFFFFFF ;	// variable
	fMethods[kFWUnitDirAddEntry_Buffer].flags	= kIOUCScalarIStructI ;
	
	fMethods[kFWUnitDirAddEntry_UInt32].object	= this ;
	fMethods[kFWUnitDirAddEntry_UInt32].func	= (IOMethod)&IOFireWireUserClient::addEntry_UInt32 ;
	fMethods[kFWUnitDirAddEntry_UInt32].count0	= 3 ;
	fMethods[kFWUnitDirAddEntry_UInt32].count1	= 0 ;
	fMethods[kFWUnitDirAddEntry_UInt32].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kFWUnitDirAddEntry_FWAddr].object	= this ;
	fMethods[kFWUnitDirAddEntry_FWAddr].func	= (IOMethod)&IOFireWireUserClient::addEntry_FWAddr ;
	fMethods[kFWUnitDirAddEntry_FWAddr].count0	= 2 ;
	fMethods[kFWUnitDirAddEntry_FWAddr].count1	= sizeof(FWAddress) ;	// sizeof(FWAddress)
	fMethods[kFWUnitDirAddEntry_FWAddr].flags	= kIOUCScalarIStructI ;
	
	fMethods[kFWUnitDirAddEntry_UnitDir].object	= this ;
	fMethods[kFWUnitDirAddEntry_UnitDir].func	= (IOMethod)&IOFireWireUserClient::addEntry_UnitDir ;
	fMethods[kFWUnitDirAddEntry_UnitDir].count0	= 3 ;
	fMethods[kFWUnitDirAddEntry_UnitDir].count1	= 0 ;
	fMethods[kFWUnitDirAddEntry_UnitDir].flags	= kIOUCScalarIStructI ;
	
	fMethods[kFWUnitDirPublish].object			= this ;
	fMethods[kFWUnitDirPublish].func			= (IOMethod) & IOFireWireUserClient::publish ;
	fMethods[kFWUnitDirPublish].count0			= 1 ;
	fMethods[kFWUnitDirPublish].count1			= 0 ;
	fMethods[kFWUnitDirPublish].flags			= kIOUCScalarIScalarO ;
	
	fMethods[kFWUnitDirUnpublish].object		= this ;
	fMethods[kFWUnitDirUnpublish].func			= (IOMethod) & IOFireWireUserClient::unpublish ;
	fMethods[kFWUnitDirUnpublish].count0		= 1 ;
	fMethods[kFWUnitDirUnpublish].count1		= 0 ;
	fMethods[kFWUnitDirUnpublish].flags			= kIOUCScalarIScalarO ;
	
	// --- Pseudo Address Space Methods ---------
	
	fMethods[kFWPseudoAddrSpace_Allocate].object	= this ;
	fMethods[kFWPseudoAddrSpace_Allocate].func		= (IOMethod) & IOFireWireUserClient::allocateAddressSpace ;
	fMethods[kFWPseudoAddrSpace_Allocate].count0	= sizeof(FWAddrSpaceCreateParams) ;
	fMethods[kFWPseudoAddrSpace_Allocate].count1	= sizeof(FWKernAddrSpaceRef) ;
	fMethods[kFWPseudoAddrSpace_Allocate].flags		= kIOUCStructIStructO ;
	
	fMethods[kFWPseudoAddrSpace_Release].object		= this ;
	fMethods[kFWPseudoAddrSpace_Release].func		= (IOMethod) & IOFireWireUserClient::releaseAddressSpace ;
	fMethods[kFWPseudoAddrSpace_Release].count0		= 1 ;
	fMethods[kFWPseudoAddrSpace_Release].count1		= 0 ;
	fMethods[kFWPseudoAddrSpace_Release].flags		= kIOUCScalarIScalarO ;
	
	fMethods[kFWPseudoAddrSpace_GetFWAddrInfo].object	= this ;
	fMethods[kFWPseudoAddrSpace_GetFWAddrInfo].func 	= (IOMethod) & IOFireWireUserClient::getPseudoAddressSpaceInfo ;
	fMethods[kFWPseudoAddrSpace_GetFWAddrInfo].count0	= 1 ;
	fMethods[kFWPseudoAddrSpace_GetFWAddrInfo].count1	= 3 ;
	fMethods[kFWPseudoAddrSpace_GetFWAddrInfo].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kFWPseudoAddrSpace_ClientCommandIsComplete].object	= this ;
	fMethods[kFWPseudoAddrSpace_ClientCommandIsComplete].func	= (IOMethod) & IOFireWireUserClient::clientCommandIsComplete ;
	fMethods[kFWPseudoAddrSpace_ClientCommandIsComplete].count0	= 3 ;
	fMethods[kFWPseudoAddrSpace_ClientCommandIsComplete].count1	= 0 ;
	fMethods[kFWPseudoAddrSpace_ClientCommandIsComplete].flags	= kIOUCScalarIScalarO ;
	
	// --- Physical Address Space Methods ---------
	
	fMethods[kFWPhysicalAddrSpace_Allocate].object 				= this ;
	fMethods[kFWPhysicalAddrSpace_Allocate].func				= (IOMethod) & IOFireWireUserClient::allocatePhysicalAddressSpace ;
	fMethods[kFWPhysicalAddrSpace_Allocate].count0				= sizeof(FWPhysicalAddrSpaceCreateParams) ;
	fMethods[kFWPhysicalAddrSpace_Allocate].count1				= sizeof(FWKernPhysicalAddrSpaceRef) ;
	fMethods[kFWPhysicalAddrSpace_Allocate].flags				= kIOUCStructIStructO ;
	
	fMethods[kFWPhysicalAddrSpace_Release].object 				= this ;
	fMethods[kFWPhysicalAddrSpace_Release].func					= (IOMethod) & IOFireWireUserClient::releasePhysicalAddressSpace ;
	fMethods[kFWPhysicalAddrSpace_Release].count0				= 1 ;
	fMethods[kFWPhysicalAddrSpace_Release].count1				= 0 ;
	fMethods[kFWPhysicalAddrSpace_Release].flags				= kIOUCScalarIScalarO ;
	
	//	fMethods[kFWPhysicalAddrSpace_GetFWAddrInfo].object 		= this ;
	//	fMethods[kFWPhysicalAddrSpace_GetFWAddrInfo].func			= (IOMethod) & IOFireWireUserClient::getPhysicalAddressSpaceInfo ;
	//	fMethods[kFWPhysicalAddrSpace_GetFWAddrInfo].count0			=
	//	fMethods[kFWPhysicalAddrSpace_GetFWAddrInfo].count1			=
	//	fMethods[kFWPhysicalAddrSpace_GetFWAddrInfo].flags			=
	
	fMethods[kFWPhysicalAddrSpace_GetSegmentCount].object		= this ;
	fMethods[kFWPhysicalAddrSpace_GetSegmentCount].func			= (IOMethod) & IOFireWireUserClient::getPhysicalAddressSpaceSegmentCount ;
	fMethods[kFWPhysicalAddrSpace_GetSegmentCount].count0		= 1 ;
	fMethods[kFWPhysicalAddrSpace_GetSegmentCount].count1		= 1 ;
	fMethods[kFWPhysicalAddrSpace_GetSegmentCount].flags		= kIOUCScalarIScalarO ;
	
	fMethods[kFWPhysicalAddrSpace_GetSegments].object			= this ;
	fMethods[kFWPhysicalAddrSpace_GetSegments].func				= (IOMethod) & IOFireWireUserClient::getPhysicalAddressSpaceSegments ;
	fMethods[kFWPhysicalAddrSpace_GetSegments].count0			= 4 ;	
	fMethods[kFWPhysicalAddrSpace_GetSegments].count1			= 1 ;
	fMethods[kFWPhysicalAddrSpace_GetSegments].flags			= kIOUCScalarIScalarO ;
	
	fMethods[kFWClientCommandIsComplete].object	= this ;
	fMethods[kFWClientCommandIsComplete].func	= (IOMethod) & IOFireWireUserClient::clientCommandIsComplete ;
	fMethods[kFWClientCommandIsComplete].count0	= 2 ;
	fMethods[kFWClientCommandIsComplete].count1	= 0 ;
	fMethods[kFWClientCommandIsComplete].flags	= kIOUCScalarIScalarO ;
	
	// --- config directory ----------------------
	fMethods[kFWConfigDirectoryCreate].object = this ;
	fMethods[kFWConfigDirectoryCreate].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryCreate ;
	fMethods[kFWConfigDirectoryCreate].count0 = 0 ;
	fMethods[kFWConfigDirectoryCreate].count1 = 1 ;
	fMethods[kFWConfigDirectoryCreate].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryRelease].object	= this ;
	fMethods[kFWConfigDirectoryRelease].func	= (IOMethod) & IOFireWireUserClient::configDirectoryRelease ;
	fMethods[kFWConfigDirectoryRelease].count0	= 1 ;
	fMethods[kFWConfigDirectoryRelease].count1	= 0 ;
	fMethods[kFWConfigDirectoryRelease].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryUpdate].object 	= this ;
	fMethods[kFWConfigDirectoryUpdate].func 	= 
			(IOMethod) & IOFireWireUserClient::configDirectoryUpdate ;
	fMethods[kFWConfigDirectoryUpdate].count0 	= 1 ;
	fMethods[kFWConfigDirectoryUpdate].count1 	= 0 ;
	fMethods[kFWConfigDirectoryUpdate].flags 	= kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetKeyType].object = this ;
	fMethods[kFWConfigDirectoryGetKeyType].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyType ;
	fMethods[kFWConfigDirectoryGetKeyType].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetKeyType].count1 = 1 ;
	fMethods[kFWConfigDirectoryGetKeyType].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetKeyValue_UInt32].object = this ;
	fMethods[kFWConfigDirectoryGetKeyValue_UInt32].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyValue_UInt32 ;
	fMethods[kFWConfigDirectoryGetKeyValue_UInt32].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetKeyValue_UInt32].count1 = 3 ;
	fMethods[kFWConfigDirectoryGetKeyValue_UInt32].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetKeyValue_Data].object = this ;
	fMethods[kFWConfigDirectoryGetKeyValue_Data].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyValue_Data ;
	fMethods[kFWConfigDirectoryGetKeyValue_Data].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetKeyValue_Data].count1 = 4 ;
	fMethods[kFWConfigDirectoryGetKeyValue_Data].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetKeyValue_ConfigDirectory].object = this ;
	fMethods[kFWConfigDirectoryGetKeyValue_ConfigDirectory].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyValue_ConfigDirectory ;
	fMethods[kFWConfigDirectoryGetKeyValue_ConfigDirectory].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetKeyValue_ConfigDirectory].count1 = 3 ;
	fMethods[kFWConfigDirectoryGetKeyValue_ConfigDirectory].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetKeyOffset_FWAddress].object = this ;
	fMethods[kFWConfigDirectoryGetKeyOffset_FWAddress].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyOffset_FWAddress ;
	fMethods[kFWConfigDirectoryGetKeyOffset_FWAddress].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetKeyOffset_FWAddress].count1 = 4 ;
	fMethods[kFWConfigDirectoryGetKeyOffset_FWAddress].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexType].object = this ;
	fMethods[kFWConfigDirectoryGetIndexType].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexType ;
	fMethods[kFWConfigDirectoryGetIndexType].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexType].count1 = 1 ;
	fMethods[kFWConfigDirectoryGetIndexType].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexKey].object = this ;
	fMethods[kFWConfigDirectoryGetIndexKey].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexKey ;
	fMethods[kFWConfigDirectoryGetIndexKey].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexKey].count1 = 1 ;
	fMethods[kFWConfigDirectoryGetIndexKey].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexValue_UInt32].object = this ;
	fMethods[kFWConfigDirectoryGetIndexValue_UInt32].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexValue_UInt32 ;
	fMethods[kFWConfigDirectoryGetIndexValue_UInt32].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexValue_UInt32].count1 = 1 ;
	fMethods[kFWConfigDirectoryGetIndexValue_UInt32].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexValue_Data].object = this ;
	fMethods[kFWConfigDirectoryGetIndexValue_Data].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexValue_Data ;
	fMethods[kFWConfigDirectoryGetIndexValue_Data].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexValue_Data].count1 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexValue_Data].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexValue_String].object = this ;
	fMethods[kFWConfigDirectoryGetIndexValue_String].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexValue_String ;
	fMethods[kFWConfigDirectoryGetIndexValue_String].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexValue_String].count1 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexValue_String].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexValue_ConfigDirectory].object = this ;
	fMethods[kFWConfigDirectoryGetIndexValue_ConfigDirectory].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexValue_ConfigDirectory ;
	fMethods[kFWConfigDirectoryGetIndexValue_ConfigDirectory].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexValue_ConfigDirectory].count1 = 1 ;
	fMethods[kFWConfigDirectoryGetIndexValue_ConfigDirectory].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexOffset_FWAddress].object = this ;
	fMethods[kFWConfigDirectoryGetIndexOffset_FWAddress].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexOffset_FWAddress ;
	fMethods[kFWConfigDirectoryGetIndexOffset_FWAddress].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexOffset_FWAddress].count1 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexOffset_FWAddress].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexOffset_UInt32].object = this ;
	fMethods[kFWConfigDirectoryGetIndexOffset_UInt32].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexOffset_UInt32 ;
	fMethods[kFWConfigDirectoryGetIndexOffset_UInt32].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexOffset_UInt32].count1 = 1 ;
	fMethods[kFWConfigDirectoryGetIndexOffset_UInt32].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetIndexEntry].object = this ;
	fMethods[kFWConfigDirectoryGetIndexEntry].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexEntry ;
	fMethods[kFWConfigDirectoryGetIndexEntry].count0 = 2 ;
	fMethods[kFWConfigDirectoryGetIndexEntry].count1 = 1 ;
	fMethods[kFWConfigDirectoryGetIndexEntry].flags = kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetSubdirectories].object	= this ;
	fMethods[kFWConfigDirectoryGetSubdirectories].func 		= (IOMethod) & IOFireWireUserClient::configDirectoryGetSubdirectories ;
	fMethods[kFWConfigDirectoryGetSubdirectories].count0	= 1 ;
	fMethods[kFWConfigDirectoryGetSubdirectories].count1	= 1 ;
	fMethods[kFWConfigDirectoryGetSubdirectories].flags		= kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetKeySubdirectories].object	= this ;
	fMethods[kFWConfigDirectoryGetKeySubdirectories].func 	= (IOMethod) & IOFireWireUserClient::configDirectoryGetKeySubdirectories ;
	fMethods[kFWConfigDirectoryGetKeySubdirectories].count0	= 2 ;
	fMethods[kFWConfigDirectoryGetKeySubdirectories].count1	= 1 ;
	fMethods[kFWConfigDirectoryGetKeySubdirectories].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kFWConfigDirectoryGetType].object				= this ;
	fMethods[kFWConfigDirectoryGetType].func 				= (IOMethod) & IOFireWireUserClient::configDirectoryGetType ;
	fMethods[kFWConfigDirectoryGetType].count0				= 1 ;
	fMethods[kFWConfigDirectoryGetType].count1				= 1 ;
	fMethods[kFWConfigDirectoryGetType].flags 				= kIOUCScalarIScalarO ;

	fMethods[kFWConfigDirectoryGetNumEntries].object		= this ;
	fMethods[kFWConfigDirectoryGetNumEntries].func 			= (IOMethod) & IOFireWireUserClient::configDirectoryGetNumEntries ;
	fMethods[kFWConfigDirectoryGetNumEntries].count0		= 1 ;
	fMethods[kFWConfigDirectoryGetNumEntries].count1		= 1 ;
	fMethods[kFWConfigDirectoryGetNumEntries].flags 		= kIOUCScalarIScalarO ;

	fMethods[kFWCommand_Allocate].object					= NULL ; // (!)
	fMethods[kFWCommand_Allocate].func						= NULL;//(IOMethod) & IOFireWireUserClient::fwCommandAllocate ;
	fMethods[kFWCommand_Allocate].count0					= 0 ;
	fMethods[kFWCommand_Allocate].count1					= 0 ;
	fMethods[kFWCommand_Allocate].flags						= kIOUCScalarIScalarO ;

	fMethods[kFWCommand_Release].object						= this ;
	fMethods[kFWCommand_Release].func						= (IOMethod) & IOFireWireUserClient::userAsyncCommand_Release ;
	fMethods[kFWCommand_Release].count0						= 1 ;
	fMethods[kFWCommand_Release].count1						= 0 ;
	fMethods[kFWCommand_Release].flags						= kIOUCScalarIScalarO ;
	
	fMethods[kFWCommand_Cancel].object						= this ;
	fMethods[kFWCommand_Cancel].func						= (IOMethod) & IOFireWireUserClient::userAsyncCommand_Cancel ;
	fMethods[kFWCommand_Cancel].count0						= 2 ;
	fMethods[kFWCommand_Cancel].count1						= 0 ;
	fMethods[kFWCommand_Cancel].flags						= kIOUCScalarIScalarO ;
}

void
IOFireWireUserClient::initAsyncMethodTable()
{
	fAsyncMethods[kFWSetAsyncRef_BusReset].object	= this ;
	fAsyncMethods[kFWSetAsyncRef_BusReset].func	= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_BusReset ;
	fAsyncMethods[kFWSetAsyncRef_BusReset].count0	= 2 ;
	fAsyncMethods[kFWSetAsyncRef_BusReset].count1	= 0 ;
	fAsyncMethods[kFWSetAsyncRef_BusReset].flags	= kIOUCScalarIScalarO ;

	fAsyncMethods[kFWSetAsyncRef_BusResetDone].object	= this ;
	fAsyncMethods[kFWSetAsyncRef_BusResetDone].func	= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_BusResetDone ;
	fAsyncMethods[kFWSetAsyncRef_BusResetDone].count0	= 2 ;
	fAsyncMethods[kFWSetAsyncRef_BusResetDone].count1	= 0 ;
	fAsyncMethods[kFWSetAsyncRef_BusResetDone].flags	= kIOUCScalarIScalarO ;

	fAsyncMethods[kFWSetAsyncRef_Packet].object				= this ;
	fAsyncMethods[kFWSetAsyncRef_Packet].func				= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_Packet ;
	fAsyncMethods[kFWSetAsyncRef_Packet].count0				= 3 ;
	fAsyncMethods[kFWSetAsyncRef_Packet].count1				= 0 ;
	fAsyncMethods[kFWSetAsyncRef_Packet].flags				= kIOUCScalarIScalarO ;

	fAsyncMethods[kFWSetAsyncRef_SkippedPacket].object		= this ;
	fAsyncMethods[kFWSetAsyncRef_SkippedPacket].func		= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_SkippedPacket ;
	fAsyncMethods[kFWSetAsyncRef_SkippedPacket].count0		= 3 ;
	fAsyncMethods[kFWSetAsyncRef_SkippedPacket].count1		= 0 ;
	fAsyncMethods[kFWSetAsyncRef_SkippedPacket].flags		= kIOUCScalarIScalarO ;
		
	fAsyncMethods[kFWSetAsyncRef_Read].object				= this ;
	fAsyncMethods[kFWSetAsyncRef_Read].func					= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_Read ;
	fAsyncMethods[kFWSetAsyncRef_Read].count0				= 3 ;
	fAsyncMethods[kFWSetAsyncRef_Read].count1				= 0 ;
	fAsyncMethods[kFWSetAsyncRef_Read].flags				= kIOUCScalarIScalarO ;

	fAsyncMethods[kFWCommand_Submit].object				= this ;
	fAsyncMethods[kFWCommand_Submit].func				= (IOAsyncMethod) & IOFireWireUserClient::userAsyncCommand_Submit ;
	fAsyncMethods[kFWCommand_Submit].count0				= 0xFFFFFFFF ;	// variable
	fAsyncMethods[kFWCommand_Submit].count1				= 0xFFFFFFFF ;
	fAsyncMethods[kFWCommand_Submit].flags				= kIOUCStructIStructO ;

	fAsyncMethods[kFWCommand_SubmitAbsolute].object		= this ;
	fAsyncMethods[kFWCommand_SubmitAbsolute].func		= (IOAsyncMethod) & IOFireWireUserClient::userAsyncCommand_SubmitAbsolute ;
	fAsyncMethods[kFWCommand_SubmitAbsolute].count0		= 0xFFFFFFFF ; // variable
	fAsyncMethods[kFWCommand_SubmitAbsolute].count1		= 0xFFFFFFFF ;
	fAsyncMethods[kFWCommand_SubmitAbsolute].flags		= kIOUCStructIStructO ;

}



IOExternalMethod* 
IOFireWireUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if( index >= kNumFireWireMethods )
        return NULL;
    else
    {
        *target = fMethods[index].object ;
        return &fMethods[index];
    }
}

IOExternalAsyncMethod* 
IOFireWireUserClient::getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if( index >= kNumFireWireAsyncMethods )
       return NULL;
   else
   {
       *target = fMethods[index].object ;
       return &fAsyncMethods[index];
   }
    return NULL;
}

IOReturn
IOFireWireUserClient::registerNotificationPort(
	mach_port_t port,
	UInt32		/* type */,
	UInt32		refCon)
{
	fNotificationPort = port ;
	fNotificationRefCon = refCon ;

    return( kIOReturnUnsupported);
}

IOReturn
IOFireWireUserClient::userOpen()
{

	IOReturn result = kIOReturnSuccess ;		
	
	if (fOwner->open(this))
	{
		fOpenClient = this ;
		result = kIOReturnSuccess ;
	}
	else
		result = kIOReturnExclusiveAccess ;

	return result ;
}

IOReturn
IOFireWireUserClient::userOpenWithSessionRef(IOService*	sessionRef)
{
	IOReturn	result = kIOReturnSuccess ;
	
	if (fOwner->isOpen())
	{
		IOService*	client = OSDynamicCast(IOService, sessionRef) ;
	
		if (!client)
			result = kIOReturnBadArgument ;
		else
		{
			while (client != NULL)
			{
				if (client == fOwner)
				{
					fOpenClient = sessionRef ;	// sessionRef is the originally passed in user object
				
					break ;
				}
				
				client = client->getProvider() ;
			}
		}
	}
	else
		result = kIOReturnNotOpen ;
		
	return result ;
}

IOReturn
IOFireWireUserClient::userClose()
{
	IOReturn result = kIOReturnSuccess ;
	
	if (!fOwner->isOpen( this ))
		result = kIOReturnNotOpen ;
	else
	{
		if (fOpenClient == this)
			fOwner->close(this) ;

		fOpenClient = NULL ;
	}		
	
	return result ;
}


IOReturn
IOFireWireUserClient::readQuad(UInt64 addr, UInt32 failOnReset, UInt32 generation, UInt32* val)
{
    IOReturn 				res;
    IOFWReadQuadCommand*	cmd = NULL;
    do {
        cmd = fOwner->createReadQuadCommand(*((FWAddress*)&addr), val, 1, NULL, NULL, failOnReset);
        if(!cmd) {
			res = kIOReturnNoMemory;
			break;
		}
		cmd->setGeneration(generation) ;

        res = cmd->submit();        // We block here until the command finishes

        if(kIOReturnSuccess == res)
            res = cmd->getStatus();

    } while(false);

    if(cmd)
        cmd->release();

    return res;
}


IOReturn
IOFireWireUserClient::readQuadAbsolute(UInt64 addr, UInt32 failOnReset, UInt32 generation, UInt32* val)
{
    IOReturn 				res;
    IOFWReadQuadCommand*	cmd = NULL;

	((FWAddress*)&addr)->nodeID = 0 ;
	
    do {
        cmd = fOwner->createReadQuadCommand(*((FWAddress*)&addr), val, 1, NULL, NULL, failOnReset);
        if(!cmd) {
			res = kIOReturnNoMemory;
			break;
		}

		cmd->setGeneration(generation) ;
        res = cmd->submit();
        // We block here until the command finishes
        if(kIOReturnSuccess == res)
            res = cmd->getStatus();

    } while(false);

    if(cmd)
        cmd->release();

    return res;
}

IOReturn
IOFireWireUserClient::read(
	FWReadWriteParams*	 	inParams,
	UInt32* 				outSize)
{
    IOReturn 				res;
    IOMemoryDescriptor *	mem = NULL;
    IOFWReadCommand*		cmd = NULL;

    do {
        *outSize = 0;

		mem = IOMemoryDescriptor::withAddress((vm_address_t)inParams->buf, (IOByteCount)inParams->size, kIODirectionIn, fTask);
		if(!mem) {
			res = kIOReturnNoMemory;
			break;
		}

		cmd = fOwner->createReadCommand(inParams->addr, mem, NULL, NULL, inParams->failOnReset);
		if(!cmd) {
			res = kIOReturnNoMemory;
			break;
		}
		cmd->setGeneration(inParams->generation) ;

		if ( kIOReturnSuccess != mem->prepare() )
			IOLog("%s %u: prepare failed\n", __FILE__, __LINE__) ;
        res = cmd->submit();
		mem->complete() ;

        // We block here until the command finishes
        if(kIOReturnSuccess == res)
            res = cmd->getStatus();
        if(kIOReturnSuccess == res)
            *outSize = cmd->getBytesTransferred();
		
    } while(false);

	if(cmd)
		cmd->release();
	if(mem)
		mem->release();

    return res;
}

IOReturn
IOFireWireUserClient::readAbsolute(
	FWReadWriteParams*	 	inParams,
	UInt32* 				outSize)
{
    IOReturn 				res;
    IOMemoryDescriptor *	mem = NULL;
    IOFWReadCommand*		cmd = NULL;

    do {
        *outSize = 0;

		mem = IOMemoryDescriptor::withAddress((vm_address_t)inParams->buf, inParams->size, kIODirectionIn, fTask);
		if(!mem) {
			res = kIOReturnNoMemory;
			break;
		}

		cmd = fOwner->createReadCommand(inParams->addr, mem, NULL, NULL, inParams->failOnReset);
		if(!cmd) {
			res = kIOReturnNoMemory;
			break;
		}

		cmd->setGeneration(inParams->generation) ;

		if ( kIOReturnSuccess != mem->prepare() )
			IOLog("%s %u: prepare failed\n", __FILE__, __LINE__) ;
        res = cmd->submit();
		mem->complete() ;
		
        // We block here until the command finishes
        if(kIOReturnSuccess == res)
            res = cmd->getStatus();
        if(kIOReturnSuccess == res)
               *outSize = cmd->getBytesTransferred();
    
    } while(false);

	if(cmd)
		cmd->release();
	if(mem)
		mem->release();

    return res;
}

IOReturn
IOFireWireUserClient::writeQuad(UInt64 addr, UInt32 val, UInt32 failOnReset, UInt32 generation)
{
    IOReturn 				res;
    IOFWWriteQuadCommand*	cmd = NULL;

    do {
		cmd = fOwner->createWriteQuadCommand(*((FWAddress*)&addr), & val, 1, NULL, NULL, failOnReset);
		if(!cmd) {
			res = kIOReturnNoMemory;
			break;
		}

	cmd->setGeneration(generation) ;
	res = cmd->submit();
	// We block here until the command finishes
	if(kIOReturnSuccess == res)
		res = cmd->getStatus();
    
    } while(false);

    if(cmd)
		cmd->release();

    return res;
}

IOReturn
IOFireWireUserClient::writeQuadAbsolute(UInt64 addr, UInt32 val, UInt32 failOnReset, UInt32 generation)
{
    IOReturn 				res;
    IOFWWriteQuadCommand*	cmd = NULL;

	((FWAddress*)&addr)->nodeID = 0 ;
    do {
		cmd = fOwner->createWriteQuadCommand(*((FWAddress*)&addr), & val, 1, NULL, NULL, failOnReset);
		if(!cmd) {
			res = kIOReturnNoMemory;
			break;
		}

	cmd->setGeneration(generation) ;
	res = cmd->submit();
	// We block here until the command finishes
	if(kIOReturnSuccess == res)
		res = cmd->getStatus();
    
    } while(false);

    if(cmd)
		cmd->release();

    return res;
}

IOReturn
IOFireWireUserClient::write(FWReadWriteParams*	inParams, UInt32* outSize)
//FWAddress addr, void* buf, UInt32 size, UInt32 failOnReset, UInt32 generation)
{
    IOReturn 				res;
    IOMemoryDescriptor *	mem = NULL;
    IOFWWriteCommand*		cmd = NULL;

    do {
		mem = IOMemoryDescriptor::withAddress((vm_address_t) inParams->buf, inParams->size, kIODirectionOut, fTask);
		if(!mem) {
			res = kIOReturnNoMemory;
			break;
		}

		cmd = fOwner->createWriteCommand(inParams->addr, mem, NULL, NULL, inParams->failOnReset);
		if(!cmd) {
			res = kIOReturnNoMemory;
			break;
		}

		cmd->setGeneration(inParams->generation) ;
		res = cmd->submit();
	
		// We block here until the command finishes
		if(kIOReturnSuccess == res)
			res = cmd->getStatus();
    
    } while(false);

    if(cmd)
		cmd->release();
    if(mem)
		mem->release();

    return res;
}

IOReturn
IOFireWireUserClient::writeAbsolute(FWReadWriteParams* inParams, UInt32* outSize)
{
    IOReturn 				res;
    IOMemoryDescriptor *	mem = NULL;
    IOFWWriteCommand*		cmd = NULL;

    do {
		mem = IOMemoryDescriptor::withAddress((vm_address_t) inParams->buf, inParams->size, kIODirectionOut, fTask);
		if(!mem) {
			res = kIOReturnNoMemory;
			break;
		}

		cmd = fOwner->createWriteCommand(inParams->addr, mem, NULL, NULL, inParams->failOnReset);
		if(!cmd) {
			res = kIOReturnNoMemory;
			break;
		}

		cmd->setGeneration(inParams->generation) ;
		res = cmd->submit();
	
		// We block here until the command finishes
		if(kIOReturnSuccess == res)
			res = cmd->getStatus();
    
    } while(false);

    if(cmd)
		cmd->release();
    if(mem)
		mem->release();

    return res;
}

IOReturn
IOFireWireUserClient::compareSwap(UInt64 addr, UInt32 cmpVal, UInt32 newVal, UInt32 failOnReset, UInt32 generation)
{
    IOReturn 			res;
    IOFWCompareAndSwapCommand *	cmd = NULL;

    cmd = fOwner->createCompareAndSwapCommand(*((FWAddress*)&addr), &cmpVal, &newVal, 1, NULL, NULL, failOnReset);
    if(!cmd) {
        return kIOReturnNoMemory;
    }

	cmd->setGeneration(generation) ;		
    res = cmd->submit();
    // We block here until the command finishes
    if(kIOReturnSuccess == res) {
        UInt32 oldVal;
        res = cmd->getStatus();
        if(kIOReturnSuccess == res && !cmd->locked(&oldVal))
            res = kIOReturnCannotLock;
    }

    if(cmd)
	cmd->release();

    return res;
}

IOReturn
IOFireWireUserClient::compareSwapAbsolute(UInt64 addr, UInt32 cmpVal, UInt32 newVal, UInt32 failOnReset, UInt32 generation)
{
    IOReturn 			res;
    IOFWCompareAndSwapCommand *	cmd = NULL;

	((FWAddress*)&addr)->nodeID = 0 ;
    cmd = fOwner->createCompareAndSwapCommand(*((FWAddress*) &addr), &cmpVal, &newVal, 1, NULL, NULL, failOnReset);
    if(!cmd) {
        return kIOReturnNoMemory;
    }

	cmd->setGeneration(generation) ;
    res = cmd->submit();
    // We block here until the command finishes
    if(kIOReturnSuccess == res) {
        UInt32 oldVal;
        res = cmd->getStatus();
        if(kIOReturnSuccess == res && !cmd->locked(&oldVal))
            res = kIOReturnCannotLock;
    }

    if(cmd)
	cmd->release();

    return res;
}

IOReturn IOFireWireUserClient::busReset()
{
    return fOwner->getController()->resetBus();
}

IOReturn
IOFireWireUserClient::getGenerationAndNodeID(
	UInt32*					outGeneration,
	UInt16*					outNodeID) const
{
    fOwner->getNodeIDGeneration(*outGeneration, *outNodeID);
	if (fOwner->getController()->checkGeneration(*outGeneration))		
		return kIOReturnSuccess ;
	else
		return kIOReturnNotFound ;	// nodeID we got was stale...
}

IOReturn
IOFireWireUserClient::getLocalNodeID(
	UInt16*					outLocalNodeID) const
{
    UInt16 nodeID;
    UInt32 generation;
    
    return fOwner->getNodeIDGeneration(generation, nodeID, *outLocalNodeID);
}

IOReturn
IOFireWireUserClient::getResetTime(
	AbsoluteTime*	outResetTime) const
{
	*outResetTime = *fOwner->getController()->getResetTime() ;

	return kIOReturnSuccess ;
}

IOReturn IOFireWireUserClient::message( UInt32 type, IOService * provider, void * arg )
{
	switch(type)
	{
		case kIOMessageServiceIsSuspended:
			if (fBusResetAsyncNotificationRef[0])
				sendAsyncResult(fBusResetAsyncNotificationRef, kIOReturnSuccess, NULL, 0) ;
			break ;
		case kIOMessageServiceIsResumed:
			if (fBusResetDoneAsyncNotificationRef[0])
				sendAsyncResult(fBusResetDoneAsyncNotificationRef, kIOReturnSuccess, NULL, 0) ;
			break ;
		case kIOFWMessageServiceIsRequestingClose:
			fOwner->messageClients(kIOMessageServiceIsRequestingClose) ;
			break;
		
	}
	
	IOUserClient::message(type, provider) ;
	
    return kIOReturnSuccess;
}

#pragma mark -
#pragma mark --user client/DCL

IOReturn
IOFireWireUserClient::getOSStringData(
	FWKernOSStringRef	inStringRef,
	UInt32				inStringLen,
	char*				inStringBuffer,
	UInt32*				outStringLen)
{
	*outStringLen = 0 ;

	if (!OSDynamicCast(OSString, inStringRef))
		return kIOReturnBadArgument ;
	
	UInt32 len = MIN(inStringLen, inStringRef->getLength()) ;

	IOMemoryDescriptor*	mem = IOMemoryDescriptor::withAddress((vm_address_t)inStringBuffer, len, kIODirectionOut, fTask) ;
	if (!mem)
		return kIOReturnNoMemory ;

	IOReturn result = mem->prepare() ;
	
	if (kIOReturnSuccess == result)
		*outStringLen = mem->writeBytes(0, inStringRef->getCStringNoCopy(), len) ;

	mem->complete() ;
	mem->release() ;
	
	return kIOReturnSuccess;//result ;
}

IOReturn
IOFireWireUserClient::getOSDataData(
	FWKernOSDataRef			inDataRef,
	IOByteCount				inDataLen,
	char*					inDataBuffer,
	IOByteCount*			outDataLen)
{
	if (!OSDynamicCast(OSData, inDataRef))
		return kIOReturnBadArgument ;
	
	UInt32 len = MIN(inDataRef->getLength(), inDataLen) ;
	IOMemoryDescriptor*	mem = IOMemoryDescriptor::withAddress((vm_address_t)inDataBuffer, len, kIODirectionOut, fTask) ;
	if (!mem)
		return kIOReturnNoMemory ;
	
	IOReturn result = mem->prepare() ;
	if (kIOReturnSuccess == result)
		*outDataLen = mem->writeBytes(0, inDataRef->getBytesNoCopy(), len) ;
	mem->complete() ;
	mem->release() ;
	
	return result ;
}

#pragma mark -
#pragma mark --user client/config directory methods
//	Config Directory methods

IOReturn
IOFireWireUserClient::unitDirCreate(
	FWKernUnitDirRef*	outDir)
{
	FWKernUnitDirRef newUnitDir = IOLocalConfigDirectory::create() ;

	IOFireWireUserClientLogIfNil_(*outDir, ("IOFireWireUserClient::UnitDirCreate: IOLocalConfigDirectory::create returned nil\n")) ;
	if (!newUnitDir)
		return kIOReturnNoMemory ;
		
	addObjectToSet(newUnitDir, fUserUnitDirectories) ;
	*outDir = newUnitDir ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::unitDirRelease(
	FWKernUnitDirRef	inDir)
{
	IOReturn	result = kIOReturnSuccess ;
	IOLocalConfigDirectory* dir = OSDynamicCast(IOLocalConfigDirectory, inDir) ;
	
	if (!dir)
		return kIOReturnBadArgument ;

	result = unpublish(dir) ;
	removeObjectFromSet(inDir, fUserUnitDirectories) ;

	return result ;
}

IOReturn
IOFireWireUserClient::addEntry_Buffer(
	FWKernUnitDirRef		inDir, 
	int 					key,
	char*					buffer,
	UInt32					kr_size)
{
	#if DEBUGGING_LEVEL > 0
	IOLog(	"IOFireWireFamily: IOFireWireUserClient::AddEntry_Buffer() (inDir=%08lX, buffer=%08lX, kr_size=%08lX)\n",
			(UInt32) inDir,
			(UInt32) buffer,
			kr_size );
	#endif
	
	IOReturn				kr = kIOReturnSuccess ;
	IOLocalConfigDirectory*	dir ;
	
	if ( NULL == (dir = OSDynamicCast(IOLocalConfigDirectory, inDir)) )
		kr = kIOReturnBadArgument ;
	else
	{
    	OSData *data = OSData::withBytes(buffer, kr_size) ;

		kr = dir->addEntry(key, data) ;
	}

	return kr ;
}

IOReturn
IOFireWireUserClient::addEntry_UInt32(
	FWKernUnitDirRef		inDir,
	int						key,
	UInt32					value)
{
	#if DEBUGGING_LEVEL > 0
	IOLog(	"IOFireWireFamily: IOFireWireUserClient::AddEntry_UInt32() (inDir=%08lX, value=%08lX)\n",
			(UInt32) inDir,
			(UInt32) value);
	#endif
	
	IOReturn				kr = kIOReturnSuccess ;
	IOLocalConfigDirectory*	dir ;

	if ( NULL == (dir = OSDynamicCast(IOLocalConfigDirectory, inDir)) )
		kr = kIOReturnBadArgument ;
	else
		kr = dir->addEntry(key, value) ;

	return kr ;
}

IOReturn
IOFireWireUserClient::addEntry_FWAddr(
	IOLocalConfigDirectory*	inDir,
	int						key,
	FWAddress				value)
{
	#if DEBUGGING_LEVEL > 0
	IOLog(	"IOFireWireUserClient::AddEntry_FWAddr() (inDir=%08lX, value=%04X%04X.%08lX)\n",
			(UInt32) inDir,
			value.nodeID,
			value.addressHi,
			value.addressLo );
	#endif

	IOReturn				kr = kIOReturnSuccess ;
	IOLocalConfigDirectory*	dir ;

	if ( NULL == (dir = OSDynamicCast(IOLocalConfigDirectory, inDir)) )
		kr = kIOReturnBadArgument ;
	else
		kr = dir->addEntry(key, value) ;
	
	return kr ;
}

IOReturn
IOFireWireUserClient::addEntry_UnitDir(
	IOLocalConfigDirectory*	inDir,
	int						key,
	IOLocalConfigDirectory*	inValue)
{
	IOReturn				result = kIOReturnSuccess ;
	IOLocalConfigDirectory*	dir ;
	IOLocalConfigDirectory*	value ;
	
	dir = OSDynamicCast(IOLocalConfigDirectory, inDir) ;
	value = OSDynamicCast(IOLocalConfigDirectory, inValue) ;
	
	if (!dir || !value)
		result = kIOReturnBadArgument ;
	else
		result = dir->addEntry(key, value) ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::publish(
	IOLocalConfigDirectory*	inDir)
{	
	IOReturn				kr = kIOReturnSuccess ;
	IOLocalConfigDirectory*	dir ;
	
	if ( NULL == (dir = OSDynamicCast(IOLocalConfigDirectory, inDir)) )
		kr = kIOReturnBadArgument ;

	if ( kIOReturnSuccess == kr )
	{
	    kr = fOwner->getController()->AddUnitDirectory(dir) ;		
	}

	return kr ;
}

IOReturn
IOFireWireUserClient::unpublish(
	IOLocalConfigDirectory*	inDir)
{
	IOReturn				kr = kIOReturnSuccess ;
	IOLocalConfigDirectory*	dir ;
	
	if ( NULL == (dir = OSDynamicCast(IOLocalConfigDirectory, inDir)) )
		kr = kIOReturnBadArgument ;
	
	if ( kIOReturnSuccess == kr)
		kr = fOwner->getBus()->RemoveUnitDirectory(dir) ;

	return kr ;
}
#pragma mark -
#pragma mark --user client/pseudo address space
//
// Address Space methods
//

IOReturn
IOFireWireUserClient::allocateAddressSpace(
	FWAddrSpaceCreateParams*	inParams,
	FWKernAddrSpaceRef* 		outKernAddrSpaceRef)
{
	IOReturn		err				= kIOReturnSuccess ;
	
	IOFWUserClientPseudoAddrSpace* newPseudoAddrSpace		= new IOFWUserClientPseudoAddrSpace;

	if ( !newPseudoAddrSpace )
		return kIOReturnNoMemory ;

	if ( !newPseudoAddrSpace->initAll( this, inParams ) )
	{
		newPseudoAddrSpace->release() ;
		return kIOReturnNoMemory ;
	}

	if ( !err )
		err = newPseudoAddrSpace->activate() ;

	if ( !err )
	{
		err = addObjectToSet(newPseudoAddrSpace, fUserPseudoAddrSpaces) ;
		
		if ( err )
			newPseudoAddrSpace->deactivate() ;
	}

	if ( err )
	{
		newPseudoAddrSpace->release() ;
		newPseudoAddrSpace = NULL ;
	}

#	if IOFIREWIREUSERCLIENTDEBUG > 0
	if ( newPseudoAddrSpace )
		fStatistics->pseudoAddressSpaces->setObject( newPseudoAddrSpace ) ;
#	endif

	*outKernAddrSpaceRef = (FWKernAddrSpaceRef) newPseudoAddrSpace ;

	return err ;
}

IOReturn
IOFireWireUserClient::releaseAddressSpace(
	IOFWUserClientPseudoAddrSpace*	inAddrSpace)
{
	IOReturn						result = kIOReturnSuccess ;

	if (!OSDynamicCast(IOFWUserClientPseudoAddrSpace, inAddrSpace))
		result = kIOReturnBadArgument ;

	inAddrSpace->deactivate() ;
	removeObjectFromSet(inAddrSpace, fUserPseudoAddrSpaces) ;
	
#	if IOFIREWIREUSERCLIENTDEBUG > 0
	fStatistics->pseudoAddressSpaces->removeObject( inAddrSpace ) ;
#	endif

	return result ;
}


IOReturn
IOFireWireUserClient::getPseudoAddressSpaceInfo(
	IOFWUserClientPseudoAddrSpace*	inAddrSpaceRef,
	UInt32*							outNodeID,
	UInt32*							outAddressHi,
	UInt32*							outAddressLo)
//	void**							outBuffer,
//	UInt32*							outBufferSize)
{
    IOReturn						result 	= kIOReturnSuccess ;
	IOFWUserClientPseudoAddrSpace*	me 		= OSDynamicCast(IOFWUserClientPseudoAddrSpace, inAddrSpaceRef) ;

	if (!me)
		result = kIOReturnBadArgument ;

	if (kIOReturnSuccess == result)
	{		
	    *outNodeID 		= me->getBase().nodeID ;
		*outAddressHi	= me->getBase().addressHi ;
		*outAddressLo	= me->getBase().addressLo ;
//		*outBuffer		= me-> ;
//		*outBufferSize	= me->getLength() ;
	}

	return result ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_Packet(
	OSAsyncReference		asyncRef,
	FWKernAddrSpaceRef		inAddrSpaceRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	#if DEBUGGING_LEVEL > 0
	IOLog("IOFireWireUserClient::SetAsyncRef_Packet: inCallback=%08lX, inUserRefCon=%08lX\n") ;
	#endif

	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserClientPseudoAddrSpace*	me		= OSDynamicCast(IOFWUserClientPseudoAddrSpace, (IOFWUserClientPseudoAddrSpace*) inAddrSpaceRef) ;

	if ( NULL == me)
		result = kIOReturnBadArgument ;

	if ( kIOReturnSuccess == result )
	{
		if (inCallback)
			IOUserClient::setAsyncReference(asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon) ;
		else
			asyncRef[0] = 0 ;

		me->setAsyncRef_Packet(asyncRef) ;
	}
	
	return result ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_SkippedPacket(
	OSAsyncReference		asyncRef,
	FWKernAddrSpaceRef		inAddrSpaceRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	#if DEBUGGING_LEVEL > 0
	IOLog("IOFireWireUserClient:: setAsyncRef_SkippedPacket: inCallback=%08lX, inUserRefCon=%08lX\n") ;
	#endif

	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserClientPseudoAddrSpace*	me		= OSDynamicCast(IOFWUserClientPseudoAddrSpace, (IOFWUserClientPseudoAddrSpace*) inAddrSpaceRef) ;

	if ( NULL == me)
		result = kIOReturnBadArgument ;

	if ( kIOReturnSuccess == result )
	{
		if (inCallback)
			IOUserClient::setAsyncReference(asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon) ;
		else
			asyncRef[0] = 0 ;
			
		me->setAsyncRef_SkippedPacket(asyncRef) ;
	}
	
	return result ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_Read(
	OSAsyncReference		asyncRef,
	FWKernAddrSpaceRef		inAddrSpaceRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserClientPseudoAddrSpace*	me		= OSDynamicCast(IOFWUserClientPseudoAddrSpace, (IOFWUserClientPseudoAddrSpace*) inAddrSpaceRef) ;

	if ( NULL == me)
		result = kIOReturnBadArgument ;

	if ( kIOReturnSuccess == result )
	{
		if (inCallback)
			IOUserClient::setAsyncReference(asyncRef, (mach_port_t) asyncRef[0], inCallback, inUserRefCon) ;
		else
			asyncRef[0] = 0 ;
			
		me->setAsyncRef_Read(asyncRef) ;
	}
	
	return result ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_BusReset(
	OSAsyncReference		asyncRef,
	void*					inCallback,
	void*					inRefCon,
	void*,
	void*,
	void*,
	void*)
{
	#if DEBUGGING_LEVEL > 0
	IOLog("IOFireWireUserClient::setAsyncRef_BusReset: inCallback=%08lX\n", inCallback) ;
	#endif
	
	IOUserClient::setAsyncReference(asyncRef, (mach_port_t) asyncRef[0], inCallback, inRefCon) ;
	
	bcopy(asyncRef, fBusResetAsyncNotificationRef, sizeof(OSAsyncReference)) ;

	return kIOReturnSuccess ;
}
	
IOReturn
IOFireWireUserClient::setAsyncRef_BusResetDone(
	OSAsyncReference		inAsyncRef,
	void*					inCallback,
	void*					inRefCon,
	void*,
	void*,
	void*,
	void*)
{
	#if DEBUGGING_LEVEL > 0
	IOLog("IOFireWireUserClient::setAsyncRef_BusResetDone: inCallback=%08lX\n", inCallback) ;
	#endif
	
	IOUserClient::setAsyncReference(inAsyncRef, (mach_port_t) inAsyncRef[0], inCallback, inRefCon) ;

	bcopy(inAsyncRef, fBusResetDoneAsyncNotificationRef, sizeof(OSAsyncReference)) ;

	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::clientCommandIsComplete(
	FWKernAddrSpaceRef		inAddrSpaceRef,
	FWClientCommandID		inCommandID,
	IOReturn				inResult)
{
	IOReturn	result = kIOReturnSuccess ;
	IOFWUserClientPseudoAddrSpace*	me	= OSDynamicCast(IOFWUserClientPseudoAddrSpace, inAddrSpaceRef) ;

	if (!me)
		result = kIOReturnBadArgument ;

	if (kIOReturnSuccess == result)
		me->clientCommandIsComplete(inCommandID, inResult) ;

	return result ;
}


#pragma mark
#pragma mark --config directories
IOReturn
IOFireWireUserClient::configDirectoryCreate(
	FWKernConfigDirectoryRef*	outDirRef)
{
	IOReturn result = fOwner->getConfigDirectory(*outDirRef);
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryRelease(
	FWKernConfigDirectoryRef	inDirRef)
{
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::configDirectoryUpdate(
	FWKernConfigDirectoryRef 	dirRef, 
	UInt32 						offset, 
	const UInt32*&				romBase)
{
	return kIOReturnUnsupported ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyType(
	FWKernConfigDirectoryRef	inDirRef,
	int							key,
	IOConfigKeyType*			outType)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getKeyType(key, *outType) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyValue_UInt32(
	FWKernConfigDirectoryRef	inDirRef,
	int							key,
	UInt32*						outValue,
	FWKernOSStringRef*			outString,
	UInt32*						outStringLen)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getKeyValue(key, *outValue, outString) ;

	if ( (*outString) && (kIOReturnSuccess == result) )
		*outStringLen = (*outString)->getLength() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyValue_Data(
	FWKernConfigDirectoryRef	inDirRef,
	int							key,
	FWKernOSDataRef*			outValue,
	IOByteCount*				outDataSize,
	FWKernOSStringRef*			outString,
	UInt32*						outStringLen)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getKeyValue(key, *outValue, outString) ;
	if (kIOReturnSuccess == result)
	{
		if (*outValue)
			*outDataSize = (*outValue)->getLength() ;
		if (*outString)
			*outStringLen = (*outString)->getLength() ;
	}
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyValue_ConfigDirectory(
	FWKernConfigDirectoryRef	inDirRef,
	int							key,
	FWKernConfigDirectoryRef*	outValue,
	FWKernOSStringRef*			outString,
	UInt32*						outStringLen)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getKeyValue(key, *outValue, outString) ;
	if ( (*outString) && (kIOReturnSuccess == result) )
		*outStringLen = (*outString)->getLength() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyOffset_FWAddress(
	FWKernConfigDirectoryRef	inDirRef,
	int							key,
	UInt32*						addressHi,
	UInt32*						addressLo,
	FWKernOSStringRef*			outString,
	UInt32*						outStringLen)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	FWAddress tempAddress ;
	IOReturn result = inDirRef->getKeyOffset(key, tempAddress, outString) ;

	if (kIOReturnSuccess == result)
	{
		*addressHi = tempAddress.nodeID << 16 | tempAddress.addressHi ;
		*addressLo = tempAddress.addressLo ;
		if (*outString)
			*outStringLen = (*outString)->getLength() ;
	}
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexType(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	IOConfigKeyType*			outType)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexType(index, *outType) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexKey(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	int*						outKey)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexKey(index, *outKey) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexValue_UInt32(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	UInt32*						outKey)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexValue(index, *outKey) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexValue_Data(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	FWKernOSDataRef*			outDataRef,
	IOByteCount*				outDataLen)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getIndexValue(index, *outDataRef) ;
	
	if (kIOReturnSuccess == result)
		if (*outDataRef)
			*outDataLen = (*outDataRef)->getLength() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexValue_String(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	FWKernOSStringRef*			outString,
	UInt32*						outStringLen)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getIndexValue(index, *outString) ;
	if ( (*outString) && (kIOReturnSuccess == result) )
		*outStringLen = (*outString)->getLength() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexValue_ConfigDirectory(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	FWKernConfigDirectoryRef*	outDirRef)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getIndexValue(index, *outDirRef) ;

	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexOffset_FWAddress(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	UInt32*						addressHi,
	UInt32*						addressLo)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	FWAddress tempAddress ;
	IOReturn result = inDirRef->getIndexOffset(index, tempAddress) ;
	
	if (kIOReturnSuccess == result)
	{
		*addressHi = (tempAddress.nodeID << 16) | tempAddress.addressHi ;
		*addressLo = tempAddress.addressLo ;
	}

	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexOffset_UInt32(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	UInt32*						outValue)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexOffset(index, *outValue) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexEntry(
	FWKernConfigDirectoryRef	inDirRef,
	int							index,
	UInt32*						outValue)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexEntry(index, *outValue) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetSubdirectories(
	FWKernConfigDirectoryRef	inDirRef,
	OSIterator**				outIterator)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getSubdirectories(*outIterator) ;		
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeySubdirectories(
	FWKernConfigDirectoryRef	inDirRef,
	int							key,
	OSIterator**				outIterator)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getKeySubdirectories(key, *outIterator) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetType(
	FWKernConfigDirectoryRef	inDirRef,
	int*						outType)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;

	*outType = inDirRef->getType() ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetNumEntries(
	FWKernConfigDirectoryRef	inDirRef,
	int*						outNumEntries)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	*outNumEntries = inDirRef->getNumEntries() ;
	return kIOReturnSuccess ;
}

#pragma mark -
#pragma mark --- physical addr spaces ----------
//
// --- physical addr spaces ----------
//

IOReturn
IOFireWireUserClient::allocatePhysicalAddressSpace(
	FWPhysicalAddrSpaceCreateParams* inParams,
	FWKernPhysicalAddrSpaceRef* outKernAddrSpaceRef)
{
	#if DEBUGGING_LEVEL > 0
	IOLog("Params: %08lX, %08lX, %08lX\n", inParams->size, inParams->backingStore, inParams->flags) ;
	#endif
	
	IOMemoryDescriptor*	mem = IOMemoryDescriptor::withAddress((vm_address_t)inParams->backingStore, inParams->size, kIODirectionNone, fTask) ;
	IOFWUserClientPhysicalAddressSpace*	addrSpace = new IOFWUserClientPhysicalAddressSpace ;
	IOReturn	result = kIOReturnSuccess ;
	
	if (!mem || !addrSpace)
		result = kIOReturnNoMemory ;
	
	#if DEBUGGING_LEVEL > 0
	IOLog("IOFireWireUserClient::allocatePhysicalAddressSpace: Mem: %08lX, addrSpace: %08lX\n", mem, addrSpace) ;
	#endif
	
	if (kIOReturnSuccess == result)
	{
		if (!addrSpace->initWithDesc(fOwner->getController(), mem))
			result = kIOReturnError ;
			
		#if DEBUGGING_LEVEL > 0
		IOLog("IOFireWireUserClient::allocatePhysicalAddressSpace: initWitDesc returned %08lX\n", result) ;
		#endif
	}
	
	if (kIOReturnSuccess == result)
	{
		result = addrSpace->activate() ;
		#if DEBUGGING_LEVEL > 0
		IOLog("IOFireWireUserClient::allocatePhysicalAddressSpace: activate returned %08lX\n", result) ;
		#endif
	}
	
	if (kIOReturnSuccess == result)
		result = addObjectToSet(addrSpace, fUserPhysicalAddrSpaces) ;
		
	if (kIOReturnSuccess != result)
	{
		if (mem)
			mem->release() ;
		if (addrSpace)
			addrSpace->release() ;
	}
	
	if (kIOReturnSuccess == result)
	{
		#if DEBUGGING_LEVEL > 0
		IOLog("IOFireWireUserClient::allocatePhysicalAddressSpace: setting outKernAddrSpaceRef to %08lX\n", addrSpace) ;
		#endif
		
		*(void**) outKernAddrSpaceRef = (void*) addrSpace ;
	}
	
	#if DEBUGGING_LEVEL > 0
	IOLog("IOFireWireUserClient::allocatePhysicalAddressSpace returning %08lX\n", result) ;
	#endif
	
	return result ;
	
}

IOReturn
IOFireWireUserClient::releasePhysicalAddressSpace(
	IOFWUserClientPhysicalAddressSpace*	inAddrSpace)
{
	if (!OSDynamicCast(IOFWUserClientPhysicalAddressSpace, inAddrSpace))
		return kIOReturnBadArgument ;
	
	inAddrSpace->deactivate() ;
	removeObjectFromSet(inAddrSpace, fUserPhysicalAddrSpaces) ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getPhysicalAddressSpaceSegmentCount(
	FWKernPhysicalAddrSpaceRef			inAddrSpace,
	UInt32*								outCount)
{
	if (!OSDynamicCast(IOFWUserClientPhysicalAddressSpace, inAddrSpace))
		return kIOReturnBadArgument ;

	*outCount = inAddrSpace->getSegmentCount() ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getPhysicalAddressSpaceSegments(
	FWKernPhysicalAddrSpaceRef			inAddrSpace,
	UInt32								inSegmentCount,
	IOPhysicalAddress*					outSegments,
	IOByteCount*						outSegmentLengths,
	UInt32*								outSegmentCount)
{
	IOFWUserClientPhysicalAddressSpace* addrSpace = OSDynamicCast(IOFWUserClientPhysicalAddressSpace, inAddrSpace) ;
	IOReturn	result = kIOReturnSuccess ;
	
	if (!addrSpace)
		result = kIOReturnBadArgument ;
		
	#if DEBUGGING_LEVEL > 0
	IOLog("IOFireWireUserClient::getPhysicalAddressSpaceSegments: addrSpace = %08lX\n", addrSpace) ;
	#endif
	
	if (kIOReturnSuccess == result)
	{		
		UInt32 segmentCount = MIN(addrSpace->getSegmentCount(), inSegmentCount) ;
	
		IOMemoryDescriptor*	outSegmentsMem	= IOMemoryDescriptor::withAddress(
													(vm_address_t)outSegments, 
													segmentCount*sizeof(IOPhysicalAddress), 
													kIODirectionOut, fTask) ;
		IOMemoryDescriptor*	outSegmentLengthsMem = IOMemoryDescriptor::withAddress(
													(vm_address_t)outSegmentLengths,
													segmentCount*sizeof(IOByteCount),
													kIODirectionOut, fTask) ;
														
		IOPhysicalAddress* 	segments = new IOPhysicalAddress[segmentCount] ;
		IOByteCount*		segmentLengths = new IOByteCount[segmentCount] ;
		
		if (!outSegmentsMem || !outSegmentLengthsMem || !segments || !segmentLengths)
			result = kIOReturnNoMemory ;
		
		if (kIOReturnSuccess == result)
		{
			*outSegmentCount = segmentCount ;
			result = addrSpace->getSegments(outSegmentCount, segments, segmentLengths) ;
			
			#if DEBUGGING_LEVEL > 0
			IOLog("IOFireWireUserClient::getPhysicalAddressSpaceSegments: getSegments result = %08lX\n", result) ;
			for(UInt32 i=0; i<segmentCount; i++)
				IOLog("IOFireWireUserClient::getPhysicalAddressSpaceSegments: address space segment %08lX at %08lX, len %08lX\n", 
						i, segments[i], segmentLengths[i]) ;
			#endif

			if (kIOReturnSuccess == result)
			{
				UInt32 bytes = 0 ;
				result = outSegmentsMem->prepare(kIODirectionOut) ;

				#if DEBUGGING_LEVEL > 0
				IOLog("IOFireWireUserClient::getPhysicalAddressSpaceSegments: outSegmentsMem->prepare result = %08lX\n", 
						result) ;
				#endif
				
				if (kIOReturnSuccess == result)
				{
					bytes = outSegmentsMem->writeBytes(0, segments, segmentCount*sizeof(IOPhysicalAddress)) ;

					#if DEBUGGING_LEVEL > 0
					IOLog("IOFireWireUserClient::getPhysicalAddressSpaceSegments: outSegmentsMem->writeBytes bytes = %08lX\n", bytes) ;
					#endif
				}
				
				if (kIOReturnSuccess == result)
				{
					result = outSegmentsMem->complete(kIODirectionOut) ;

					#if DEBUGGING_LEVEL > 0
					IOLog("outSegmentsMem->complete result = %08lX\n", result) ;
					#endif
				}
				
				if (kIOReturnSuccess == result)
				{
					result = outSegmentLengthsMem->prepare(kIODirectionOut) ;

					#if DEBUGGING_LEVEL > 0
					IOLog("outSegmentLengthsMem->prepare result = %08lX\n", result) ;
					#endif
				}
				
				if (kIOReturnSuccess == result)
				{
					bytes = outSegmentLengthsMem->writeBytes(0, segmentLengths, segmentCount*sizeof(IOByteCount)) ;

					#if DEBUGGING_LEVEL > 0
					IOLog("outSegmentLengthsMem->writeBytes bytes = %08lX\n", bytes) ;
					#endif
				}
				
				if (kIOReturnSuccess == result)
				{
					result = outSegmentLengthsMem->complete(kIODirectionOut) ;

					#if DEBUGGING_LEVEL > 0
					IOLog("outSegmentLengthsMem->complete result = %08lX\n", result) ;
					#endif
				}
			}
			
			
		}
		
		if (outSegmentsMem)
			outSegmentsMem->release() ;
		if (outSegmentLengthsMem)
			outSegmentLengthsMem->release() ;
		if (segments)
		{
			delete[] segments ;
			segments = NULL ;
		}
		if (segmentLengths)
		{
			delete[] segmentLengths ;
			segmentLengths = NULL ;
		}
	}
	
	return result ;
}

// async

IOReturn
IOFireWireUserClient::lazyAllocateUserCommand(
	FWUserCommandSubmitParams*	inParams,
	IOFWUserCommand**			outCommand)
{
	// lazy allocate new command
	IOReturn	result = kIOReturnSuccess ;
	
	*outCommand = IOFWUserCommand::withSubmitParams(inParams, this) ;

	if (!*outCommand)
		result = kIOReturnNoMemory ;
	else
		result = addObjectToSet(*outCommand, fUserCommandObjects) ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::userAsyncCommand_Submit(
	OSAsyncReference			asyncRef,
	FWUserCommandSubmitParams*	inParams,
	FWUserCommandSubmitResult*	outResult,
	IOByteCount					inParamsSize,
	IOByteCount*				outResultSize)
{

	IOReturn			result	= kIOReturnSuccess ;
	IOFWUserCommand*	me		= inParams->kernCommandRef ;
	
	if (me)
	{
		IOFireWireUserClientLog_(("using existing command object\n")) ;

		if (!OSDynamicCast(IOFWUserCommand, me))
			return kIOReturnBadArgument ;
	}
	else
	{
		IOFireWireUserClientLog_(("lazy allocating command object\n")) ;
	
		result = lazyAllocateUserCommand(inParams, & me) ;
		outResult->kernCommandRef = me ;
	}
	
	// assume 'me' valid
	if (kIOReturnSuccess == result)
	{
		IOUserClient::setAsyncReference( asyncRef, 
										 (mach_port_t) asyncRef[0], 
										 (void*)inParams->callback, 
										 (void*)inParams->refCon) ;

		me->setAsyncReference(asyncRef) ;
		result = me->submit( inParams, outResult ) ;
	}

	return result ;
}

IOReturn
IOFireWireUserClient::userAsyncCommand_SubmitAbsolute(
	OSAsyncReference			asyncRef,
	FWUserCommandSubmitParams*	inParams,
	FWUserCommandSubmitResult*	outResult,
	IOByteCount					inParamsSize,
	IOByteCount*				outResultSize)
{
	IOReturn			result	= kIOReturnSuccess ;
	IOFWUserCommand*	me		= inParams->kernCommandRef ;
	
	if (me)
	{
		if (!OSDynamicCast(IOFWUserCommand, me))
			return kIOReturnBadArgument ;
	}
	else
	{
		result = lazyAllocateUserCommand(inParams, & me) ;
		outResult->kernCommandRef = me ;
	}
	
	// assume 'me' valid
	if (kIOReturnSuccess == result)
	{
		IOUserClient::setAsyncReference( asyncRef, 
										 (mach_port_t) asyncRef[0], 
										 (void*)inParams->callback, 
										 (void*)inParams->refCon) ;

		me->setAsyncReference(asyncRef) ;
		result = me->submit( inParams, outResult ) ;
	}

	return result ;
}

IOReturn
IOFireWireUserClient::userAsyncCommand_Release(
	FWKernCommandRef		inCommandRef)
{
	IOReturn		result = kIOReturnSuccess ;

	if (!OSDynamicCast(IOFWUserCommand, inCommandRef))
		result = kIOReturnBadArgument ;

	removeObjectFromSet(inCommandRef, fUserCommandObjects) ;
	
	return result ;
}
