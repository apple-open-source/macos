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

OSDefineMetaClassAndStructors(IOFWAsyncStreamCommand, IOFWCommand)
OSMetaClassDefineReservedUnused(IOFWAsyncStreamCommand, 0);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamCommand, 1);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamCommand, 2);
OSMetaClassDefineReservedUnused(IOFWAsyncStreamCommand, 3);

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
    if(!IOFWCommand::initWithController(control))
        return false;
    fMaxRetries = kDefaultRetries;
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
    fFailOnReset = false;
    return true;
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

// complete
//
//

IOReturn IOFWAsyncStreamCommand::complete(IOReturn status)
{
    removeFromQ();	// Remove from current queue

    // If we're in the middle of processing a bus reset and
    // the command should be retried after a bus reset, put it on the
    // 'after reset queue'
    // If we aren't still scanning the bus, and we're supposed to retry after bus resets, turn it into device offline 
    if((status == kIOFireWireBusReset) && !fFailOnReset) {
        if(fControl->scanningBus()) {
            setHead(fControl->getAfterResetHandledQ());
            return fStatus = kIOFireWirePending;	// On a queue waiting to execute
        }
    }
    fStatus = status;
    if(fSync)
        fSyncWakeup->signal(status);
    else if(fComplete)
		(*fComplete)(fRefCon, status, fControl, this);

    return status;
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
    IOReturn result;
    
    fStatus = kIOReturnBusy;

	result = fControl->asyncStreamWrite(fGeneration,
		fSpeed, fTag, fSyncBits, fChannel,fMemDesc,0,fSize, this);

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
