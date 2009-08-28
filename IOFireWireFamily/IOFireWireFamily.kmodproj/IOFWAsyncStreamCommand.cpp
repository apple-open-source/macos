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
 
//#define IOASSERT 1	// Set to 1 to activate assert()

// public
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/firewire/IOLocalConfigDirectory.h>

// system
#include <IOKit/assert.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommand.h>

#pragma mark -

OSDefineMetaClassAndStructors(IOFWAsyncStreamCommand, IOFWCommand)
OSMetaClassDefineReservedUnused(IOFWAsyncStreamCommand, 0);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamCommand, 1);

#pragma mark -

// initAll
//
//

bool IOFWAsyncStreamCommand::initAll(
    							IOFireWireController 	* control,
                                UInt32 					generation, 
                                UInt32 					channel,
                                UInt32 					sync,
                                UInt32 					tag,
                                IOMemoryDescriptor 		* hostMem,
                                UInt32					size,
                                int						speed,
                                FWAsyncStreamCallback 	completion,
                                void 					* refcon)
{
	return initAll( control, generation, channel, sync, tag, hostMem, size, speed, completion, refcon, false);
}

bool IOFWAsyncStreamCommand::initAll(
    							IOFireWireController 	* control,
                                UInt32 					generation, 
                                UInt32 					channel,
                                UInt32 					sync,
                                UInt32 					tag,
                                IOMemoryDescriptor 		* hostMem,
                                UInt32					size,
                                int						speed,
                                FWAsyncStreamCallback 	completion,
                                void 					* refcon,
								bool					failOnReset)
{
	bool success = true;
	
	success = IOFWCommand::initWithController(control);
	
	if( success )
	{
		fMaxRetries = kFWCmdDefaultRetries;
		fCurRetries = fMaxRetries;
		fMemDesc = hostMem;
		fComplete = completion;
		fSync = completion == NULL;
		fRefCon = refcon;
		fTimeout = 1000*125;	// 1000 frames, 125mSec
		if(hostMem)
			fSize = hostMem->getLength();

		fGeneration = generation;
		fChannel = channel;
		fSyncBits = sync;
		fTag = tag;
		fSpeed = speed;
		fSize = size;
		fFailOnReset = failOnReset;
    }

	return success;
}


// free
//
//

void IOFWAsyncStreamCommand::free()
{	
	IOFWCommand::free();
}


// reinit
//
//

IOReturn IOFWAsyncStreamCommand::reinit(
								UInt32 					generation, 
                                UInt32 					channel,
                                UInt32 					sync,
                                UInt32 					tag,
                                IOMemoryDescriptor 		* hostMem,
                                UInt32					size,
                                int						speed,
                                FWAsyncStreamCallback 	completion,
                                void 					* refcon)
{
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
	return fStatus;

    fComplete = completion;
    fRefCon = refcon;
    fMemDesc=hostMem;
    if(fMemDesc)
        fSize=fMemDesc->getLength();
    fSync = completion == NULL;
    fCurRetries = fMaxRetries;

    fGeneration = generation;
    fChannel = channel;
    fSyncBits = sync;
    fTag = tag;
    fSpeed = speed;
    fSize = size;
    return fStatus = kIOReturnSuccess;
}

IOReturn IOFWAsyncStreamCommand::reinit(
								UInt32 					generation, 
                                UInt32 					channel,
                                UInt32 					sync,
                                UInt32 					tag,
                                IOMemoryDescriptor 		* hostMem,
                                UInt32					size,
                                int						speed,
                                FWAsyncStreamCallback 	completion,
                                void 					* refcon,
								bool					failOnReset)
{
	if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
		return fStatus;

	fFailOnReset = failOnReset;
		
	return reinit( generation, channel, sync, tag, hostMem, size, speed, completion, refcon );
}

// complete
//
//

IOReturn IOFWAsyncStreamCommand::complete(IOReturn status)
{
	// latch the most recent completion status
	IOFWCommand::fMembers->fCompletionStatus = status; 
	
	if( fStatus == kIOFireWireCompleting )
	{
		// prevent double completion
		return kIOReturnSuccess;
	}
	
	// tell the fwim we're completing
	// this could cause this routine to be reentered, hence the protection above
	
	fStatus = kIOFireWireCompleting;
	fControl->handleAsyncCompletion( this, status );
	
	// we're back - actually complete the command
	IOReturn completion_status = IOFWCommand::fMembers->fCompletionStatus;

    removeFromQ();	// Remove from current queue

    // If we're in the middle of processing a bus reset and
    // the command should be retried after a bus reset, put it on the
    // 'after reset queue'
    // If we aren't still scanning the bus, and we're supposed to retry after bus resets, turn it into device offline 
    if( (completion_status == kIOFireWireBusReset) && !fFailOnReset) 
	{
        if(fControl->scanningBus()) 
		{
            setHead(fControl->getAfterResetHandledQ());
            return fStatus = kIOFireWirePending;	// On a queue waiting to execute
        }
    }
    fStatus = completion_status;
    if(fSync)
        fSyncWakeup->signal(completion_status);
    else if(fComplete)
		(*fComplete)(fRefCon, completion_status, fControl, this);

    return completion_status;
}

// gotAck
//
//

void IOFWAsyncStreamCommand::gotAck(int ackCode)
{
    if (ackCode == kFWAckComplete ) 
    	complete( kIOReturnSuccess );
    else
    	complete( kIOReturnTimeout );
}

// execute
//
//

IOReturn IOFWAsyncStreamCommand::execute()
{
    IOReturn result = kIOReturnBadArgument;
    
    fStatus = kIOReturnBusy;

    if( !fFailOnReset ) 
	{
        // Update generation
        fGeneration = fControl->getGeneration();
    }

	fSpeed = min((int)fControl->getBroadcastSpeed(), fSpeed) ;

	if( fSize < ( 1 << 9+fControl->getBroadcastSpeed() ) and ( fChannel >= 0 and fChannel < 64) )
	{
		result = fControl->asyncStreamWrite(fGeneration,
											fSpeed, fTag, fSyncBits, fChannel,fMemDesc,0,fSize, this);
	}

	// complete could release us so protect fStatus with retain and release
	IOReturn status = fStatus;	
    if(result != kIOReturnSuccess)
	{
		retain();
        complete(result);
		status = fStatus;
		release();
	}
	return status;
}
