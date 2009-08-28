/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 * IOFireWireIRM
 *
 * HISTORY
 *
 */

// public
#include <IOKit/firewire/IOFireWireFamilyCommon.h>

// private
#include "IOFireWireIRM.h"

//#define FWLOCALLOGGING 1 
#include "FWDebugging.h"

#define kChannel31Mask 0x00000001

OSDefineMetaClassAndStructors( IOFireWireIRM, OSObject )

// create
//
//

IOFireWireIRM * IOFireWireIRM::create( IOFireWireController * controller )
{
    IOReturn 		status = kIOReturnSuccess;
    IOFireWireIRM * me;
        
    if( status == kIOReturnSuccess )
    {
        me = OSTypeAlloc( IOFireWireIRM );
        if( me == NULL )
            status = kIOReturnNoMemory;
    }
    
    if( status == kIOReturnSuccess )
    {
        bool success = me->initWithController( controller );
		if( !success )
		{
			status = kIOReturnError;
		}
    }
    
    if( status != kIOReturnSuccess )
    {
        me = NULL;
    }

	FWLOCALKLOG(( "IOFireWireIRM::create() - created new IRM 0x%08lx\n", (UInt32)me ));
    
    return me;
}

// initWithController
//
//

bool IOFireWireIRM::initWithController(IOFireWireController * control)
{
	IOReturn status = kIOReturnSuccess;
	
	bool success = OSObject::init();
	FWPANICASSERT( success == true );
	
	fControl = control;

	fIRMNodeID = kFWBadNodeID;
	fOurNodeID = kFWBadNodeID;
	fGeneration = 0;
	
	//
    // create BROADCAST_CHANNEL register
	//
	
	fBroadcastChannelBuffer = OSSwapHostToBigInt32( kBroadcastChannelInitialValues );
    fBroadcastChannelAddressSpace = IOFWPseudoAddressSpace::simpleRWFixed( fControl, FWAddress(kCSRRegisterSpaceBaseAddressHi, kCSRBroadcastChannel), 
																			 sizeof(fBroadcastChannelBuffer), &fBroadcastChannelBuffer );
	FWPANICASSERT( fBroadcastChannelAddressSpace != NULL );
    
	status = fBroadcastChannelAddressSpace->activate();
	FWPANICASSERT( status == kIOReturnSuccess );

	//
	// create lock command
	//
	
	fLockCmdInUse = false;
	fLockCmd = OSTypeAlloc( IOFWCompareAndSwapCommand );
	FWPANICASSERT( fLockCmd != NULL );
	fLockCmd->initAll( fControl, 0, FWAddress(), NULL, NULL, 0, IOFireWireIRM::lockCompleteStatic, this );

	FWLOCALKLOG(( "IOFireWireIRM::initWithController() - IRM intialized\n" ));

	return true;
}

// free
//
//

void IOFireWireIRM::free()
{	
	FWLOCALKLOG(( "IOFireWireIRM::free() - freeing IRM 0x%08lx\n", (UInt32)this ));

	//
	// free lock command
	//
	
	if( fLockCmd != NULL )
	{
		// cancel the command if its in use.
		if( fLockCmdInUse )
		{
			fLockCmd->cancel( kIOFireWireBusReset );
		}

        fLockCmd->release();
		fLockCmd = NULL;
	}
	
	//
	// free BROADCAST_CHANNEL register
	//
		
	if( fBroadcastChannelAddressSpace != NULL) 
	{
		fBroadcastChannelAddressSpace->deactivate();
        fBroadcastChannelAddressSpace->release();
        fBroadcastChannelAddressSpace = NULL;
    }
	
	OSObject::free();
}

// isIRMActive
//
//

bool IOFireWireIRM::isIRMActive( void )
{
	// this irm should be active if we're the IRM node and we're 
	// not the only node on the bus
	
	return (fOurNodeID == fIRMNodeID && (fOurNodeID & 0x3f) != 0);
}

// processBusReset
//
//

