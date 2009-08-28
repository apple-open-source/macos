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

// private
#import "FWDebugging.h"
#include <IOKit/firewire/IOFWUtils.h>

#define kIOFWAsyncCommandMaxExecutionTime		30000	// try to get the command out for up to 30 seconds

#pragma mark -

OSDefineMetaClass( IOFWAsyncCommand, IOFWCommand )
OSDefineAbstractStructors(IOFWAsyncCommand, IOFWCommand)
OSMetaClassDefineReservedUnused(IOFWAsyncCommand, 0);
OSMetaClassDefineReservedUnused(IOFWAsyncCommand, 1);
OSMetaClassDefineReservedUnused(IOFWAsyncCommand, 2);
OSMetaClassDefineReservedUnused(IOFWAsyncCommand, 3);

#pragma mark -

// initWithController
//
//

bool IOFWAsyncCommand::initWithController(IOFireWireController *control)
{
	bool success = true;
	
    success = IOFWCommand::initWithController(control);
    
	if( success && fMembers == NULL )
	{
		// create member variables
		
		success = createMemberVariables();
	}
		
    return success;
}

// initAll
//
//

bool IOFWAsyncCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
	bool success = true;
	
    success = IOFWCommand::initWithController(device->getController());
    
	if( success && fMembers == NULL )
	{
		// create member variables
		
		success = createMemberVariables();
	}
	
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
		fBytesTransferred = 0;
	
		fDevice = device;
		device->getNodeIDGeneration(fGeneration, fNodeID);
		fAddressHi = devAddress.addressHi;
		fAddressLo = devAddress.addressLo;
		fMaxPack = 1 << device->maxPackLog(fWrite, devAddress);
		fSpeed = fControl->FWSpeed(fNodeID);
		if( fMembers->fMaxSpeed < fSpeed )
		{
			fSpeed = fMembers->fMaxSpeed;
		}
		fFailOnReset = failOnReset;
		fMembers->fAckCode = 0;
		fMembers->fResponseCode = 0xff;
		fMembers->fResponseSpeed = 0xff;
	}
		
    return success;
}

// initAll
//
//

bool IOFWAsyncCommand::initAll(IOFireWireController *control,
        UInt32 generation, FWAddress devAddress,
        IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
        void *refcon)
{
	bool success = true;
	
    success = IOFWCommand::initWithController(control);
	
	if( success && fMembers == NULL )
	{
		// create member variables
		
		success = createMemberVariables();
	}
		
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
		fBytesTransferred = 0;
	
		fDevice = NULL;
		fGeneration = generation;
		fNodeID = devAddress.nodeID;
		fAddressHi = devAddress.addressHi;
		fAddressLo = devAddress.addressLo;
		fMaxPack = 1 << fControl->maxPackLog(fWrite, fNodeID);
		fSpeed = fControl->FWSpeed(fNodeID);
		if( fMembers->fMaxSpeed < fSpeed )
		{
			fSpeed = fMembers->fMaxSpeed;
		}
		fFailOnReset = true;
		fMembers->fAckCode = 0;
		fMembers->fResponseCode = 0xff;
		fMembers->fResponseSpeed = 0xff;
	}
	
    return success;
}

// createMemberVariables
//
//

bool IOFWAsyncCommand::createMemberVariables( void )
{
	bool success = true;
	
	if( fMembers == NULL )
	{
		// create member variables
		
		if( success )
		{
			fMembers = (MemberVariables*)IOMalloc( sizeof(MemberVariables) );
			if( fMembers == NULL )
				success = false;
		}
		
		// zero member variables
		
		if( success )
		{
			bzero( fMembers, sizeof(MemberVariables) );
		
			fMembers->fMaxSpeed = kFWSpeedMaximum;
		}
		
		// clean up on failure
		
		if( !success )
		{
			destroyMemberVariables();
		}
	}
	
	return success;
}

// destroyMemberVariables
//
//

void IOFWAsyncCommand::destroyMemberVariables( void )
{
	if( fMembers != NULL )
	{
		IOFree( fMembers, sizeof(MemberVariables) );
		fMembers = NULL;
	}
}

// free
//
//

void IOFWAsyncCommand::free()
{	
	destroyMemberVariables();
	
	IOFWCommand::free();
}

// reinit
//
//

