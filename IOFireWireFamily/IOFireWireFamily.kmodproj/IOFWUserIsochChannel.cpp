/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
 *  IOFWUserIsochChannel.cpp
 *  IOFireWireFamily
 *
 *  Created by noggin on Tue May 15 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <libkern/c++/OSCollectionIterator.h>
#include <libkern/c++/OSSet.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFWIsochPort.h>

#include "IOFWUserIsochChannel.h"

// so niels can use the IOFireWireUserClientLog_ macro (et. al.)
#include"IOFireWireUserClient.h"

OSDefineMetaClassAndStructors(IOFWUserIsochChannel, IOFWIsochChannel)

IOReturn
IOFWUserIsochChannel::allocateChannel()
{
	// maybe we should call user space lib here?
	IOLog("IOFWUserIsochChannel::allocateChannel called!\n") ;
	return kIOReturnUnsupported ;
}

IOReturn
IOFWUserIsochChannel::releaseChannel()
{
	// maybe we should call user space lib here?
	IOLog("IOFWUserIsochChannel::releaseChannel called!\n") ;
	return kIOReturnUnsupported ;
}


IOReturn
IOFWUserIsochChannel::start()
{
	// maybe we should call user space lib here?
	IOLog("IOFWUserIsochChannel::start called!\n") ;
	return kIOReturnUnsupported ;
}

IOReturn
IOFWUserIsochChannel::stop()
{
	// maybe we should call user space lib here?
	IOLog("IOFWUserIsochChannel::stop called!\n") ;
	return kIOReturnUnsupported ;
}


// Note: userAllocateChannelBegin is equivalent to IOFWUserIsochChannel::allocateChannel()
// minus the bits that actually call IOFWIsochPort::allocatePort(). This is because we must
// call that function from user space to avoid deadlocking the user process. Perhaps
// the superclass IOFWIsochChannel should contain this function to avoid the potential
// for out-of-sync code.

IOReturn
IOFWUserIsochChannel::userAllocateChannelBegin(
	IOFWSpeed		inSpeed,
	UInt32			inAllowedChansHi,
	UInt32			inAllowedChansLo,
	IOFWSpeed*		outActualSpeed,
	UInt32*			outActualChannel)
{
	IOReturn 	err = kIOReturnSuccess;

	if (!fBandwidthAllocated)
		do {
			err = allocateChannelBegin( inSpeed, (UInt64)inAllowedChansHi<<32 | inAllowedChansLo, fChannel ) ;
		} while ( err == kIOFireWireBusReset || err == kIOReturnCannotLock ) ;

	if ( !err )
	{
		IOFireWireUserClientLog_("-IOFWUserIsochChannel::userAllocateChannelBegin: result=0x%08x, fSpeed=%u, fChannel=0x%08lX\n", err, fSpeed, fChannel) ;

		// set channel's speed
		fSpeed = inSpeed ;

		fBandwidthAllocated = true ;

		// return results to user space
		*outActualSpeed 	= fSpeed ;
		*outActualChannel	= fChannel ;
	}

    if( err ) {
        releaseChannel();
    }
	
    return err ;
}

IOReturn
IOFWUserIsochChannel::userReleaseChannelComplete()
{
	// allocate hardware resources for each port
	// ** note: this bit of the code moved to user space. this allows us to directly call
	// user space ports to avoid potential deadlock in user apps.

    // release bandwidth and channel

	if ( !fBandwidthAllocated )
		return kIOReturnSuccess ;

	fBandwidthAllocated = false ;	
		
    return releaseChannelComplete() ;
}

IOReturn
IOFWUserIsochChannel::allocateListenerPorts()
{
	IOFWIsochPort*		listen;
	IOReturn			result 			= kIOReturnSuccess ;
	OSIterator*			listenIterator	= OSCollectionIterator::withCollection(fListeners) ;

	if(listenIterator) {
		listenIterator->reset();
		while( (listen = (IOFWIsochPort *) listenIterator->getNextObject()) && (result == kIOReturnSuccess)) {
			result = listen->allocatePort(fSpeed, fChannel);
		}
		listenIterator->release();
	}
	
	return result ;
}

IOReturn
IOFWUserIsochChannel::allocateTalkerPort()
{
	IOReturn	result	= kIOReturnSuccess ;

	if(fTalker)
		result = fTalker->allocatePort(fSpeed, fChannel);
	
	return result ;
}
