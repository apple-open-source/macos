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
/*
	$Log: IOFireWireUserClient.cpp,v $
	Revision 1.60  2002/11/20 00:34:27  niels
	fix minor bug in getAsyncTargetAndMethodForIndex
	
	Revision 1.59  2002/10/18 23:29:45  collin
	fix includes, fix cast which fails on new compiler
	
	Revision 1.58  2002/10/17 00:29:44  collin
	reenable FireLog
	
	Revision 1.57  2002/10/16 21:42:00  niels
	no longer panic trying to get config directory on local node.. still can't get the directory, however
	
	Revision 1.56  2002/09/25 00:27:25  niels
	flip your world upside-down
	
	Revision 1.55  2002/09/12 22:41:54  niels
	add GetIRMNodeID() to user client
	
*/

#include <IOKit/IOTypes.h>
#import <IOKit/IOLib.h>

#import <sys/proc.h>

// public
#import <IOKit/firewire/IOFireWireFamilyCommon.h>
#import <IOKit/firewire/IOFireWireNub.h>
#import <IOKit/firewire/IOLocalConfigDirectory.h>
#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOFireWireDevice.h>
#import <IOKit/firewire/IOFireLog.h>

// protected
#import <IOKit/firewire/IOFireWireLink.h>

// private
#import "IOFireWireUserClient.h"
#import "IOFWUserPseudoAddressSpace.h"
#import "IOFWUserPhysicalAddressSpace.h"
#import "IOFWUserIsochChannel.h"
#import "IOFWUserIsochPort.h"
#import "IOFireWireLibPriv.h"
#import "IOFireWireLocalNode.h"
#import "IOFWUserCommand.h"

// system
#import <IOKit/IOMessage.h>

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

		if ( result )
			result = (NULL != (fUserRemoteConfigDirectories = OSSet::withCapacity(0)) ) ;

#if IOFIREWIREUSERCLIENTDEBUG > 0
		if ( result )
			result = (NULL != (fStatistics = new IOFireWireUserClientStatistics ) ) ;
		
		if ( result )
			result = ( NULL != ( fStatistics->dict = OSDictionary::withCapacity(4)) ) ;
#endif
		
		if (!result)
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

#if IOFIREWIREUSERCLIENTDEBUG > 0
			if ( fStatistics->dict )
				fStatistics->dict->release() ;
			if ( fStatistics )
				delete fStatistics ;
#endif
		}

		IOLockUnlock(fSetLock) ;
	}

	// borrowed from bsd/vm/vm_unix.c, function pid_for_task() which has some other weird crap
	// going on too... I just took this part:
	if ( result )
	{
		proc*		p 			= (proc*)get_bsdtask_info( fTask );
		OSNumber*	pidProp 	= OSNumber::withNumber( p->p_pid, sizeof(p->p_pid) * 8 ) ;
		if ( pidProp )
		{
			setProperty( "Owning PID", pidProp ) ;
			pidProp->release() ;			// property table takes a reference
		}
		else
			result = false ;
	}
	
	#if IOFIREWIREUSERCLIENTDEBUG > 0
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
	
	OSCollectionIterator* 	iterator ;
	IOReturn				err		= kIOReturnSuccess ;

	
	if ( fUserPseudoAddrSpaces )
	{
		iterator	= OSCollectionIterator::withCollection( fUserPseudoAddrSpaces )  ;
		
		if ( !iterator )
			IOLog("%s %u: Couldn't get iterator to clean up pseudo address spaces\n", __FILE__, __LINE__) ;
		else
		{
			IOFWUserPseudoAddressSpace*	addrSpace ;
			while ( NULL != (addrSpace = OSDynamicCast( IOFWUserPseudoAddressSpace, iterator->getNextObject() ) ) )
				addrSpace->deactivate() ;
					
			iterator->release() ;
		}
		
//		fUserIsochChannels->flushCollection() ;
		fUserPseudoAddrSpaces->release() ;
		fUserPseudoAddrSpaces = NULL ;
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
		
//		fUserIsochChannels->flushCollection() ;
		fUserPhysicalAddrSpaces->release() ;
		fUserPhysicalAddrSpaces = NULL ;
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

//		fUserIsochChannels->flushCollection() ;
		fUserIsochChannels->release() ;
		fUserIsochChannels = NULL ;
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

//		fUserIsochPorts->flushCollection() ;
		fUserIsochPorts->release() ;
		fUserIsochPorts = NULL ;
	}
	
	if (fUserCommandObjects)
	{
//		fUserCommandObjects->flushCollection() ;
		fUserCommandObjects->release() ;	// should we find a way to cancel these or wait for them to complete?
		fUserCommandObjects = NULL ;
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
					IOLog("IOFireWireUserClient::clientClose: getController()->RemoteUnitDirectory() failed with error 0x%08lX\n", (UInt32) err) ;
			}
			
			iterator->release() ;
		}

//		fUserUnitDirectories->flushCollection() ;
		fUserUnitDirectories->release() ;
		fUserUnitDirectories = NULL ;
	}

	if ( fUserRemoteConfigDirectories )
	{
//		fUserRemoteConfigDirectories->flushCollection() ;
		fUserRemoteConfigDirectories->release() ;
		fUserRemoteConfigDirectories = NULL ;
	}

	IOLockUnlock(fSetLock) ;
}

void
IOFireWireUserClient::free()
{
#if IOFIREWIREUSERCLIENTDEBUG > 0
	if ( fStatistics )
	{
		if ( fStatistics->dict )
			fStatistics->dict->release() ;
		
		if ( fStatistics->isochCallbacks )
			fStatistics->isochCallbacks->release() ;
		
		if ( fStatistics->pseudoAddressSpaces )
		{
//			fStatistics->pseudoAddressSpaces->flushCollection() ;
			fStatistics->pseudoAddressSpaces->release() ;
		}
	}
	
	delete fStatistics ;
	fStatistics = NULL ;

#endif

	deallocateSets() ;

	IOLockFree(fSetLock) ;	
	
	IOUserClient::free() ;
}