IOReturn IOFWAsyncCommand::reinit(FWAddress devAddress, IOMemoryDescriptor *hostMem,
			FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending || fStatus == kIOFireWireCompleting)
		return fStatus;

    fComplete = completion;
    fRefCon = refcon;
    fMemDesc=hostMem;
    if(fMemDesc)
        fSize=fMemDesc->getLength();
    fBytesTransferred = 0;
    fSync = completion == NULL;
    fTrans = NULL;
    fCurRetries = fMaxRetries;

    if(fDevice) {
        fDevice->getNodeIDGeneration(fGeneration, fNodeID);
        fMaxPack = 1 << fDevice->maxPackLog(fWrite, devAddress);        
    }
    fAddressHi = devAddress.addressHi;
    fAddressLo = devAddress.addressLo;
    fSpeed = fControl->FWSpeed(fNodeID);
	if( fMembers->fMaxSpeed < fSpeed )
	{
		fSpeed = fMembers->fMaxSpeed;
	}
    fFailOnReset = failOnReset;
	fMembers->fAckCode = 0;
	fMembers->fResponseCode = 0xff;
	fMembers->fResponseSpeed = 0xff;
	
    return fStatus = kIOReturnSuccess;
}

// reinit
//
//

IOReturn IOFWAsyncCommand::reinit(UInt32 generation, FWAddress devAddress, IOMemoryDescriptor *hostMem,
                        FWDeviceCallback completion, void *refcon)
{
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending || fStatus == kIOFireWireCompleting)
        return fStatus;
    if(fDevice)
        return kIOReturnBadArgument;
    fComplete = completion;
    fRefCon = refcon;
    fMemDesc=hostMem;
    if(fMemDesc)
        fSize=fMemDesc->getLength();
    fBytesTransferred = 0;
    fSync = completion == NULL;
    fTrans = NULL;
    fCurRetries = fMaxRetries;

    fGeneration = generation;
    fNodeID = devAddress.nodeID;
    fAddressHi = devAddress.addressHi;
    fAddressLo = devAddress.addressLo;
    fMaxPack = 1 << fControl->maxPackLog(fWrite, fNodeID);
    fSpeed = fControl->FWSpeed(fNodeID);
	if( fMembers->fMaxSpeed < fSpeed )
	{
		fSpeed = fMembers->fMaxSpeed;
	}
	fMembers->fAckCode = 0;
	fMembers->fResponseCode = 0xff;
	fMembers->fResponseSpeed = 0xff;

    return fStatus = kIOReturnSuccess;
}

// updateGeneration
//
//

IOReturn IOFWAsyncCommand::updateGeneration()
{
    if(!fDevice)
        return kIOReturnBadArgument;
    fDevice->getNodeIDGeneration(fGeneration, fNodeID);

    // If currently in bus reset state, we're OK now.
    if(fStatus == kIOFireWireBusReset)
        fStatus = kIOReturnSuccess;
    return fStatus;
}

// updateNodeID
//
// explicitly update nodeID/generation after bus reset

IOReturn IOFWAsyncCommand::updateNodeID(UInt32 generation, UInt16 nodeID)
{
    fGeneration = generation;
    fNodeID = nodeID;

    // If currently in bus reset state, we're OK now.
    if(fStatus == kIOFireWireBusReset)
        fStatus = kIOReturnSuccess;
    return fStatus;
}

// checkProgress
//
//

IOReturn IOFWAsyncCommand::checkProgress( void )
{
	IOReturn status = kIOReturnSuccess;		// all is well
	
	AbsoluteTime now;
	UInt32 milliDelta;
	UInt64 nanoDelta;
		
	IOFWGetAbsoluteTime( &now );
	SUB_ABSOLUTETIME( &now, &IOFWCommand::fMembers->fSubmitTime );
	absolutetime_to_nanoseconds( now, &nanoDelta );
	milliDelta = nanoDelta / 1000000;
	
	if( milliDelta > kIOFWAsyncCommandMaxExecutionTime )
	{
		status = kIOReturnTimeout;
	}
	
	return status;
}

// complete
//
//

