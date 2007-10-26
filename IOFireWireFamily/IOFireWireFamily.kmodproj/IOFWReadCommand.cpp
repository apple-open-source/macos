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

OSDefineMetaClassAndStructors(IOFWReadCommand, IOFWAsyncCommand)
OSMetaClassDefineReservedUnused(IOFWReadCommand, 0);
OSMetaClassDefineReservedUnused(IOFWReadCommand, 1);

#pragma mark -

// gotPacket
//
//

void IOFWReadCommand::gotPacket(int rcode, const void* data, int size)
{
	setResponseCode( rcode );
	
    if(rcode != kFWResponseComplete) {
        //kprintf("Received rcode %d for read command 0x%x, nodeID %x\n", rcode, this, fNodeID);
        if(rcode == kFWResponseTypeError && fMaxPack > 4) {
            // try reading a quad at a time
            fMaxPack = 4;
            size = 0;
        }
        else {
            complete(kIOFireWireResponseBase+rcode);
            return;
        }
    }
    else {
        fMemDesc->writeBytes(fBytesTransferred, data, size);
        fSize -= size;
	fBytesTransferred += size;
    }

    if(fSize > 0) {
        fAddressLo += size;
        fControl->freeTrans(fTrans);  // Free old tcode
        updateTimer();
        fCurRetries = fMaxRetries;
        execute();
    }
    else {
        complete(kIOReturnSuccess);
    }
}

// initAll
//
//

bool IOFWReadCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    return IOFWAsyncCommand::initAll(device, devAddress,
                          hostMem, completion, refcon, failOnReset);
}

// initAll
//
//

bool IOFWReadCommand::initAll(IOFireWireController *control,
                              UInt32 generation, FWAddress devAddress,
        IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
        void *refcon)
{
    return IOFWAsyncCommand::initAll(control, generation, devAddress,
                          hostMem, completion, refcon);
}

// reinit
//
//

IOReturn IOFWReadCommand::reinit(FWAddress devAddress,
	IOMemoryDescriptor *hostMem,
	FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    return IOFWAsyncCommand::reinit(devAddress,
	hostMem, completion, refcon, failOnReset);
}

IOReturn IOFWReadCommand::reinit(UInt32 generation, FWAddress devAddress,
        IOMemoryDescriptor *hostMem,
        FWDeviceCallback completion, void *refcon)
{
    return IOFWAsyncCommand::reinit(generation, devAddress,
        hostMem, completion, refcon);
}

// execute
//
//

IOReturn IOFWReadCommand::execute()
{
    IOReturn result;
    int transfer;

    fStatus = kIOReturnBusy;

    if(!fFailOnReset) {
        // Update nodeID and generation
        fDevice->getNodeIDGeneration(fGeneration, fNodeID);
		fSpeed = fControl->FWSpeed( fNodeID );
		if( fMembers->fMaxSpeed < fSpeed )
		{
			fSpeed = fMembers->fMaxSpeed;
		} 
    }

    transfer = fSize;
    if(transfer > fMaxPack)
	{
		transfer = fMaxPack;
	}
	
	int maxPack = (1 << fControl->maxPackLog(fWrite, fNodeID));
	if( maxPack < transfer )
	{
		transfer = maxPack;
	}

	UInt32 flags = kIOFWReadFlagsNone;

	if( fMembers )
	{
		if( ((IOFWAsyncCommand::MemberVariables*)fMembers)->fForceBlockRequests )
		{
			flags |= kIOFWWriteBlockRequest;
		}
	}
	
    fTrans = fControl->allocTrans(this);
    if(fTrans) {
        result = fControl->asyncRead(fGeneration, fNodeID, fAddressHi,
                        fAddressLo, fSpeed, fTrans->fTCode, transfer, this, (IOFWReadFlags)flags );
    }
    else {
    //    IOLog("IOFWReadCommand::execute: Out of tLabels?\n");
        result = kIOFireWireOutOfTLabels;
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