IOReturn
IOFireWireUserClient::clientClose()
{
	IOReturn	result = userClose() ;

	if ( result == kIOReturnSuccess )
		IOFireWireUserClientLog_("IOFireWireUserClient::clientClose(): client left user client open, should call close. Closing...\n") ;
	else if ( result == kIOReturnNotOpen )
		result = kIOReturnSuccess ;

	if ( !terminate() )
		IOLog("IOFireWireUserClient::clientClose: terminate failed!, fOwner->isOpen(this) returned %u\n", fOwner->isOpen(this)) ;

	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient::clientDied( void )
{
	return( clientClose() );
}

IOReturn
IOFireWireUserClient::setProperties(
	OSObject*				properties )
{
	IOReturn result = kIOReturnSuccess ;

	OSDictionary*	dict = OSDynamicCast( OSDictionary, properties ) ;

	if ( dict )
	{
		OSObject*	value = dict->getObject( "unsafe bus resets" ) ;

		if ( value and OSDynamicCast(OSNumber, value ) )
		{
			fUnsafeResets = ( ((OSNumber*)value)->unsigned8BitValue() != 0 ) ;
		}
		else
		{
			result = IOUserClient::setProperties(properties) ;
		}
	}
	else
		result = IOUserClient::setProperties(properties) ;
	
	return result ;
}

#pragma mark -
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

#pragma mark -
void 
IOFireWireUserClient::initMethodTable()
{
	fMethods[kOpen].object = this ;
	fMethods[kOpen].func = (IOMethod) & IOFireWireUserClient::userOpen ;
	fMethods[kOpen].count0 = 0 ;
	fMethods[kOpen].count1 = 0 ;
	fMethods[kOpen].flags = kIOUCScalarIScalarO ;
	
	fMethods[kOpenWithSessionRef].object = this ;
	fMethods[kOpenWithSessionRef].func = (IOMethod) & IOFireWireUserClient::userOpenWithSessionRef ;
	fMethods[kOpenWithSessionRef].count0 = 1 ;
	fMethods[kOpenWithSessionRef].count1 = 0 ;
	fMethods[kOpenWithSessionRef].flags = kIOUCScalarIScalarO ;
	
	fMethods[kClose].object = this ;
	fMethods[kClose].func = (IOMethod) & IOFireWireUserClient::userClose ;
	fMethods[kClose].count0 = 0 ;
	fMethods[kClose].count1 = 0 ;
	fMethods[kClose].flags = kIOUCScalarIScalarO ;
	
	fMethods[kReadQuad].object 	= this;
	fMethods[kReadQuad].func	= (IOMethod)&IOFireWireUserClient::readQuad;
	fMethods[kReadQuad].count0	= sizeof( ReadQuadParams ) ;//4;
	fMethods[kReadQuad].count1	= sizeof( UInt32 ) ;//1;
	fMethods[kReadQuad].flags	= kIOUCStructIStructO;
	
	fMethods[kRead].object = this;
	fMethods[kRead].func =
		(IOMethod)&IOFireWireUserClient::read;
	fMethods[kRead].count0 = sizeof( ReadParams );
	fMethods[kRead].count1 = sizeof( IOByteCount ) ;
	fMethods[kRead].flags = kIOUCStructIStructO;
	
	fMethods[kWriteQuad].object	= this;
	fMethods[kWriteQuad].func	= (IOMethod)&IOFireWireUserClient::writeQuad;
	fMethods[kWriteQuad].count0	= sizeof( WriteQuadParams ) ;//5;
	fMethods[kWriteQuad].count1	= 0;
	fMethods[kWriteQuad].flags	= kIOUCStructIStructO;
	
	fMethods[kWrite].object = this;
	fMethods[kWrite].func =
		(IOMethod)&IOFireWireUserClient::write;
	fMethods[kWrite].count0 = sizeof( WriteParams ) ;
	fMethods[kWrite].count1 = sizeof( IOByteCount ) ;
	fMethods[kWrite].flags = kIOUCStructIStructO;
	
	fMethods[kCompareSwap].object = this;
	fMethods[kCompareSwap].func =
		(IOMethod)&IOFireWireUserClient::compareSwap;
	fMethods[kCompareSwap].count0 = sizeof( CompareSwapParams ) ;
	fMethods[kCompareSwap].count1 = sizeof(UInt64);
	fMethods[kCompareSwap].flags = kIOUCStructIStructO ;
	
	fMethods[kBusReset].object = this;
	fMethods[kBusReset].func =
		(IOMethod)&IOFireWireUserClient::busReset;
	fMethods[kBusReset].count0 = 0;
	fMethods[kBusReset].count1 = 0;
	fMethods[kBusReset].flags = kIOUCScalarIScalarO;
	
	// Need to take workloop lock in case hw is sleeping. Controller does that.
	fMethods[kCycleTime].object = fOwner->getController();
	fMethods[kCycleTime].func =
		(IOMethod)&IOFireWireController::getCycleTime;
	fMethods[kCycleTime].count0 = 0;
	fMethods[kCycleTime].count1 = 1;
	fMethods[kCycleTime].flags = kIOUCScalarIScalarO;
	
	// Need to take workloop lock in case hw is sleeping. Controller does that.
	fMethods[kGetBusCycleTime].object = fOwner->getController();
	fMethods[kGetBusCycleTime].func =
		(IOMethod)&IOFireWireController::getBusCycleTime;
	fMethods[kGetBusCycleTime].count0 = 0;
	fMethods[kGetBusCycleTime].count1 = 2;
	fMethods[kGetBusCycleTime].flags = kIOUCScalarIScalarO;
	
	fMethods[kGetGenerationAndNodeID].object = this ;
	fMethods[kGetGenerationAndNodeID].func =
		(IOMethod) & IOFireWireUserClient::getGenerationAndNodeID ;
	fMethods[kGetGenerationAndNodeID].count0 = 0 ;
	fMethods[kGetGenerationAndNodeID].count1 = 2 ;
	fMethods[kGetGenerationAndNodeID].flags = kIOUCScalarIScalarO ;
	
	fMethods[kGetLocalNodeID].object = this ;
	fMethods[kGetLocalNodeID].func =
		(IOMethod) & IOFireWireUserClient::getLocalNodeID ;
	fMethods[kGetLocalNodeID].count0 = 0 ;
	fMethods[kGetLocalNodeID].count1 = 1 ;
	fMethods[kGetLocalNodeID].flags = kIOUCScalarIScalarO ;
	
	fMethods[kGetResetTime].object = this ;
	fMethods[kGetResetTime].func =
		(IOMethod) & IOFireWireUserClient::getResetTime ;
	fMethods[kGetResetTime].count0 = 0 ;
	fMethods[kGetResetTime].count1 = sizeof(AbsoluteTime) ;	// this is 2 because we're returning an AbsoluteTime
	fMethods[kGetResetTime].flags = kIOUCStructIStructO ;
	
	fMethods[kGetOSStringData].object = this ;
	fMethods[kGetOSStringData].func = (IOMethod) & IOFireWireUserClient::getOSStringData ;
	fMethods[kGetOSStringData].count0 = 3 ;
	fMethods[kGetOSStringData].count1 = 1 ;
	fMethods[kGetOSStringData].flags = kIOUCScalarIScalarO ;
	
	fMethods[kGetOSDataData].object = this ;
	fMethods[kGetOSDataData].func = (IOMethod) & IOFireWireUserClient::getOSDataData ;
	fMethods[kGetOSDataData].count0 = 3 ;
	fMethods[kGetOSDataData].count1 = 1 ;
	fMethods[kGetOSDataData].flags = kIOUCScalarIScalarO ;
	
	// -- Config ROM Methods ----------
	
	fMethods[kUnitDirCreate].object			= this;
	fMethods[kUnitDirCreate].func 			= (IOMethod)&IOFireWireUserClient::unitDirCreate ;
	fMethods[kUnitDirCreate].count0 			= 0;
	fMethods[kUnitDirCreate].count1 			= 1;
	fMethods[kUnitDirCreate].flags 			= kIOUCScalarIScalarO ;
	
	fMethods[kUnitDirRelease].object			= this ;
	fMethods[kUnitDirRelease].func			= (IOMethod) & IOFireWireUserClient::unitDirRelease ;
	fMethods[kUnitDirRelease].count0			= 1 ;
	fMethods[kUnitDirRelease].count1			= 0 ;
	fMethods[kUnitDirRelease].flags			= kIOUCScalarIScalarO ;
	
	fMethods[kUnitDirAddEntry_Buffer].object	= this ;
	fMethods[kUnitDirAddEntry_Buffer].func	= (IOMethod)&IOFireWireUserClient::addEntry_Buffer ;
	fMethods[kUnitDirAddEntry_Buffer].count0	= 2 ;
	fMethods[kUnitDirAddEntry_Buffer].count1	= 0xFFFFFFFF ;	// variable
	fMethods[kUnitDirAddEntry_Buffer].flags	= kIOUCScalarIStructI ;
	
	fMethods[kUnitDirAddEntry_UInt32].object	= this ;
	fMethods[kUnitDirAddEntry_UInt32].func	= (IOMethod)&IOFireWireUserClient::addEntry_UInt32 ;
	fMethods[kUnitDirAddEntry_UInt32].count0	= 3 ;
	fMethods[kUnitDirAddEntry_UInt32].count1	= 0 ;
	fMethods[kUnitDirAddEntry_UInt32].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kUnitDirAddEntry_FWAddr].object	= this ;
	fMethods[kUnitDirAddEntry_FWAddr].func	= (IOMethod)&IOFireWireUserClient::addEntry_FWAddr ;
	fMethods[kUnitDirAddEntry_FWAddr].count0	= 2 ;
	fMethods[kUnitDirAddEntry_FWAddr].count1	= sizeof(FWAddress) ;	// sizeof(FWAddress)
	fMethods[kUnitDirAddEntry_FWAddr].flags	= kIOUCScalarIStructI ;
	
	fMethods[kUnitDirAddEntry_UnitDir].object	= this ;
	fMethods[kUnitDirAddEntry_UnitDir].func	= (IOMethod)&IOFireWireUserClient::addEntry_UnitDir ;
	fMethods[kUnitDirAddEntry_UnitDir].count0	= 3 ;
	fMethods[kUnitDirAddEntry_UnitDir].count1	= 0 ;
	fMethods[kUnitDirAddEntry_UnitDir].flags	= kIOUCScalarIStructI ;
	
	fMethods[kUnitDirPublish].object			= this ;
	fMethods[kUnitDirPublish].func			= (IOMethod) & IOFireWireUserClient::publish ;
	fMethods[kUnitDirPublish].count0			= 1 ;
	fMethods[kUnitDirPublish].count1			= 0 ;
	fMethods[kUnitDirPublish].flags			= kIOUCScalarIScalarO ;
	
	fMethods[kUnitDirUnpublish].object		= this ;
	fMethods[kUnitDirUnpublish].func			= (IOMethod) & IOFireWireUserClient::unpublish ;
	fMethods[kUnitDirUnpublish].count0		= 1 ;
	fMethods[kUnitDirUnpublish].count1		= 0 ;
	fMethods[kUnitDirUnpublish].flags			= kIOUCScalarIScalarO ;
	
	// --- Pseudo Address Space Methods ---------
	
	fMethods[kPseudoAddrSpace_Allocate].object	= this ;
	fMethods[kPseudoAddrSpace_Allocate].func		= (IOMethod) & IOFireWireUserClient::allocateAddressSpace ;
	fMethods[kPseudoAddrSpace_Allocate].count0	= sizeof(AddressSpaceCreateParams) ;
	fMethods[kPseudoAddrSpace_Allocate].count1	= sizeof(KernAddrSpaceRef) ;
	fMethods[kPseudoAddrSpace_Allocate].flags		= kIOUCStructIStructO ;
	
	fMethods[kPseudoAddrSpace_Release].object		= this ;
	fMethods[kPseudoAddrSpace_Release].func		= (IOMethod) & IOFireWireUserClient::releaseAddressSpace ;
	fMethods[kPseudoAddrSpace_Release].count0		= 1 ;
	fMethods[kPseudoAddrSpace_Release].count1		= 0 ;
	fMethods[kPseudoAddrSpace_Release].flags		= kIOUCScalarIScalarO ;
	
	fMethods[kPseudoAddrSpace_GetFWAddrInfo].object	= this ;
	fMethods[kPseudoAddrSpace_GetFWAddrInfo].func 	= (IOMethod) & IOFireWireUserClient::getPseudoAddressSpaceInfo ;
	fMethods[kPseudoAddrSpace_GetFWAddrInfo].count0	= 1 ;
	fMethods[kPseudoAddrSpace_GetFWAddrInfo].count1	= 3 ;
	fMethods[kPseudoAddrSpace_GetFWAddrInfo].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kPseudoAddrSpace_ClientCommandIsComplete].object	= this ;
	fMethods[kPseudoAddrSpace_ClientCommandIsComplete].func	= (IOMethod) & IOFireWireUserClient::clientCommandIsComplete ;
	fMethods[kPseudoAddrSpace_ClientCommandIsComplete].count0	= 3 ;
	fMethods[kPseudoAddrSpace_ClientCommandIsComplete].count1	= 0 ;
	fMethods[kPseudoAddrSpace_ClientCommandIsComplete].flags	= kIOUCScalarIScalarO ;
	
	// --- Physical Address Space Methods ---------
	
	fMethods[kPhysicalAddrSpace_Allocate].object 			= this ;
	fMethods[kPhysicalAddrSpace_Allocate].func				= (IOMethod) & IOFireWireUserClient::allocatePhysicalAddressSpace ;
	fMethods[kPhysicalAddrSpace_Allocate].count0			= sizeof( PhysicalAddressSpaceCreateParams ) ;
	fMethods[kPhysicalAddrSpace_Allocate].count1			= sizeof( KernPhysicalAddrSpaceRef ) ;
	fMethods[kPhysicalAddrSpace_Allocate].flags				= kIOUCStructIStructO ;
	
	fMethods[kPhysicalAddrSpace_Release].object 			= this ;
	fMethods[kPhysicalAddrSpace_Release].func				= (IOMethod) & IOFireWireUserClient::releasePhysicalAddressSpace ;
	fMethods[kPhysicalAddrSpace_Release].count0				= 1 ;
	fMethods[kPhysicalAddrSpace_Release].count1				= 0 ;
	fMethods[kPhysicalAddrSpace_Release].flags				= kIOUCScalarIScalarO ;
	
	fMethods[kPhysicalAddrSpace_GetSegmentCount].object		= this ;
	fMethods[kPhysicalAddrSpace_GetSegmentCount].func		= (IOMethod) & IOFireWireUserClient::getPhysicalAddressSpaceSegmentCount ;
	fMethods[kPhysicalAddrSpace_GetSegmentCount].count0		= 1 ;
	fMethods[kPhysicalAddrSpace_GetSegmentCount].count1		= 1 ;
	fMethods[kPhysicalAddrSpace_GetSegmentCount].flags		= kIOUCScalarIScalarO ;
	
	fMethods[kPhysicalAddrSpace_GetSegments].object			= this ;
	fMethods[kPhysicalAddrSpace_GetSegments].func				= (IOMethod) & IOFireWireUserClient::getPhysicalAddressSpaceSegments ;
	fMethods[kPhysicalAddrSpace_GetSegments].count0			= 4 ;	
	fMethods[kPhysicalAddrSpace_GetSegments].count1			= 1 ;
	fMethods[kPhysicalAddrSpace_GetSegments].flags			= kIOUCScalarIScalarO ;
	
	fMethods[kClientCommandIsComplete].object	= this ;
	fMethods[kClientCommandIsComplete].func	= (IOMethod) & IOFireWireUserClient::clientCommandIsComplete ;
	fMethods[kClientCommandIsComplete].count0	= 2 ;
	fMethods[kClientCommandIsComplete].count1	= 0 ;
	fMethods[kClientCommandIsComplete].flags	= kIOUCScalarIScalarO ;
	
	// --- config directory ----------------------
	fMethods[kConfigDirectoryCreate].object = this ;
	fMethods[kConfigDirectoryCreate].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryCreate ;
	fMethods[kConfigDirectoryCreate].count0 = 0 ;
	fMethods[kConfigDirectoryCreate].count1 = 1 ;
	fMethods[kConfigDirectoryCreate].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryRelease].object	= this ;
	fMethods[kConfigDirectoryRelease].func	= (IOMethod) & IOFireWireUserClient::configDirectoryRelease ;
	fMethods[kConfigDirectoryRelease].count0	= 1 ;
	fMethods[kConfigDirectoryRelease].count1	= 0 ;
	fMethods[kConfigDirectoryRelease].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryUpdate].object 	= this ;
	fMethods[kConfigDirectoryUpdate].func 	= 
			(IOMethod) & IOFireWireUserClient::configDirectoryUpdate ;
	fMethods[kConfigDirectoryUpdate].count0 	= 1 ;
	fMethods[kConfigDirectoryUpdate].count1 	= 0 ;
	fMethods[kConfigDirectoryUpdate].flags 	= kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetKeyType].object = this ;
	fMethods[kConfigDirectoryGetKeyType].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyType ;
	fMethods[kConfigDirectoryGetKeyType].count0 = 2 ;
	fMethods[kConfigDirectoryGetKeyType].count1 = 1 ;
	fMethods[kConfigDirectoryGetKeyType].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetKeyValue_UInt32].object = this ;
	fMethods[kConfigDirectoryGetKeyValue_UInt32].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyValue_UInt32 ;
	fMethods[kConfigDirectoryGetKeyValue_UInt32].count0 = 3 ;
	fMethods[kConfigDirectoryGetKeyValue_UInt32].count1 = 3 ;
	fMethods[kConfigDirectoryGetKeyValue_UInt32].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetKeyValue_Data].object = this ;
	fMethods[kConfigDirectoryGetKeyValue_Data].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyValue_Data ;
	fMethods[kConfigDirectoryGetKeyValue_Data].count0 = 3 ;
	fMethods[kConfigDirectoryGetKeyValue_Data].count1 = sizeof(GetKeyValueDataResults) ;
	fMethods[kConfigDirectoryGetKeyValue_Data].flags = kIOUCScalarIStructO ;
	
	fMethods[kConfigDirectoryGetKeyValue_ConfigDirectory].object = this ;
	fMethods[kConfigDirectoryGetKeyValue_ConfigDirectory].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyValue_ConfigDirectory ;
	fMethods[kConfigDirectoryGetKeyValue_ConfigDirectory].count0 = 3 ;
	fMethods[kConfigDirectoryGetKeyValue_ConfigDirectory].count1 = 3 ;
	fMethods[kConfigDirectoryGetKeyValue_ConfigDirectory].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetKeyOffset_FWAddress].object = this ;
	fMethods[kConfigDirectoryGetKeyOffset_FWAddress].func = 
			(IOMethod) & IOFireWireUserClient::configDirectoryGetKeyOffset_FWAddress ;
	fMethods[kConfigDirectoryGetKeyOffset_FWAddress].count0 = 3 ;
	fMethods[kConfigDirectoryGetKeyOffset_FWAddress].count1 = sizeof(GetKeyOffsetResults) ;
	fMethods[kConfigDirectoryGetKeyOffset_FWAddress].flags = kIOUCScalarIStructO ;
	
	fMethods[kConfigDirectoryGetIndexType].object = this ;
	fMethods[kConfigDirectoryGetIndexType].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexType ;
	fMethods[kConfigDirectoryGetIndexType].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexType].count1 = 1 ;
	fMethods[kConfigDirectoryGetIndexType].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetIndexKey].object = this ;
	fMethods[kConfigDirectoryGetIndexKey].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexKey ;
	fMethods[kConfigDirectoryGetIndexKey].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexKey].count1 = 1 ;
	fMethods[kConfigDirectoryGetIndexKey].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetIndexValue_UInt32].object = this ;
	fMethods[kConfigDirectoryGetIndexValue_UInt32].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexValue_UInt32 ;
	fMethods[kConfigDirectoryGetIndexValue_UInt32].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexValue_UInt32].count1 = 1 ;
	fMethods[kConfigDirectoryGetIndexValue_UInt32].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetIndexValue_Data].object = this ;
	fMethods[kConfigDirectoryGetIndexValue_Data].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexValue_Data ;
	fMethods[kConfigDirectoryGetIndexValue_Data].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexValue_Data].count1 = 2 ;
	fMethods[kConfigDirectoryGetIndexValue_Data].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetIndexValue_String].object = this ;
	fMethods[kConfigDirectoryGetIndexValue_String].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexValue_String ;
	fMethods[kConfigDirectoryGetIndexValue_String].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexValue_String].count1 = 2 ;
	fMethods[kConfigDirectoryGetIndexValue_String].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetIndexValue_ConfigDirectory].object = this ;
	fMethods[kConfigDirectoryGetIndexValue_ConfigDirectory].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexValue_ConfigDirectory ;
	fMethods[kConfigDirectoryGetIndexValue_ConfigDirectory].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexValue_ConfigDirectory].count1 = 1 ;
	fMethods[kConfigDirectoryGetIndexValue_ConfigDirectory].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetIndexOffset_FWAddress].object = this ;
	fMethods[kConfigDirectoryGetIndexOffset_FWAddress].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexOffset_FWAddress ;
	fMethods[kConfigDirectoryGetIndexOffset_FWAddress].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexOffset_FWAddress].count1 = 2 ;
	fMethods[kConfigDirectoryGetIndexOffset_FWAddress].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetIndexOffset_UInt32].object = this ;
	fMethods[kConfigDirectoryGetIndexOffset_UInt32].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexOffset_UInt32 ;
	fMethods[kConfigDirectoryGetIndexOffset_UInt32].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexOffset_UInt32].count1 = 1 ;
	fMethods[kConfigDirectoryGetIndexOffset_UInt32].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetIndexEntry].object = this ;
	fMethods[kConfigDirectoryGetIndexEntry].func =
			(IOMethod) & IOFireWireUserClient::configDirectoryGetIndexEntry ;
	fMethods[kConfigDirectoryGetIndexEntry].count0 = 2 ;
	fMethods[kConfigDirectoryGetIndexEntry].count1 = 1 ;
	fMethods[kConfigDirectoryGetIndexEntry].flags = kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetSubdirectories].object	= this ;
	fMethods[kConfigDirectoryGetSubdirectories].func 		= (IOMethod) & IOFireWireUserClient::configDirectoryGetSubdirectories ;
	fMethods[kConfigDirectoryGetSubdirectories].count0	= 1 ;
	fMethods[kConfigDirectoryGetSubdirectories].count1	= 1 ;
	fMethods[kConfigDirectoryGetSubdirectories].flags		= kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetKeySubdirectories].object	= this ;
	fMethods[kConfigDirectoryGetKeySubdirectories].func 	= (IOMethod) & IOFireWireUserClient::configDirectoryGetKeySubdirectories ;
	fMethods[kConfigDirectoryGetKeySubdirectories].count0	= 2 ;
	fMethods[kConfigDirectoryGetKeySubdirectories].count1	= 1 ;
	fMethods[kConfigDirectoryGetKeySubdirectories].flags	= kIOUCScalarIScalarO ;
	
	fMethods[kConfigDirectoryGetType].object				= this ;
	fMethods[kConfigDirectoryGetType].func 				= (IOMethod) & IOFireWireUserClient::configDirectoryGetType ;
	fMethods[kConfigDirectoryGetType].count0				= 1 ;
	fMethods[kConfigDirectoryGetType].count1				= 1 ;
	fMethods[kConfigDirectoryGetType].flags 				= kIOUCScalarIScalarO ;

	fMethods[kConfigDirectoryGetNumEntries].object		= this ;
	fMethods[kConfigDirectoryGetNumEntries].func 			= (IOMethod) & IOFireWireUserClient::configDirectoryGetNumEntries ;
	fMethods[kConfigDirectoryGetNumEntries].count0		= 1 ;
	fMethods[kConfigDirectoryGetNumEntries].count1		= 1 ;
	fMethods[kConfigDirectoryGetNumEntries].flags 		= kIOUCScalarIScalarO ;

	fMethods[kCommand_Release].object						= this ;
	fMethods[kCommand_Release].func						= (IOMethod) & IOFireWireUserClient::userAsyncCommand_Release ;
	fMethods[kCommand_Release].count0						= 1 ;
	fMethods[kCommand_Release].count1						= 0 ;
	fMethods[kCommand_Release].flags						= kIOUCScalarIScalarO ;
	
	fMethods[kCommand_Cancel].object						= this ;
	fMethods[kCommand_Cancel].func						= (IOMethod) & IOFireWireUserClient::userAsyncCommand_Cancel ;
	fMethods[kCommand_Cancel].count0						= 2 ;
	fMethods[kCommand_Cancel].count1						= 0 ;
	fMethods[kCommand_Cancel].flags						= kIOUCScalarIScalarO ;

	fMethods[kSeize].object								= this ;
	fMethods[kSeize].func									= (IOMethod) & IOFireWireUserClient::seize ;
	fMethods[kSeize].count0								= 1 ;
	fMethods[kSeize].count1								= 0 ;
	fMethods[kSeize].flags								= kIOUCScalarIScalarO ;
	
	fMethods[kFireLog].object								= this ;
	fMethods[kFireLog].func									= (IOMethod) & IOFireWireUserClient::firelog ;
	fMethods[kFireLog].count0								= 0xFFFFFFFF ;	// variable size string in
	fMethods[kFireLog].count1								= 0 ;
	fMethods[kFireLog].flags								= kIOUCStructIStructO ;

	//
	// v4
	//
	fMethods[kGetBusGeneration].object	= this ;
	fMethods[kGetBusGeneration].func		= (IOMethod) & IOFireWireUserClient::getBusGeneration ;
	fMethods[kGetBusGeneration].count0 	= 0 ;
	fMethods[kGetBusGeneration].count1 	= 1 ;
	fMethods[kGetBusGeneration].flags		= kIOUCScalarIScalarO ;

	fMethods[kGetLocalNodeIDWithGeneration].object 	= this ;
	fMethods[kGetLocalNodeIDWithGeneration].func		= (IOMethod) & IOFireWireUserClient::getLocalNodeIDWithGeneration ;
	fMethods[kGetLocalNodeIDWithGeneration].count0	= 1 ;
	fMethods[kGetLocalNodeIDWithGeneration].count1	= 1 ;
	fMethods[kGetLocalNodeIDWithGeneration].flags		= kIOUCScalarIScalarO ;

	fMethods[kGetRemoteNodeID].object		= this ;
	fMethods[kGetRemoteNodeID].func			= (IOMethod) & IOFireWireUserClient::getRemoteNodeID ;
	fMethods[kGetRemoteNodeID].count0		= 1 ;
	fMethods[kGetRemoteNodeID].count1		= 1 ;
	fMethods[kGetRemoteNodeID].flags		= kIOUCScalarIScalarO ;

	fMethods[kGetSpeedToNode].object		= this ;
	fMethods[kGetSpeedToNode].func			= (IOMethod) & IOFireWireUserClient::getSpeedToNode ;
	fMethods[kGetSpeedToNode].count0		= 1 ;
	fMethods[kGetSpeedToNode].count1		= 1 ;
	fMethods[kGetSpeedToNode].flags			= kIOUCScalarIScalarO ;

	fMethods[kGetSpeedBetweenNodes].object	= this ;
	fMethods[kGetSpeedBetweenNodes].func	= (IOMethod) & IOFireWireUserClient::getSpeedBetweenNodes ;
	fMethods[kGetSpeedBetweenNodes].count0	= 3 ;
	fMethods[kGetSpeedBetweenNodes].count1	= 1 ;
	fMethods[kGetSpeedBetweenNodes].flags	= kIOUCScalarIScalarO ;

	//
	// v5
	//

	fMethods[kGetIRMNodeID].object		= this ;
	fMethods[kGetIRMNodeID].func		= (IOMethod) & IOFireWireUserClient::getIRMNodeID ;
	fMethods[kGetIRMNodeID].count0		= 1 ;
	fMethods[kGetIRMNodeID].count1		= 1 ;
	fMethods[kGetIRMNodeID].flags		= kIOUCScalarIScalarO ;
}