void IOFireWireIRM::processBusReset( UInt16 ourNodeID, UInt16 irmNodeID, UInt32 generation )
{
	FWLOCALKLOG(( "IOFireWireIRM::processBusReset() - bus reset occurred\n" ));

	FWLOCALKLOG(( "IOFireWireIRM::processBusReset() - ourNodeID = 0x%04x, irmNodeID = 0x%04x, generation = %d\n", ourNodeID, irmNodeID, generation ));

	// node id's and generation

	fIRMNodeID = irmNodeID;
	fOurNodeID = ourNodeID;
	fGeneration = generation;
	
	if( isIRMActive() )
	{
	
		// stop any command in progress. any inflight commands should already 
		// have been canceled by the bus reset before this point.  
		// this is just an extra precaution
		
		// lockComplete will run with status = kIOFireWireBusReset before we
		// return from cancel.  fLockCmdInUse is cleared by lockComplete.
		
		// calling cancel on commands that are not busy will still call
		// complete, so we must make sure a command is in use before cancelling it.
		
		// commands do not have a reliable API for tracking usage, so we
		// use fLockCmdInUse instead
		
		if( fLockCmdInUse )
		{
			fLockCmd->cancel( kIOFireWireBusReset );
		}
		
		// initialize fOldChannelsAvailable31_0 and fLockRetries
		fLockRetries = 8;
		fOldChannelsAvailable31_0 = OSSwapHostToBigInt32( 0xffffffff );  // don't really need to swap of course

		allocateBroadcastChannel();
	}
	else
	{
		FWLOCALKLOG(( "IOFireWireIRM::processBusReset() - clear valid bit in BROADCAST_CHANNEL register\n" ));
		fBroadcastChannelBuffer = OSSwapHostToBigInt32(kBroadcastChannelInitialValues);
	}
	
}

// allocateBroadcastChannel
//
//

void IOFireWireIRM::allocateBroadcastChannel( void )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLOCALKLOG(( "IOFireWireIRM::allocateBroadcastChannel() - attempting to allocate broadcast channel\n" ));

	FWAddress address( kCSRRegisterSpaceBaseAddressHi, kCSRChannelsAvailable31_0 );
	address.nodeID = fIRMNodeID;

	UInt32 host_channels_available = OSSwapBigToHostInt32( fOldChannelsAvailable31_0 );
	host_channels_available &= ~kChannel31Mask;
	fNewChannelsAvailable31_0 = OSSwapHostToBigInt32( host_channels_available );
	
	fLockCmd->reinit( fGeneration, address, &fOldChannelsAvailable31_0, &fNewChannelsAvailable31_0, 1, IOFireWireIRM::lockCompleteStatic, this );
	
	// the standard async commands call complete with an error before
	// returning an error from submit. 
	
	fLockCmdInUse = true;
	status = fLockCmd->submit();
}

// lockComplete
//
//

void IOFireWireIRM::lockCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
	IOFireWireIRM * me = (IOFireWireIRM*)refcon;
	me->lockComplete( status );
}

void IOFireWireIRM::lockComplete( IOReturn status )
{
	bool done = true;
	
	fLockCmdInUse = false;
	
	if( status == kIOReturnSuccess )
	{
		// update fOldChannelsAvailable31_0 and fLockRetries
		bool tryAgain = !fLockCmd->locked( &fOldChannelsAvailable31_0 );
		if( tryAgain && fLockRetries-- )
		{
			FWLOCALKLOG(( "IOFireWireIRM::lockComplete() - allocation attempt failed, will retry\n" ));
			allocateBroadcastChannel();
			
			done = false;
		}
	}
	
	// done means we did not resubmit this command
	if( done )
	{	
		// if this command was completed because of a bus reset,
		// we will retry the channel allocation in a moment when 
		// processBusReset is called, therefore don't set the
		// channel as valid just yet.
		//
		// otherwise, if no bus reset processing is pending we 
		// pretend we've allocated the channel even if we failed to do so. 

#if FWLOCALLOGGING
		if( status == kIOReturnSuccess )
		{
			FWLOCALKLOG(( "IOFireWireIRM::lockComplete() - successfully allocated broadcast channel\n" ));
		}
		else
		{
			FWLOCALKLOG(( "IOFireWireIRM::lockComplete() - failed to allocate broadcast channel\n" ));
		}
#endif
		
		if( status != kIOFireWireBusReset )
		{
			FWLOCALKLOG(( "IOFireWireIRM::lockComplete() - set valid bit in BROADCAST_CHANNEL register\n" ));
			
			fBroadcastChannelBuffer = OSSwapHostToBigInt32( kBroadcastChannelInitialValues | kBroadcastChannelValidMask );
		}
	}
}