IOReturn IOFWAsyncCommand::complete(IOReturn status)
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
        fControl->freeTrans(fTrans);
        fTrans = NULL;
    }
	
    // If we're in the middle of processing a bus reset and
    // the command should be retried after a bus reset, put it on the
    // 'after reset queue'
    // If we aren't still scanning the bus, and we're supposed to retry after bus resets, turn it into device offline 
    if( (completion_status == kIOFireWireBusReset) && !fFailOnReset ) 
	{
		// first check if we're not making forward progress with this command
		IOReturn progress_status = checkProgress();
		if( progress_status != kIOReturnSuccess )
		{
			status = progress_status;
		}
		else
		{
			if(fControl->scanningBus()) 
			{
				setHead(fControl->getAfterResetHandledQ());
				return fStatus = kIOFireWirePending;	// On a queue waiting to execute
			}
			else if(fDevice) 
			{
				DebugLog( "FireWire: Command for device %p that's gone away\n", fDevice );
				completion_status = kIOReturnOffline;	// device must have gone.
			}
		}
	}
    else if(completion_status == kIOReturnTimeout) 
	{
        if(fCurRetries--) 
		{
			bool tryAgain = false;
			int ack = getAckCode();
			if( (ack == kFWAckBusyX) || (ack == kFWAckBusyA) || (ack == kFWAckBusyB) )
			{
				tryAgain = true;
			}
			
            if(!tryAgain) 
			{
                // Some devices just don't handle block requests properly.
                // Only retry as Quads for ROM area
                if( fMaxPack > 4 && 
					fAddressHi == kCSRRegisterSpaceBaseAddressHi &&
                    fAddressLo >= kConfigROMBaseAddress && 
					fAddressLo < kConfigROMBaseAddress + 1024) 
				{
                    fMaxPack = 4;
                    tryAgain = true;
                }
                else
                    tryAgain = kIOReturnSuccess == fControl->handleAsyncTimeout(this);
            }
            
			if( fNodeID == 0x4242 )
			{
				// never retry FireBug packets
				tryAgain = false;
			}
                
			if(tryAgain) 
			{
				IOReturn result;
				
				// startExecution() may release this command so retain it
				retain();
				fStatus = startExecution();
				result = fStatus;
				release();
				
				return result;
            }
        }
    }
	
    fStatus = completion_status;
    if(fSync)
        fSyncWakeup->signal(completion_status);
    else if(fComplete)
		(*fComplete)(fRefCon, completion_status, fDevice, this);

    return completion_status;
}

// gotAck
//
//

void IOFWAsyncCommand::gotAck(int ackCode)
{
    int rcode;
    
	setAckCode( ackCode );

	switch( ackCode ) 
	{
		case kFWAckPending:
			// This has been turned on in the FWIM
			//IOLog("Command 0x%p received Ack code %d\n", this, ackCode);
			return;
    
		case kFWAckComplete:
			rcode = kFWResponseComplete;
			break;

		// Device is still busy after several hardware retries
		// Stash away that fact to use when the command times out.
		case kFWAckBusyX:
		case kFWAckBusyA:
		case kFWAckBusyB:
			return;	// Retry after command times out
			
		// Device isn't acking at all
		case kFWAckTimeout:
			return;	// Retry after command times out
	
		default:
			rcode = kFWResponseTypeError;	// Block transfers will try quad now
    }

    gotPacket(rcode, NULL, 0);
}

// setAckCode
//
//

void IOFWAsyncCommand::setAckCode( int ack )
{
	fMembers->fAckCode = ack;
}

// getAckCode
//
//

int IOFWAsyncCommand::getAckCode( void )
{
	return fMembers->fAckCode;
}

// setMaxSpeed
//
//

void IOFWAsyncCommand::setMaxSpeed( int speed ) 
{ 
	fMembers->fMaxSpeed = speed;
	if( fMembers->fMaxSpeed < fSpeed )
	{
		fSpeed = fMembers->fMaxSpeed;
	}
};

// setRetries
//
//

void IOFWAsyncCommand::setRetries( int retries ) 
{ 
	fMaxRetries = retries;
	fCurRetries = fMaxRetries;
};

// getMaxRetries
//
//

int IOFWAsyncCommand::getMaxRetries( void )
{ 
	return fMaxRetries;
};

// setResponseCode
//
//

void IOFWAsyncCommand::setResponseCode( UInt32 rcode )
{
	fMembers->fResponseCode = rcode;
}

// getResponseCode
//
//

UInt32 IOFWAsyncCommand::getResponseCode( void ) const
{
	return fMembers->fResponseCode;
}