void
IOFireWireUserClient::initAsyncMethodTable()
{
	fAsyncMethods[kSetAsyncRef_BusReset].object	= this ;
	fAsyncMethods[kSetAsyncRef_BusReset].func	= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_BusReset ;
	fAsyncMethods[kSetAsyncRef_BusReset].count0	= 2 ;
	fAsyncMethods[kSetAsyncRef_BusReset].count1	= 0 ;
	fAsyncMethods[kSetAsyncRef_BusReset].flags	= kIOUCScalarIScalarO ;

	fAsyncMethods[kSetAsyncRef_BusResetDone].object	= this ;
	fAsyncMethods[kSetAsyncRef_BusResetDone].func	= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_BusResetDone ;
	fAsyncMethods[kSetAsyncRef_BusResetDone].count0	= 2 ;
	fAsyncMethods[kSetAsyncRef_BusResetDone].count1	= 0 ;
	fAsyncMethods[kSetAsyncRef_BusResetDone].flags	= kIOUCScalarIScalarO ;

	fAsyncMethods[kSetAsyncRef_Packet].object				= this ;
	fAsyncMethods[kSetAsyncRef_Packet].func				= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_Packet ;
	fAsyncMethods[kSetAsyncRef_Packet].count0				= 3 ;
	fAsyncMethods[kSetAsyncRef_Packet].count1				= 0 ;
	fAsyncMethods[kSetAsyncRef_Packet].flags				= kIOUCScalarIScalarO ;

	fAsyncMethods[kSetAsyncRef_SkippedPacket].object		= this ;
	fAsyncMethods[kSetAsyncRef_SkippedPacket].func		= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_SkippedPacket ;
	fAsyncMethods[kSetAsyncRef_SkippedPacket].count0		= 3 ;
	fAsyncMethods[kSetAsyncRef_SkippedPacket].count1		= 0 ;
	fAsyncMethods[kSetAsyncRef_SkippedPacket].flags		= kIOUCScalarIScalarO ;
		
	fAsyncMethods[kSetAsyncRef_Read].object				= this ;
	fAsyncMethods[kSetAsyncRef_Read].func					= (IOAsyncMethod) & IOFireWireUserClient::setAsyncRef_Read ;
	fAsyncMethods[kSetAsyncRef_Read].count0				= 3 ;
	fAsyncMethods[kSetAsyncRef_Read].count1				= 0 ;
	fAsyncMethods[kSetAsyncRef_Read].flags				= kIOUCScalarIScalarO ;

	fAsyncMethods[kCommand_Submit].object				= this ;
	fAsyncMethods[kCommand_Submit].func				= (IOAsyncMethod) & IOFireWireUserClient::userAsyncCommand_Submit ;
	fAsyncMethods[kCommand_Submit].count0				= 0xFFFFFFFF ;	// variable
	fAsyncMethods[kCommand_Submit].count1				= 0xFFFFFFFF ;
	fAsyncMethods[kCommand_Submit].flags				= kIOUCStructIStructO ;
}