#pragma mark -

OSDefineMetaClassAndStructors( IOFireWireIRMAllocation, OSObject );
OSMetaClassDefineReservedUnused(IOFireWireIRMAllocation, 0);
OSMetaClassDefineReservedUnused(IOFireWireIRMAllocation, 1);
OSMetaClassDefineReservedUnused(IOFireWireIRMAllocation, 2);
OSMetaClassDefineReservedUnused(IOFireWireIRMAllocation, 3);
OSMetaClassDefineReservedUnused(IOFireWireIRMAllocation, 4);
OSMetaClassDefineReservedUnused(IOFireWireIRMAllocation, 5);
OSMetaClassDefineReservedUnused(IOFireWireIRMAllocation, 6);
OSMetaClassDefineReservedUnused(IOFireWireIRMAllocation, 7);

// IRMAllocationThreadInfo
//
// A little struct for keeping track of our this pointer and generation
// when transitioning to a second thread during bandwidth reallocation.

struct IRMAllocationThreadInfo
{
    IOFireWireIRMAllocation * fIRMAllocation;
    UInt32 fGeneration;
	IOFireWireController * fControl;
	IORecursiveLock * fLock;
	UInt8 fIsochChannel;
	UInt32 fBandwidthUnits;
};

// IOFireWireIRMAllocation::init
//
//
bool IOFireWireIRMAllocation::init( IOFireWireController * control,
									Boolean releaseIRMResourcesOnFree, 
									AllocationLostNotificationProc allocationLostProc,
									void *pLostProcRefCon)
{
	if (!OSObject::init())
		return false ;
	
	// Allocate a lock
	fLock = IORecursiveLockAlloc () ;
	if ( ! fLock )
		return false ;
	
	// Initialize some class members
	fControl = control;
	fAllocationGeneration = 0xFFFFFFFF;
	fAllocationLostProc = allocationLostProc;
	fLostProcRefCon = pLostProcRefCon;
	fReleaseIRMResourcesOnFree = releaseIRMResourcesOnFree;
	fBandwidthUnits = 0;
	fIsochChannel = 64;

	fControl->addToIRMAllocationSet(this);
	
	isAllocated = false;
	return true;
}

// IOFireWireIRMAllocation::release
//
//
void IOFireWireIRMAllocation::release() const
{
	DebugLog( "IOFireWireIRMAllocation::release, retain count before release = %d\n",getRetainCount() ) ;

	// Take the lock
	IORecursiveLockLock(fLock);

	int retainCnt = getRetainCount();
	
	if ( retainCnt == 2 )
	{
		if( isAllocated == false )
		{
			fControl->removeFromIRMAllocationSet((IOFireWireIRMAllocation*)this);
		}
		else
		{
			// The controller has an extra retain on the IOFireWireIRMAllocation object
			// because it's in the array used to restore allocations after a bus-reset.
			// We now need to remove it from the controller's array, so it's no longer
			// auto-restored after bus-reset!
			fControl->removeIRMAllocation((IOFireWireIRMAllocation*)this);
		}
	}

	OSObject::release();

	// Bypass unlock if we just did the last release!
	if (retainCnt != 1)
		IORecursiveLockUnlock(fLock);
}

// IOFireWireIRMAllocation::free
//
//
void IOFireWireIRMAllocation::free( void )
{
	DebugLog( "IOFireWireIRMAllocation::free\n") ;

	// Take the lock
	IORecursiveLockLock(fLock);

	
	// If we need to release the isoch resources, do so now!
	if (isAllocated)
	{
		if (fReleaseIRMResourcesOnFree)
		{
			if (fBandwidthUnits > 0)
				fControl->releaseIRMBandwidthInGeneration(fBandwidthUnits,fAllocationGeneration);
			if (fIsochChannel < 64)
				fControl->releaseIRMChannelInGeneration(fIsochChannel,fAllocationGeneration);
		}
		// Note: we already removed this allocation from the controller's array! Don't need to do it here!
	}
	
	// Free the lock
	if ( fLock )
		IORecursiveLockFree( fLock ) ;

	OSObject::free();
}

