/*
 * Copyright (c) 1998-2007 Apple Computer, Inc. All rights reserved.
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

OSDefineMetaClassAndStructors( IOFWAsyncPHYCommand, IOFWCommand )
OSMetaClassDefineReservedUnused( IOFWAsyncPHYCommand, 0 );
OSMetaClassDefineReservedUnused( IOFWAsyncPHYCommand, 1 );
OSMetaClassDefineReservedUnused( IOFWAsyncPHYCommand, 2 );
OSMetaClassDefineReservedUnused( IOFWAsyncPHYCommand, 3 );
OSMetaClassDefineReservedUnused( IOFWAsyncPHYCommand, 4 );
OSMetaClassDefineReservedUnused( IOFWAsyncPHYCommand, 5 );
OSMetaClassDefineReservedUnused( IOFWAsyncPHYCommand, 6 );
OSMetaClassDefineReservedUnused( IOFWAsyncPHYCommand, 7 );

#pragma mark -

// initAll
//
//

bool IOFWAsyncPHYCommand::initAll(
    							IOFireWireController  *	control,
                                UInt32 					generation, 
								UInt32					data1,
								UInt32					data2,
                                FWAsyncPHYCallback		completion,
                                void 				*	refcon,
								bool 					failOnReset )
{
	bool success = true;
	
	success = IOFWCommand::initWithController( control );
	
	if( success )
	{
		fMaxRetries = kFWCmdDefaultRetries;
		fCurRetries = fMaxRetries;
		fTrans = NULL;
		fData1 = data1;
		fData2 = data2;
		fComplete = completion;
		fSync = (completion == NULL);
		fRefCon = refcon;
		fTimeout = 1000*125;	// 1000 frames, 125mSec

		fGeneration = generation;
		fFailOnReset = failOnReset;

		if( !fFailOnReset )
		{
			// latch generation
			fGeneration = fControl->getGeneration();
		}
    }

	return success;
}

// free
//
//

void IOFWAsyncPHYCommand::free()
{	
	IOFWCommand::free();
}


// reinit
//
//

IOReturn IOFWAsyncPHYCommand::reinit(
								UInt32 					generation, 
								UInt32					data1,
								UInt32					data2,
                                FWAsyncPHYCallback		completion,
                                void 				*	refcon,
								bool 					failOnReset )
{
    if( fStatus == kIOReturnBusy || fStatus == kIOFireWirePending )
		return fStatus;

    fComplete = completion;
    fRefCon = refcon;
    fSync = (completion == NULL);
    fCurRetries = fMaxRetries;
	fTrans = NULL;
	fData1 = data1;
	fData2 = data2;
    
	fGeneration = generation;
	fFailOnReset = failOnReset;
	
	if( !fFailOnReset )
	{
		// latch generation
		fGeneration = fControl->getGeneration();
	}
	 
    return (fStatus = kIOReturnSuccess);
}

// complete
//
//

IOReturn IOFWAsyncPHYCommand::complete(IOReturn status)
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
    if(fTrans) 
	{
        fControl->freeTrans( fTrans );
        fTrans = NULL;
    }
	
    // If we're in the middle of processing a bus reset and
    // the command should be retried after a bus reset, put it on the
    // 'after reset queue'
    // If we aren't still scanning the bus, and we're supposed to retry after bus resets, turn it into device offline 
    if( (completion_status == kIOFireWireBusReset) && !fFailOnReset) 
	{
        if( fControl->scanningBus() )
		{
            setHead( fControl->getAfterResetHandledQ() );
            return fStatus = kIOFireWirePending;	// On a queue waiting to execute
        }
    }
    fStatus = completion_status;
    if( fSync )
	{
        fSyncWakeup->signal( completion_status );
    }
	else if( fComplete )
	{
		(*fComplete)( fRefCon, completion_status, fControl, this );
	}
	
    return completion_status;
}

// setRetries
//
//

void IOFWAsyncPHYCommand::setRetries( int retries ) 
{ 
	fMaxRetries = retries;
	fCurRetries = fMaxRetries;
};

// setAckCode
//
//

void IOFWAsyncPHYCommand::setAckCode( int ack )
{
	fAckCode = ack;
}

// getAckCode
//
//

int IOFWAsyncPHYCommand::getAckCode( void )
{
	return fAckCode;
}

// setResponseCode
//
//

void IOFWAsyncPHYCommand::setResponseCode( UInt32 rcode )
{
	fResponseCode = rcode;
}

// getResponseCode
//
//

UInt32 IOFWAsyncPHYCommand::getResponseCode( void ) const
{
	return fResponseCode;
}

// gotAck
//
//

void IOFWAsyncPHYCommand::gotAck( int ackCode )
{
	setAckCode( ackCode );
	
    if( ackCode == kFWAckComplete ) 
    {
		complete( kIOReturnSuccess );
    }
	else
    {
		complete( kIOReturnTimeout );
	}
}

// gotPacket
//
//

void IOFWAsyncPHYCommand::gotPacket( int rcode  )
{
	setResponseCode( rcode );
	
	if( rcode != kFWResponseComplete ) 
	{
        complete( kIOFireWireResponseBase+rcode );
    }
    else 
	{
        complete( kIOReturnSuccess );
    }
}

// execute
//
//

IOReturn IOFWAsyncPHYCommand::execute()
{
    IOReturn result;
    
    fStatus = kIOReturnBusy;

	// allocate a tLabel even though we don't use it
	// this is a poor man's flow control to keep from flooding the FWIM
	// someday we can rip this out if we implement Family/FWIM queuing
	
	// do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans = fControl->allocTrans( NULL, this );
    if( fTrans ) 
	{
		result = fControl->asyncPHYPacket( fGeneration, fData1, fData2, this);
	}
	else
	{
		//IOLog("IOFWAsyncPHYCommand::execute: Out of tLabels?\n");
        result = kIOFireWireOutOfTLabels;
	}
	
	// complete could release us so protect fStatus with retain and release
	IOReturn status = fStatus;	
    if( result != kIOReturnSuccess )
	{
		retain();
        complete( result );
		status = fStatus;
		release();
	}

	return status;
}
