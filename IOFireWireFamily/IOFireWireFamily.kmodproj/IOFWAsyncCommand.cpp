/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommand.h>

#define kDefaultRetries 3

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
		fMaxRetries = kDefaultRetries;
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
		fFailOnReset = failOnReset;
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
		fMaxRetries = kDefaultRetries;
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
		fFailOnReset = true;
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
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
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
    fFailOnReset = failOnReset;
    return fStatus = kIOReturnSuccess;
}

// reinit
//
//

IOReturn IOFWAsyncCommand::reinit(UInt32 generation, FWAddress devAddress, IOMemoryDescriptor *hostMem,
                        FWDeviceCallback completion, void *refcon)
{
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
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

// complete
//
//

IOReturn IOFWAsyncCommand::complete(IOReturn status)
{
    removeFromQ();	// Remove from current queue
    if(fTrans) {
        fControl->freeTrans(fTrans);
        fTrans = NULL;
    }
    // If we're in the middle of processing a bus reset and
    // the command should be retried after a bus reset, put it on the
    // 'after reset queue'
    // If we aren't still scanning the bus, and we're supposed to retry after bus resets, turn it into device offline 
    if((status == kIOFireWireBusReset) && !fFailOnReset) {
        if(fControl->scanningBus()) {
            setHead(fControl->getAfterResetHandledQ());
            return fStatus = kIOFireWirePending;	// On a queue waiting to execute
        }
        else if(fDevice) {
            IOLog("Command for device %p that's gone away\n", fDevice);
            status = kIOReturnOffline;	// device must have gone.
        }
    }
    // First check for retriable error
    if(status == kIOReturnTimeout) {
        if(fCurRetries--) {
            bool tryAgain = kIOFireWireResponseBase+kFWResponseConflictError == fStatus;
            if(!tryAgain) {
                // Some devices just don't handle block requests properly.
                // Only retry as Quads for ROM area
                if(fMaxPack > 4 && fAddressHi == kCSRRegisterSpaceBaseAddressHi &&
                    fAddressLo >= kConfigROMBaseAddress && fAddressLo < kConfigROMBaseAddress + 1024) {
                    fMaxPack = 4;
                    tryAgain = true;
                }
                else
                    tryAgain = kIOReturnSuccess == fControl->handleAsyncTimeout(this);
            }
            if(tryAgain) {
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
    fStatus = status;
    if(fSync)
        fSyncWakeup->signal(status);
    else if(fComplete)
		(*fComplete)(fRefCon, status, fDevice, this);

    return status;
}

// gotAck
//
//

void IOFWAsyncCommand::gotAck(int ackCode)
{
    int rcode;
    switch (ackCode) 
	{
		case kFWAckPending:
			// This shouldn't happen.
			IOLog("Command 0x%p received Ack code %d\n", this, ackCode);
			return;
    
		case kFWAckComplete:
			rcode = kFWResponseComplete;
			break;

		// Device is still busy after several hardware retries
		// Stash away that fact to use when the command times out.
		case kFWAckBusyX:
		case kFWAckBusyA:
		case kFWAckBusyB:
			fStatus = kIOFireWireResponseBase+kFWResponseConflictError;
			return;	// Retry after command times out
			
		// Device isn't acking at all
		case kFWAckTimeout:
			return;	// Retry after command times out
	
		default:
			rcode = kFWResponseTypeError;	// Block transfers will try quad now
    }

    gotPacket(rcode, NULL, 0);
}