// IOFireWireIRMAllocation::allocateIsochResources
//
//
IOReturn IOFireWireIRMAllocation::allocateIsochResources(UInt8 isochChannel, UInt32 bandwidthUnits)
{
	IOReturn res = kIOReturnError;
	UInt32 irmGeneration;
	UInt16 irmNodeID;
	
	// Take the lock
	IORecursiveLockLock(fLock);
	
	if (!isAllocated)
	{
		// Initialize some class members
		fAllocationGeneration = 0xFFFFFFFF;
		
		// Get the current generation
		fControl->getIRMNodeID(irmGeneration, irmNodeID);
		
		res = kIOReturnSuccess;
		
		if (isochChannel < 64)
		{
			// Attempt to allocate isoch channel
			res = fControl->allocateIRMChannelInGeneration(isochChannel,irmGeneration);
		}
		
		if ((res == kIOReturnSuccess) && (bandwidthUnits > 0))
		{
			// Attempt to allocate isoch bandwidth
			res = fControl->allocateIRMBandwidthInGeneration(bandwidthUnits,irmGeneration);
			if (res != kIOReturnSuccess) 
			{
				// Need to free the isoch channel (note: will fail if generation has changed)
				fControl->releaseIRMChannelInGeneration(isochChannel,irmGeneration);
			}
		}
		
		if (res == kIOReturnSuccess)
		{
			fIsochChannel = isochChannel;
			fBandwidthUnits = bandwidthUnits;
			fAllocationGeneration = irmGeneration;
			isAllocated = true;
			
			// Register this object with the controller
			fControl->addIRMAllocation(this);
		}
	}
	
	// Unlock the lock
	IORecursiveLockUnlock(fLock);
	
	FWTrace( kFWTIsoch, kTPIsochIRMAllocateIsochResources, (uintptr_t)(fControl->getLink()), fIsochChannel, fBandwidthUnits, res );
	
	return res;
}

// IOFireWireIRMAllocation::deallocateIsochResources
//
//
IOReturn IOFireWireIRMAllocation::deallocateIsochResources(void)
{
	IOReturn res = kIOReturnError;

	// Take the lock
	IORecursiveLockLock(fLock);

	if (isAllocated)
	{
		if (fBandwidthUnits > 0)
			fControl->releaseIRMBandwidthInGeneration(fBandwidthUnits,fAllocationGeneration);
		if (fIsochChannel < 64)
			fControl->releaseIRMChannelInGeneration(fIsochChannel,fAllocationGeneration);
	
		// Unregister this object with the controller
		fControl->removeIRMAllocation(this);
		
		isAllocated = false;
		fBandwidthUnits = 0;
		fIsochChannel = 64;
		fAllocationGeneration = 0xFFFFFFFF;
	}
	
	// Unlock the lock
	IORecursiveLockUnlock(fLock);

	return res;
}

// IOFireWireIRMAllocation::areIsochResourcesAllocated
//
//
Boolean IOFireWireIRMAllocation::areIsochResourcesAllocated(UInt8 *pAllocatedIsochChannel, UInt32 *pAllocatedBandwidthUnits)
{

	*pAllocatedIsochChannel = fIsochChannel;
	*pAllocatedBandwidthUnits = fBandwidthUnits;
	return isAllocated;
}

// IOFireWireIRMAllocation::GetRefCon
//
//
void * IOFireWireIRMAllocation::GetRefCon(void)
{
	return fLostProcRefCon;
}

// IOFireWireIRMAllocation::SetRefCon
//
//
void IOFireWireIRMAllocation::SetRefCon(void* refCon) 
{
	fLostProcRefCon = refCon;
}