IOExternalMethod* 
IOFireWireUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
	if( index >= kNumMethods )
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
	if( index >= kNumAsyncMethods )
	return NULL;
else
{
	*target = fAsyncMethods[index].object ;
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

#pragma mark -

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
IOFireWireUserClient::firelog( const char* string, IOByteCount bufSize ) const
{
#if FIRELOG
	if ( bufSize > 128 )
		return kIOReturnBadArgument ;
	FireLog( string ) ;
#endif
	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient::getBusGeneration( UInt32* outGeneration )
{
	*outGeneration = fOwner->getController()->getGeneration() ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getLocalNodeIDWithGeneration( UInt32 testGeneration, UInt32* outLocalNodeID )
{
	if (!fOwner->getController()->checkGeneration(testGeneration))
		return kIOFireWireBusReset ;
	
	*outLocalNodeID = (UInt32)fOwner->getController()->getLocalNodeID() ;

	// did generation change when we weren't looking?
	if (!fOwner->getController()->checkGeneration(testGeneration))
		return kIOFireWireBusReset ;

	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getRemoteNodeID( UInt32 testGeneration, UInt32* outRemoteNodeID )
{
	UInt32 generation ;
	UInt16 nodeID ;
	
	IOReturn error = fOwner->getNodeIDGeneration( generation, nodeID ) ;
	if (error)
		return error ;
	if ( generation != testGeneration )
		return kIOFireWireBusReset ;
	
	*outRemoteNodeID = (UInt32)nodeID ;

	// did generation change when we weren't looking?
	if (!fOwner->getController()->checkGeneration(generation))
		return kIOFireWireBusReset ;

	return error ;
}

IOReturn
IOFireWireUserClient::getSpeedToNode( UInt32 generation, UInt32* outSpeed )
{
	if (!fOwner->getController()->checkGeneration(generation))
		return kIOFireWireBusReset ;
	
	UInt16 nodeID ;
	fOwner->getNodeIDGeneration( generation, nodeID ) ;
	
	*outSpeed = (UInt32)fOwner->getController()->FWSpeed(nodeID) ;

	// did generation change when we weren't looking?
	if (!fOwner->getController()->checkGeneration(generation))
		return kIOFireWireBusReset ;

	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getSpeedBetweenNodes( UInt32 generation, UInt32 fromNode, UInt32 toNode, UInt32* outSpeed )
{
	if (!fOwner->getController()->checkGeneration(generation))
		return kIOFireWireBusReset ;	
	*outSpeed = (UInt32)fOwner->getController()->FWSpeed( (UInt16)fromNode, (UInt16)toNode ) ;

	// did generation change when we weren't looking?
	if (!fOwner->getController()->checkGeneration(generation))
		return kIOFireWireBusReset ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getIRMNodeID( UInt32 generation, UInt32* irmNodeID )
{
	UInt16 tempNodeID ;
	UInt32 tempGeneration ;
	
	IOReturn error = (UInt32)fOwner->getController()->getIRMNodeID( tempGeneration, tempNodeID ) ;
	if (error)
		return error ;
		
	if ( tempGeneration != generation )
		return kIOFireWireBusReset ;
	
	*irmNodeID = (UInt32)tempNodeID ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::seize( IOOptionBits inFlags )
{
	if ( ! OSDynamicCast(IOFireWireDevice, fOwner))
		return kIOReturnUnsupported ;

	if ( kIOReturnSuccess != clientHasPrivilege( fTask, kIOClientPrivilegeAdministrator ) )
		return kIOReturnNotPrivileged ;

	// message all clients that have the device open that
	// the device is going away. It's not really going away, but we
	// want them to think so...
	if ( fOwner->isOpen() )
	{
		OSIterator*			clientIterator =  fOwner->getOpenClientIterator() ;
		if (!clientIterator)
		{
			IOFireWireUserClientLog_("IOFireWireUserClient::seize: couldn't make owner client iterator\n") ;
			return kIOReturnError ;
		}	

		{
			IOService*			client = (IOService*)clientIterator->getNextObject() ;
	
			while ( client )
			{
				if ( client != this )
				{
					client->message( kIOFWMessageServiceIsRequestingClose, fOwner ) ;
					client->message( kIOMessageServiceIsRequestingClose, fOwner ) ;

					client->terminate() ;
				}
				
				client = (IOService*)clientIterator->getNextObject() ;
			}
		}

		clientIterator->release() ;
	}
	
	if ( fOwner->isOpen() )
	{
		OSIterator*			clientIterator =  fOwner->getClientIterator() ;
		if (!clientIterator)
		{
			IOFireWireUserClientLog_("IOFireWireUserClient::seize: couldn't make owner client iterator\n") ;
			return kIOReturnError ;
		}	

		{
			IOService*			client = (IOService*)clientIterator->getNextObject() ;
	
			while ( client )
			{
				if ( client != this )
				{
					client->message( kIOFWMessageServiceIsRequestingClose, fOwner ) ;
					client->message( kIOMessageServiceIsRequestingClose, fOwner ) ;

					client->terminate() ;
				}
				
				client = (IOService*)clientIterator->getNextObject() ;
			}
		}

		clientIterator->release() ;
	}

	return kIOReturnSuccess ;
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

#pragma mark -

IOReturn
IOFireWireUserClient::readQuad( const ReadQuadParams* inParams, UInt32* outVal )
{
	IOReturn 				err ;
	IOFWReadQuadCommand*	cmd ;

	if ( inParams->isAbs )
		cmd = this->createReadQuadCommand( inParams->generation, inParams->addr, outVal, 1, NULL, NULL ) ;
	else
	{
		if ( cmd = fOwner->createReadQuadCommand( inParams->addr, outVal, 1, NULL, NULL, inParams->failOnReset ) )
			cmd->setGeneration( inParams->generation ) ;
	}
			
	if(!cmd)
		return kIOReturnNoMemory;

	err = cmd->submit();        // We block here until the command finishes

	if( !err )
		err = cmd->getStatus();

	cmd->release();

	return err;
}

IOReturn
IOFireWireUserClient::read( const ReadParams* inParams, IOByteCount* outBytesTransferred )
{
	IOReturn 					err ;
	IOMemoryDescriptor *		mem ;
	IOFWReadCommand*			cmd ;

	*outBytesTransferred = 0 ;

	mem = IOMemoryDescriptor::withAddress((vm_address_t)inParams->buf, (IOByteCount)inParams->size, kIODirectionIn, fTask);
	if(!mem)
		return kIOReturnNoMemory;

	if ( inParams->isAbs )
		cmd = this->createReadCommand( inParams->generation, inParams->addr, mem, NULL, NULL ) ;
	else
	{
		if ( cmd = fOwner->createReadCommand( inParams->addr, mem, NULL, NULL, inParams->failOnReset ) )
			cmd->setGeneration(inParams->generation) ;
	}
	
	if(!cmd)
	{
		mem->release() ;
		return kIOReturnNoMemory;
	}
	
	err = mem->prepare() ;

	if ( err )
		IOLog("%s %u: IOFireWireUserClient::read: prepare failed\n", __FILE__, __LINE__) ;
	else		
	{
		err = cmd->submit();	// We block here until the command finishes
		mem->complete() ;
	}
	
	if( !err )
		err = cmd->getStatus();
		
	*outBytesTransferred = cmd->getBytesTransferred() ;

	cmd->release();
	mem->release();

	return err;
}

IOReturn
IOFireWireUserClient::writeQuad( const WriteQuadParams* inParams)
{
	IOReturn 				err;
	IOFWWriteQuadCommand*	cmd ;

	if ( inParams->isAbs )
		cmd = this->createWriteQuadCommand( inParams->generation, inParams->addr, & (UInt32)inParams->val, 1, NULL, NULL ) ;
	else
	{
		cmd = fOwner->createWriteQuadCommand( inParams->addr, & (UInt32)inParams->val, 1, NULL, NULL, inParams->failOnReset ) ;
		cmd->setGeneration( inParams->generation ) ;
	}
	
	if(!cmd)
		return kIOReturnNoMemory;


	err = cmd->submit();	// We block here until the command finishes

	if( err )
		err = cmd->getStatus();
	
	cmd->release();

	return err;
}

IOReturn
IOFireWireUserClient::write( const WriteParams* inParams, IOByteCount* outBytesTransferred )
{
	IOMemoryDescriptor *	mem ;
	IOFWWriteCommand*		cmd ;

	*outBytesTransferred = 0 ;

	mem = IOMemoryDescriptor::withAddress((vm_address_t) inParams->buf, inParams->size, kIODirectionOut, fTask);
	if(!mem)
		return kIOReturnNoMemory;

	if ( inParams->isAbs )
		cmd = this->createWriteCommand( inParams->generation, inParams->addr, mem, NULL, NULL ) ;
	else
	{
		if ( cmd = fOwner->createWriteCommand( inParams->addr, mem, NULL, NULL, inParams->failOnReset ) )
			cmd->setGeneration( inParams->generation ) ;
	}
	
	if(!cmd) {
		mem->release() ;
		return kIOReturnNoMemory;
	}

	IOReturn 				err ;
	err = cmd->submit();

	// We block here until the command finishes
	if( !err )
		err = cmd->getStatus();

	*outBytesTransferred = cmd->getBytesTransferred() ;
	
	cmd->release();
	mem->release();

	return err ;
}

IOReturn
IOFireWireUserClient::compareSwap( const CompareSwapParams* inParams, UInt64* oldVal )
{
	IOReturn 							err ;
	IOFWCompareAndSwapCommand*			cmd ;

	if ( inParams->size > 2 )
		return kIOReturnBadArgument ;

	if ( inParams->isAbs )
	{
		cmd = this->createCompareAndSwapCommand( inParams->generation, inParams->addr, (UInt32*)& inParams->cmpVal, 
				(UInt32*)& inParams->swapVal, inParams->size, NULL, NULL ) ;
	}
	else
	{
		if ( cmd = fOwner->createCompareAndSwapCommand( inParams->addr, (UInt32*)& inParams->cmpVal, (UInt32*)& inParams->swapVal, 
				inParams->size, NULL, NULL, inParams->failOnReset ) )
		{
			cmd->setGeneration( inParams->generation ) ;
		}
	}
	
	if(!cmd)
		return kIOReturnNoMemory;

	err = cmd->submit();

	// We block here until the command finishes
	if( !err )
	{
		cmd->locked((UInt32*)oldVal) ;
		err = cmd->getStatus();
//		if(kIOReturnSuccess == err && !cmd->locked((UInt32*)oldVal))
//			err = kIOReturnCannotLock;
	}

	cmd->release();

	return err ;
}

IOReturn IOFireWireUserClient::busReset()
{
	if ( fUnsafeResets )
		return fOwner->getController()->getLink()->resetBus();

	return fOwner->getController()->resetBus();
}

IOReturn
IOFireWireUserClient::getGenerationAndNodeID(
	UInt32*					outGeneration,
	UInt32*					outNodeID) const
{
	UInt16	nodeID ;

	fOwner->getNodeIDGeneration(*outGeneration, nodeID);
	if (!fOwner->getController()->checkGeneration(*outGeneration))
		return kIOReturnNotFound ;	// nodeID we got was stale...
	
	*outNodeID = (UInt32)nodeID ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getLocalNodeID(
	UInt32*					outLocalNodeID) const
{
	*outLocalNodeID = (UInt32)(fOwner->getController()->getLocalNodeID()) ;	
	return kIOReturnSuccess ;
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
#pragma mark --- user client/DCL ----------

IOReturn
IOFireWireUserClient::getOSStringData( KernOSStringRef inStringRef, UInt32 inStringLen, char* inStringBuffer,
	UInt32* outStringLen )
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
	inStringRef->release() ;
	
	return kIOReturnSuccess;
}

IOReturn
IOFireWireUserClient::getOSDataData(
	KernOSDataRef			inDataRef,
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
	inDataRef->release() ;
	
	return result ;
}

#pragma mark -
//	Config Directory methods

IOReturn
IOFireWireUserClient::unitDirCreate(
	KernUnitDirRef*	outDir)
{
	KernUnitDirRef newUnitDir = IOLocalConfigDirectory::create() ;

	IOFireWireUserClientLogIfNil_(*outDir, ("IOFireWireUserClient::UnitDirCreate: IOLocalConfigDirectory::create returned NULL\n")) ;
	if (!newUnitDir)
		return kIOReturnNoMemory ;
		
	addObjectToSet(newUnitDir, fUserUnitDirectories) ;
	*outDir = newUnitDir ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::unitDirRelease(
	KernUnitDirRef	inDir)
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
	KernUnitDirRef		inDir, 
	int 					key,
	char*					buffer,
	UInt32					kr_size)
{
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
	KernUnitDirRef		inDir,
	int						key,
	UInt32					value)
{
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
//
// Address Space methods
//

IOReturn
IOFireWireUserClient::allocateAddressSpace(
	AddressSpaceCreateParams*	inParams,
	KernAddrSpaceRef* 		outKernAddrSpaceRef)
{
	IOReturn						err						= kIOReturnSuccess ;	
	IOFWUserPseudoAddressSpace* 	newAddrSpace			= new IOFWUserPseudoAddressSpace;

	if ( !newAddrSpace )
		return kIOReturnNoMemory ;

	if ( inParams->isInitialUnits )
	{
		if ( !newAddrSpace->initFixed( this, inParams ) )
			err = kIOReturnError ;
	}
	else
	{
		if ( !newAddrSpace->initPseudo( this, inParams ) )
			err = kIOReturnError ;
	}
	
	if ( !err )
		err = newAddrSpace->activate() ;
	
	if ( !err )
	{
		err = addObjectToSet( newAddrSpace, fUserPseudoAddrSpaces) ;
		
		if ( err )
			newAddrSpace->deactivate() ;
	}

	if ( err )
	{
		newAddrSpace->release() ;
		newAddrSpace = NULL ;
	}

#if IOFIREWIREUSERCLIENTDEBUG > 0
	if ( newAddrSpace )
		fStatistics->pseudoAddressSpaces->setObject( newAddrSpace ) ;
#endif

	*outKernAddrSpaceRef = newAddrSpace ;

	return err ;
}

IOReturn
IOFireWireUserClient::releaseAddressSpace(
	KernAddrSpaceRef		inAddrSpace)
{
	IOReturn						result = kIOReturnSuccess ;

	if ( !OSDynamicCast( IOFWUserPseudoAddressSpace, inAddrSpace ) )
		result = kIOReturnBadArgument ;

	inAddrSpace->deactivate() ;
	removeObjectFromSet(inAddrSpace, fUserPseudoAddrSpaces) ;
	
#if IOFIREWIREUSERCLIENTDEBUG > 0
	fStatistics->pseudoAddressSpaces->removeObject( inAddrSpace ) ;
#	endif

	return result ;
}

IOReturn
IOFireWireUserClient::getPseudoAddressSpaceInfo(
	KernAddrSpaceRef				inAddrSpaceRef,
	UInt32*							outNodeID,
	UInt32*							outAddressHi,
	UInt32*							outAddressLo)
{
	IOReturn						result 	= kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace*		me 		= OSDynamicCast(IOFWUserPseudoAddressSpace, inAddrSpaceRef) ;

	if (!me)
		result = kIOReturnBadArgument ;

	if (kIOReturnSuccess == result)
	{		
		*outNodeID 		= me->getBase().nodeID ;
		*outAddressHi	= me->getBase().addressHi ;
		*outAddressLo	= me->getBase().addressLo ;
	}

	return result ;
}

IOReturn
IOFireWireUserClient::setAsyncRef_Packet(
	OSAsyncReference		asyncRef,
	KernAddrSpaceRef		inAddrSpaceRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace*	me		= OSDynamicCast(IOFWUserPseudoAddressSpace, (IOFWUserPseudoAddressSpace*) inAddrSpaceRef) ;

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
	KernAddrSpaceRef		inAddrSpaceRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace*	me		= OSDynamicCast(IOFWUserPseudoAddressSpace, (IOFWUserPseudoAddressSpace*) inAddrSpaceRef) ;

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
	KernAddrSpaceRef		inAddrSpaceRef,
	void*					inCallback,
	void*					inUserRefCon,
	void*,
	void*,
	void*)
{
	IOReturn						result	= kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace*	me		= OSDynamicCast(IOFWUserPseudoAddressSpace, (IOFWUserPseudoAddressSpace*) inAddrSpaceRef) ;

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
	IOUserClient::setAsyncReference(inAsyncRef, (mach_port_t) inAsyncRef[0], inCallback, inRefCon) ;

	bcopy(inAsyncRef, fBusResetDoneAsyncNotificationRef, sizeof(OSAsyncReference)) ;

	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::clientCommandIsComplete(
	KernAddrSpaceRef		inAddrSpaceRef,
	FWClientCommandID		inCommandID,
	IOReturn				inResult)
{
	IOReturn	result = kIOReturnSuccess ;
	IOFWUserPseudoAddressSpace*	me	= OSDynamicCast(IOFWUserPseudoAddressSpace, inAddrSpaceRef) ;

	if (!me)
		result = kIOReturnBadArgument ;

	if (kIOReturnSuccess == result)
		me->clientCommandIsComplete(inCommandID, inResult) ;

	return result ;
}


#pragma mark
#pragma mark --config directories
IOReturn
IOFireWireUserClient::configDirectoryCreate( KernConfigDirectoryRef * outDirRef )
{
	if ( OSDynamicCast( IOFireWireLocalNode, fOwner ) )
		return kIOReturnUnsupported ;

	IOReturn error = fOwner->getConfigDirectoryRef(*outDirRef);

	if (not error && *outDirRef)
		addObjectToSet( *outDirRef, fUserRemoteConfigDirectories ) ;
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectoryRelease(
	KernConfigDirectoryRef	inDirRef)
{
	if ( !OSDynamicCast( IOConfigDirectory, inDirRef ) )
		return kIOReturnBadArgument ;
		
	removeObjectFromSet( inDirRef, fUserRemoteConfigDirectories ) ;
	
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::configDirectoryUpdate(
	KernConfigDirectoryRef 	dirRef, 
	UInt32 						offset, 
	const UInt32*&				romBase)
{
	return kIOReturnUnsupported ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyType(
	KernConfigDirectoryRef	inDirRef,
	int							key,
	IOConfigKeyType*			outType)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getKeyType(key, *outType) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyValue_UInt32( KernConfigDirectoryRef inDirRef, int key,
	UInt32 wantText, UInt32* outValue, KernOSStringRef* outString, UInt32* outStringLen)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getKeyValue(key, *outValue, ((bool)wantText) ? outString : NULL ) ;

	if ( wantText && (*outString) && (kIOReturnSuccess == result) )
		*outStringLen = (*outString)->getLength() ;
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyValue_Data( KernConfigDirectoryRef inDirRef, int key,
	UInt32 wantText, GetKeyValueDataResults* results )
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn error = inDirRef->getKeyValue(key, results->data, ((bool)wantText) ? ( & results->text ) : NULL ) ;
	if ( kIOReturnSuccess == error)
	{
		if (results->data)
			results->dataLength = results->data->getLength() ;
		if (wantText && results->text)
			results->textLength = results->text->getLength() ;
	}
	
	return error ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyValue_ConfigDirectory( KernConfigDirectoryRef inDirRef, int key,
	UInt32 wantText, KernConfigDirectoryRef* outValue, KernOSStringRef* outString, UInt32* outStringLen )
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getKeyValue(key, *outValue, ((bool)wantText) ? outString : NULL ) ;

	if ( wantText && *outString && (kIOReturnSuccess == result) )
		*outStringLen = (*outString)->getLength() ;
	
	if ( kIOReturnSuccess == result )
	{
		addObjectToSet( *outValue, fUserRemoteConfigDirectories ) ;
	}
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeyOffset_FWAddress( KernConfigDirectoryRef inDirRef, int key, UInt32 wantText,
	GetKeyOffsetResults* results)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	FWAddress tempAddress ;
	IOReturn error = inDirRef->getKeyOffset(key, tempAddress, ((bool)wantText) ? & results->text : NULL) ;

	if (kIOReturnSuccess == error)
	{
		results->address = tempAddress ;
		if ( wantText && results->text )
			results->length = results->text->getLength() ;
	}

	return error ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexType(
	KernConfigDirectoryRef	inDirRef,
	int							index,
	IOConfigKeyType*			outType)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexType(index, *outType) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexKey(
	KernConfigDirectoryRef	inDirRef,
	int							index,
	int*						outKey)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexKey(index, *outKey) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexValue_UInt32(
	KernConfigDirectoryRef	inDirRef,
	int							index,
	UInt32*						outKey)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexValue(index, *outKey) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexValue_Data(
	KernConfigDirectoryRef	inDirRef,
	int							index,
	KernOSDataRef*			outDataRef,
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
IOFireWireUserClient::configDirectoryGetIndexValue_String( KernConfigDirectoryRef inDirRef, int index,
	KernOSStringRef* outString, UInt32* outStringLen )
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
	KernConfigDirectoryRef	inDirRef,
	int							index,
	KernConfigDirectoryRef*	outDirRef)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	IOReturn result = inDirRef->getIndexValue(index, *outDirRef) ;
	if ( kIOReturnSuccess == result && *outDirRef )
	{
		addObjectToSet( *outDirRef, fUserRemoteConfigDirectories ) ;
	}
	
	return result ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexOffset_FWAddress(
	KernConfigDirectoryRef	inDirRef,
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
	KernConfigDirectoryRef	inDirRef,
	int							index,
	UInt32*						outValue)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexOffset(index, *outValue) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetIndexEntry(
	KernConfigDirectoryRef	inDirRef,
	int							index,
	UInt32*						outValue)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getIndexEntry(index, *outValue) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetSubdirectories(
	KernConfigDirectoryRef	inDirRef,
	OSIterator**				outIterator)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getSubdirectories(*outIterator) ;		
}

IOReturn
IOFireWireUserClient::configDirectoryGetKeySubdirectories(
	KernConfigDirectoryRef	inDirRef,
	int							key,
	OSIterator**				outIterator)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;
	
	return inDirRef->getKeySubdirectories(key, *outIterator) ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetType(
	KernConfigDirectoryRef	inDirRef,
	int*						outType)
{
	if (!OSDynamicCast(IOConfigDirectory, inDirRef))
		return kIOReturnBadArgument ;

	*outType = inDirRef->getType() ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::configDirectoryGetNumEntries(
	KernConfigDirectoryRef	inDirRef,
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
IOFireWireUserClient::allocatePhysicalAddressSpace( PhysicalAddressSpaceCreateParams* inParams, KernPhysicalAddrSpaceRef* outKernAddrSpaceRef)
{
	IOMemoryDescriptor*	mem = IOMemoryDescriptor::withAddress((vm_address_t)inParams->backingStore, inParams->size, kIODirectionNone, fTask) ;
	IOFWUserClientPhysicalAddressSpace*	addrSpace = new IOFWUserClientPhysicalAddressSpace ;
	IOReturn	result = kIOReturnSuccess ;
	
	if (!mem || !addrSpace)
		result = kIOReturnNoMemory ;
	
	if (kIOReturnSuccess == result)
	{
		if (!addrSpace->initWithDesc(fOwner->getController(), mem))
			result = kIOReturnError ;
			
	}
	
	if (kIOReturnSuccess == result)
	{
		result = addrSpace->activate() ;
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
		*(void**) outKernAddrSpaceRef = (void*) addrSpace ;
	}
	
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
	KernPhysicalAddrSpaceRef			inAddrSpace,
	UInt32*								outCount)
{
	if (!OSDynamicCast(IOFWUserClientPhysicalAddressSpace, inAddrSpace))
		return kIOReturnBadArgument ;

	*outCount = inAddrSpace->getSegmentCount() ;
	return kIOReturnSuccess ;
}

IOReturn
IOFireWireUserClient::getPhysicalAddressSpaceSegments(
	KernPhysicalAddrSpaceRef			inAddrSpace,
	UInt32								inSegmentCount,
	IOPhysicalAddress*					outSegments,
	IOByteCount*						outSegmentLengths,
	UInt32*								outSegmentCount)
{
	IOFWUserClientPhysicalAddressSpace* addrSpace = OSDynamicCast(IOFWUserClientPhysicalAddressSpace, inAddrSpace) ;
	IOReturn	result = kIOReturnSuccess ;
	
	if (!addrSpace)
		result = kIOReturnBadArgument ;
		
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
			
			if (kIOReturnSuccess == result)
			{
				UInt32 bytes = 0 ;
				result = outSegmentsMem->prepare(kIODirectionOut) ;

				if (kIOReturnSuccess == result)
				{
					bytes = outSegmentsMem->writeBytes(0, segments, segmentCount*sizeof(IOPhysicalAddress)) ;

				}
				
				if (kIOReturnSuccess == result)
					result = outSegmentsMem->complete(kIODirectionOut) ;
				
				if (kIOReturnSuccess == result)
					result = outSegmentLengthsMem->prepare(kIODirectionOut) ;
				
				if (kIOReturnSuccess == result)
					bytes = outSegmentLengthsMem->writeBytes(0, segmentLengths, segmentCount*sizeof(IOByteCount)) ;
				
				if (kIOReturnSuccess == result)
					result = outSegmentLengthsMem->complete(kIODirectionOut) ;
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
IOFireWireUserClient::lazyAllocateUserCommand( CommandSubmitParams*	inParams, IOFWUserCommand** outCommand )
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
	CommandSubmitParams*	inParams,
	CommandSubmitResult*	outResult,
	IOByteCount					inParamsSize,
	IOByteCount*				outResultSize)
{
	IOReturn			result	= kIOReturnSuccess ;
	IOFWUserCommand*	me		= inParams->kernCommandRef ;
	
	if (me)
	{
		IOFireWireUserClientLog_("using existing command object\n") ;

		if (!OSDynamicCast(IOFWUserCommand, me))
			return kIOReturnBadArgument ;
	}
	else
	{
		IOFireWireUserClientLog_("lazy allocating command object\n") ;
	
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
IOFireWireUserClient::userAsyncCommand_Release( KernCommandRef inCommandRef )
{
	IOReturn		result = kIOReturnSuccess ;

	if (!OSDynamicCast(IOFWUserCommand, inCommandRef))
		result = kIOReturnBadArgument ;

	removeObjectFromSet(inCommandRef, fUserCommandObjects) ;
	
	return result ;
}

//
// --- absolute address firewire commands ----------
//
IOFWReadCommand*
IOFireWireUserClient::createReadCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress, 
	IOMemoryDescriptor*		hostMem,
	FWDeviceCallback	 		completion,
	void*					refcon ) const
{
	IOFWReadCommand* result = new IOFWReadCommand ;
	if ( result && !result->initAll( fOwner->getController(), generation, devAddress, hostMem, completion, refcon ) )
	{
		result->release() ;
		result = NULL ;
	}
	
	return result ;
}

IOFWReadQuadCommand*
IOFireWireUserClient::createReadQuadCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress, 
	UInt32 *				quads, 
	int 					numQuads,
	FWDeviceCallback 		completion,
	void *					refcon ) const
{
	IOFWReadQuadCommand* result = new IOFWReadQuadCommand ;
	if ( result && !result->initAll( fOwner->getController(), generation, devAddress, quads, numQuads, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

IOFWWriteCommand*
IOFireWireUserClient::createWriteCommand(
	UInt32 					generation, 
	FWAddress 				devAddress, 
	IOMemoryDescriptor*		hostMem,
	FWDeviceCallback 		completion,
	void*					refcon ) const
{
	IOFWWriteCommand* result = new IOFWWriteCommand ;
	if ( result && !result->initAll( fOwner->getController(), generation, devAddress, hostMem, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

IOFWWriteQuadCommand*
IOFireWireUserClient::createWriteQuadCommand(
	UInt32 					generation, 
	FWAddress 				devAddress, 
	UInt32 *				quads, 
	int 					numQuads,
	FWDeviceCallback 		completion,
	void *					refcon ) const
{
	IOFWWriteQuadCommand* result = new IOFWWriteQuadCommand ;
	if ( result && !result->initAll( fOwner->getController(), generation, devAddress, quads, numQuads, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}

	// size is 1 for 32 bit compare, 2 for 64 bit.
IOFWCompareAndSwapCommand*
IOFireWireUserClient::createCompareAndSwapCommand( 
	UInt32 					generation, 
	FWAddress 				devAddress,
	const UInt32 *			cmpVal, 
	const UInt32 *			newVal, 
	int 					size,
	FWDeviceCallback 		completion, 
	void *					refcon ) const
{
	IOFWCompareAndSwapCommand* result = new IOFWCompareAndSwapCommand ;
	if ( result && !result->initAll( fOwner->getController(), generation, devAddress, cmpVal, newVal, size, completion, refcon ) )
	{
		result->release() ;
		return NULL ;
	}
	
	return result ;
}
