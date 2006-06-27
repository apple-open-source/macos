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
        me = new IOFireWireIRM;
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
	
	fBroadcastChannelBuffer = kBroadcastChannelInitialValues;
    fBroadcastChannelAddressSpace = IOFWPseudoAddressSpace::simpleRWFixed( fControl, FWAddress(kCSRRegisterSpaceBaseAddressHi, kCSRBroadcastChannel), 
																			 sizeof(fBroadcastChannelBuffer), &fBroadcastChannelBuffer );
	FWPANICASSERT( fBroadcastChannelAddressSpace != NULL );
    
	status = fBroadcastChannelAddressSpace->activate();
	FWPANICASSERT( status == kIOReturnSuccess );

	//
	// create lock command
	//
	
	fLockCmdInUse = false;
	fLockCmd = new IOFWCompareAndSwapCommand;
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
		fOldChannelsAvailable31_0 = 0xffffffff;

		allocateBroadcastChannel();
	}
	else
	{
		FWLOCALKLOG(( "IOFireWireIRM::processBusReset() - clear valid bit in BROADCAST_CHANNEL register\n" ));
		fBroadcastChannelBuffer = kBroadcastChannelInitialValues;
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

	fNewChannelsAvailable31_0 = fOldChannelsAvailable31_0 & ~kChannel31Mask;
	
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
			
			fBroadcastChannelBuffer = kBroadcastChannelInitialValues | kBroadcastChannelValidMask;
		}
	}
}