// IOFireWireIRMAllocation::handleBusReset
//
//
void IOFireWireIRMAllocation::handleBusReset(UInt32 generation)
{
	// Take the lock
	IORecursiveLockLock(fLock);

	if (!isAllocated)
	{
		IORecursiveLockUnlock(fLock);
		return;
	}
	
	if (fAllocationGeneration == generation)
	{
		IORecursiveLockUnlock(fLock);
		return;
	}
	
	// Spawn a thread to do the reallocation
	IRMAllocationThreadInfo * threadInfo = (IRMAllocationThreadInfo *)IOMalloc( sizeof(IRMAllocationThreadInfo) );
	if( threadInfo ) 
	{
		threadInfo->fGeneration = generation;
		threadInfo->fIRMAllocation = this;
		threadInfo->fControl = fControl;
		threadInfo->fLock = fLock;
		threadInfo->fIsochChannel = fIsochChannel; 
		threadInfo->fBandwidthUnits = fBandwidthUnits;

		retain();	// retain ourself for the thread to use

		thread_t		thread;
		if( kernel_thread_start((thread_continue_t)threadFunc, threadInfo, &thread ) == KERN_SUCCESS )
		{
			thread_deallocate(thread);
		}
	}
	
	// Unlock the lock
	IORecursiveLockUnlock(fLock);
}

// IOFireWireIRMAllocation::setReleaseIRMResourcesOnFree
//
//
void IOFireWireIRMAllocation::setReleaseIRMResourcesOnFree(Boolean doRelease)
{
	fReleaseIRMResourcesOnFree = doRelease;
}

// IOFireWireIRMAllocation::getAllocationGeneration
//
//
UInt32 IOFireWireIRMAllocation::getAllocationGeneration(void)
{
	return fAllocationGeneration;
}

// IOFireWireIRMAllocation::failedToRealloc
//
//
void IOFireWireIRMAllocation::failedToRealloc(void)
{
	// Notify client, and mark as to not reallocate in the future!

	if (fAllocationLostProc)
		fAllocationLostProc(fLostProcRefCon,this);

	// Unregister this object with the controller
	fControl->removeIRMAllocation(this);

	isAllocated = false;
	fAllocationGeneration = 0xFFFFFFFF;
}

// IOFireWireIRMAllocation::threadFunc
//
//
void IOFireWireIRMAllocation::threadFunc( void * arg )
{
	IOReturn res = kIOReturnSuccess;
    IRMAllocationThreadInfo * threadInfo = (IRMAllocationThreadInfo *)arg;
    IOFireWireIRMAllocation *pIRMAllocation = threadInfo->fIRMAllocation;
	IORecursiveLock * fLock = threadInfo->fLock;
	UInt32 generation = threadInfo->fGeneration;
	UInt32 irmGeneration;
	UInt16 irmNodeID;
	
	// Take the lock
	IORecursiveLockLock(fLock);

	// Get the current generation
	threadInfo->fControl->getIRMNodeID(irmGeneration, irmNodeID);
	
	if ((irmGeneration == generation) && (pIRMAllocation->getAllocationGeneration() != 0xFFFFFFFF))
	{
		if (threadInfo->fIsochChannel < 64)
		{
			// Attempt to reallocate isoch channel
			res = threadInfo->fControl->allocateIRMChannelInGeneration(threadInfo->fIsochChannel,generation);
		}
		
		if ((res == kIOReturnSuccess) && (threadInfo->fBandwidthUnits > 0))
		{
			// Attempt to reallocate isoch bandwidth
			res = threadInfo->fControl->allocateIRMBandwidthInGeneration(threadInfo->fBandwidthUnits,generation);
			if (res != kIOReturnSuccess) 
			{
				// Need to free the isoch channel (note: will fail if generation has changed)
				threadInfo->fControl->releaseIRMChannelInGeneration(threadInfo->fIsochChannel,generation);
			}
		}

		if ((res != kIOReturnSuccess) && (res != kIOFireWireBusReset))
		{
			// We failed to reallocate (and not due to a bus-reset).
			pIRMAllocation->failedToRealloc();
		}
	}
	
	// Unlock the lock
	IORecursiveLockUnlock(fLock);
	
	// clean up thread info
	IOFree( threadInfo, sizeof(threadInfo) );
    pIRMAllocation->release();		// retain occurred in handleBusReset
	
	FWTrace( kFWTIsoch, kTPIsochIRMThreadFunc, (uintptr_t)(threadInfo->fControl->getLink()), threadInfo->fIsochChannel, threadInfo->fBandwidthUnits, res );
}
