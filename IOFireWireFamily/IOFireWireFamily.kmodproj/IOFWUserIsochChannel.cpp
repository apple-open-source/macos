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
//	UInt64			inAllowedChans,
	IOFWSpeed*		outActualSpeed,
	UInt32*			outActualChannel)
{
	IOReturn 	result = kIOReturnSuccess;

	if (!fBandwidthAllocated)
	{
		UInt64 			allowedChans = (UInt64)inAllowedChansHi<<32 | inAllowedChansLo ;
		UInt64 			savedChans ;
		UInt16 			irm ;
		UInt32 			generation ;
		UInt32 			newVal ;
		FWAddress 		addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable) ;
		UInt32 			old[3] ;
		UInt32 			bandwidth ;
		UInt32 			channel ;
		bool 			tryAgain ;	// For locks.
	
		IOFireWireUserClientLog_( ("IOFWUserIsochChannel::userAllocateChannelBegin: allowedChans=%08lX%08lX\n",
			(UInt32)(allowedChans >> 32), (UInt32)(allowedChans & 0xFFFFFFFF)) ) ;
	
		// Get best speed, minimum of requested speed and paths from talker to each listener
		fSpeed = inSpeed ;
		
		do {
	
			// reserve bandwidth, allocate a channel
			if(fDoIRM) {
				IOFireWireUserClientLog_(("IOFWUserIsochChannel::userAllocateChannelBegin: doing IRM\n") ) ;
				
				fControl->getIRMNodeID(generation, irm);
				savedChans = allowedChans; // In case we have to try a few times
				// bandwidth is in units of quads at 1600 Mbs
				bandwidth = (fPacketSize/4 + 3) * 16 / (1 << fSpeed);
				addr.nodeID = irm;
				
				do {
					fReadCmd->reinit(generation, addr, old, 3);
					// many cameras don't like block reads to IRM registers, eg. Canon GL-1
					fReadCmd->setMaxPacket(4);
					result = fReadCmd->submit();
				} while ( result == kIOFireWireBusReset ) ;
				
				if(kIOReturnSuccess != result) {
					break;
				}
				allowedChans &= (UInt64)(old[2]) | ((UInt64)old[1] << 32);
	
				// Claim bandwidth
				tryAgain = false;
				do {
					if(old[0] < bandwidth) {
						IOFireWireUserClientLog_(("IOFWUserIsochChannel::userAllocateChannelBegin: bandwidth=0x%08lX, old[0]=0x%08lX\n", bandwidth, old[0])) ;
						result = kIOReturnNoSpace;
						break;
					}
					newVal = old[0] - bandwidth;
					fLockCmd->reinit(generation, addr, &old[0], &newVal, 1);
					result = fLockCmd->submit();

					IOFireWireUserClientLogIfErr_( result, ("IOFWUserIsochChannel::userAllocateChannelBegin: bandwith update result 0x%x\n", result) ) ;

					tryAgain = !fLockCmd->locked(&old[0]);
				} while (tryAgain);
				if(kIOReturnSuccess != result)
			break;
				fBandwidth = bandwidth;
			}
	
			tryAgain = false;
			do {
				for(channel=0; channel<64; channel++) {
					if(allowedChans & ((UInt64)1 << ( 63 - channel )) ) {
						break;
					}
				}
				if(channel == 64) {
					result = kIOReturnNoResources;
					break;
				}
	
				// Allocate a channel
				if(fDoIRM) {
					UInt32 *oldPtr;
					// Claim channel
					if(channel < 32) {
						addr.addressLo = kCSRChannelsAvailable31_0;
						oldPtr = &old[1];
						newVal = *oldPtr & ~(1<<(31-channel));
					}
					else {
								addr.addressLo = kCSRChannelsAvailable63_32;
								oldPtr = &old[2];
								newVal = *oldPtr & ~( (UInt64)1 << (63-channel) );
					}
					fLockCmd->reinit(generation, addr, oldPtr, &newVal, 1);
					result = fLockCmd->submit();
					
					IOFireWireUserClientLogIfErr_( result, ("channel update result 0x%x\n", result) );
					tryAgain = !fLockCmd->locked(oldPtr);
				}
				else
			tryAgain = false;
			} while (tryAgain);
			if(kIOReturnSuccess != result)
				break;
			fChannel = channel;
			if(fDoIRM)
				fControl->addAllocatedChannel(this);
	
			// allocate hardware resources for each port
			// ** note: this bit of the code moved to user space. this allows us to directly call
			// allocatePort on user space ports to avoid potential deadlocking in user apps.
			// (thanks collin)
		} while (false);
		
		fBandwidthAllocated = (kIOReturnSuccess == result) ;
	}

	IOFireWireUserClientLogIfErr_( result, ( "-IOFWUserIsochChannel::userAllocateChannelBegin: result=0x%08lX, fSpeed=%u, fChannel=0x%08lX\n", (UInt32) result, fSpeed, fChannel) ) ;

	*outActualSpeed 	= fSpeed ;
	*outActualChannel	= fChannel ;

    return result;
}

IOReturn
IOFWUserIsochChannel::userReleaseChannelComplete()
{
	// allocate hardware resources for each port
	// ** note: this bit of the code moved to user space. this allows us to directly call
	// user space ports to avoid potential deadlock in user apps.

    // release bandwidth and channel

	if (fBandwidthAllocated)
		if(fDoIRM) {
			/*
			* Tell the controller that we don't need to know about
			* bus resets before doing anything else, since a bus reset
			* sets us into the state we want (no allocated bandwidth).
			*/
			fControl->removeAllocatedChannel(this);
			updateBandwidth(false);
			
			IOFireWireUserClientLog_(("IOFWUserIsochChannel::userReleaseChannelComplete: freeing up bandwidth (is now %08lX)\n", fBandwidth)) ;
		}
		
	fBandwidthAllocated = false ;	
    return kIOReturnSuccess;
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
//		if(result != kIOReturnSuccess)
//			break;
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
