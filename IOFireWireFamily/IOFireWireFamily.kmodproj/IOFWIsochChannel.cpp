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
 * HISTORY
 *
 */

#include <IOKit/assert.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWIsochChannel.h>
#include <IOKit/firewire/IOFWIsochPort.h>
#include <libkern/c++/OSSet.h>
#include <libkern/c++/OSCollectionIterator.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireDevice.h>

OSDefineMetaClassAndStructors(IOFWIsochChannel, OSObject)
OSMetaClassDefineReservedUnused(IOFWIsochChannel, 0);
OSMetaClassDefineReservedUnused(IOFWIsochChannel, 1);
OSMetaClassDefineReservedUnused(IOFWIsochChannel, 2);
OSMetaClassDefineReservedUnused(IOFWIsochChannel, 3);

void IOFWIsochChannel::threadFunc(thread_call_param_t arg, thread_call_param_t)
{
    ((IOFWIsochChannel *)arg)->reallocBandwidth();
}

bool IOFWIsochChannel::init(IOFireWireController *control, bool doIRM,
	UInt32 packetSize, IOFWSpeed prefSpeed,
	FWIsochChannelForceStopNotificationProcPtr stopProc, void *stopRefCon)
{
    if(!OSObject::init())
	return false;
    fControl = control;
    fDoIRM = doIRM;
    fPacketSize = packetSize;
    fPrefSpeed = prefSpeed;
    fStopProc = stopProc;
    fStopRefCon = stopRefCon;
    fTalker = NULL;
    fListeners = OSSet::withCapacity(1);
    fChannel = 64;	// Illegal channel
    fReadCmd = new IOFWReadQuadCommand;
    if(fReadCmd)
        fReadCmd->initAll(fControl, 0, FWAddress(), NULL, 0, NULL, NULL);

    fLockCmd = new IOFWCompareAndSwapCommand;
    if(fLockCmd)
        fLockCmd->initAll(fControl, 0, FWAddress(), NULL, NULL, 0, NULL, NULL);

    return fListeners != NULL && fReadCmd != NULL && fLockCmd != NULL;
}

void IOFWIsochChannel::free()
{
    if(fListeners)
        fListeners->release();
    if(fReadCmd)
        fReadCmd->release();
    if(fLockCmd)
        fLockCmd->release();
    OSObject::free();
}

IOReturn IOFWIsochChannel::setTalker(IOFWIsochPort *talker)
{
    fTalker = talker;
    return kIOReturnSuccess;
}

IOReturn IOFWIsochChannel::addListener(IOFWIsochPort *listener)
{
    if(fListeners->setObject(listener))
        return kIOReturnSuccess;
    else
        return kIOReturnNoMemory;
}

IOReturn IOFWIsochChannel::updateBandwidth(bool claim)
{
    FWAddress addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable);
    UInt16 irm;
    UInt32 generation;
    UInt32 newVal;
    UInt32 oldVal;
    IOReturn result = kIOReturnSuccess;
    bool tryAgain;
    do {
        fControl->getIRMNodeID(generation, irm);
        // Check we aren't being asked to reclaim in the same generation we originally allocated
        if(claim && fGeneration == generation)
            return kIOReturnSuccess;
		if (!claim && fGeneration != generation)
		{
			fChannel = 64 ;
			fBandwidth = 0 ;
			return kIOReturnSuccess ;
		}
        addr.nodeID = irm;
        if(fBandwidth != 0) {
            fReadCmd->reinit(generation, addr, &oldVal, 1);
            result = fReadCmd->submit();
            if(kIOReturnSuccess != result) {
                IOLog("IRM read result 0x%x\n", result);
                break;
            }
            do {
				if(claim) {
                    if(oldVal < fBandwidth) {
                        result = kIOReturnNoSpace;
                        break;
                    }
                    newVal = oldVal - fBandwidth;
                }
                else
                    newVal = oldVal + fBandwidth;

                fLockCmd->reinit(generation, addr, &oldVal, &newVal, 1);
                result = fLockCmd->submit();
                if(kIOReturnSuccess != result) {
                    IOLog("bandwidth update result 0x%x\n", result);
                    break;
                }
                tryAgain = !fLockCmd->locked(&oldVal);
            } while (tryAgain);
            if(claim && kIOReturnNoSpace == result) {
                // Couldn't reallocate bandwidth!
                fBandwidth = 0;
                fChannel = 64;
            }
            if(!claim)
                fBandwidth = 0;
        }
        if(fChannel != 64) {
            UInt32 mask;
            if(fChannel <= 31) {
                addr.addressLo = kCSRChannelsAvailable31_0;
                mask = 1 << (31-fChannel);
            }
            else {
                addr.addressLo = kCSRChannelsAvailable63_32;
                mask = 1 << (63-fChannel);
            }
            fReadCmd->reinit(generation, addr, &oldVal, 1);
            result = fReadCmd->submit();
             if(kIOReturnSuccess != result) {
                break;
            }
            do {
                if(claim) {
                    newVal = oldVal & ~mask;
                    if(newVal == oldVal) {
                        // Channel already allocated!
                        result = kIOReturnNoSpace;
                        break;
                    }
                }
                else {
                     newVal = oldVal | mask;
                }
                fLockCmd->reinit(generation, addr, &oldVal, &newVal, 1);
                result = fLockCmd->submit();
                if(kIOReturnSuccess != result) {
                    IOLog("channel update result 0x%x\n", result);
                    break;
                }
                tryAgain = !fLockCmd->locked(&oldVal);
            } while (tryAgain);
            if(!claim)
                fChannel = 64;
        }
    } while (false);
    if(claim && kIOReturnNoSpace == result) {
        // Couldn't reallocate bandwidth or channel
        stop();
        releaseChannel();
    }
    fGeneration = generation;
    return result;
}



IOReturn
IOFWIsochChannel::allocateChannelBegin(
	IOFWSpeed		inSpeed,			// to calculate bandwidth number
	UInt64			inAllowedChans,
	UInt32&			outChannel )
{
	UInt32 			generation ;
	UInt32 			newVal ;
	FWAddress 		addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable) ;
	UInt32 			oldIRM[3] ;
	UInt32 			channel ;
	IOReturn		err = kIOReturnSuccess ;
	
	// reserve bandwidth:
	if(fDoIRM)
	{
		// bandwidth is in units of quads at 1600 Mbs
		UInt32			bandwidth = (fPacketSize/4 + 3) * 16 / (1 << inSpeed);

		// get IRM nodeID into addr.nodeID
		fControl->getIRMNodeID(generation, addr.nodeID);
		
		// read IRM into oldIRM[3] (up to 5 retries) ;
		fReadCmd->reinit(generation, addr, oldIRM, 3);
		fReadCmd->setMaxPacket(4);		// many cameras don't like block reads to IRM registers, eg. Canon GL-1
		err = fReadCmd->submit();

		// Claim bandwidth from IRM
		if ( !err && (oldIRM[0] < bandwidth) )
			err = kIOReturnNoSpace;

		if (!err)
		{
			newVal = oldIRM[0] - bandwidth;
			fLockCmd->reinit(generation, addr, &oldIRM[0], &newVal, 1);

			err = fLockCmd->submit();
			if ( !err && !fLockCmd->locked(& oldIRM[0]) )
				err = kIOReturnCannotLock ;	
			
			if (!err)
				fBandwidth = bandwidth;
		}
	}
	
	if ( !err && fDoIRM )
	{
		// mask inAllowedChans by channels IRM has available
		inAllowedChans &= (UInt64)(oldIRM[2]) | ((UInt64)oldIRM[1] << 32);
	}
	
	// if we have an error here, the bandwidth wasn't allocated
	if (!err)
	{
		for(channel=0; channel<64; channel++)
		{
			if( inAllowedChans & ((UInt64)1 << ( 63 - channel )) )
				break;
		}

		if(channel == 64)
			err = kIOReturnNoResources;
	}
	
	if (!err && fDoIRM)
	{
		UInt32*		oldPtr;

		// Claim channel
		if(channel < 32)
		{
			addr.addressLo = kCSRChannelsAvailable31_0;
			oldPtr = &oldIRM[1];
			newVal = *oldPtr & ~(1<<(31 - channel));
		}
		else
		{
				addr.addressLo = kCSRChannelsAvailable63_32;
				oldPtr = &oldIRM[2];
				newVal = *oldPtr & ~( (UInt64)1 << (63 - channel) );
		}
		
		fLockCmd->reinit(generation, addr, oldPtr, &newVal, 1);
		err = fLockCmd->submit();
		if (!err && !fLockCmd->locked(oldPtr))
			err = kIOReturnCannotLock ;
	}

	if (!err)
		outChannel = channel;

	if(!err && fDoIRM)
	{
		fGeneration = generation;
		fControl->addAllocatedChannel(this);

		// we used to allocate the hardware resources for each port by calling AllocatePort()
		// on each one here, but that code has moved...
		// It now happens (after this function ends) in user space or kernel space depending 
		// on where the end client lives.
		// we had to do this to avoid deadlocking user apps
	}
	
    return err ;
}

IOReturn
IOFWIsochChannel::releaseChannelComplete()
{
    // release bandwidth and channel

	if(fDoIRM) {
		//
		// Tell the controller that we don't need to know about
		// bus resets before doing anything else, since a bus reset
		// sets us into the state we want (no allocated bandwidth).
		//
		fControl->removeAllocatedChannel(this);
		updateBandwidth(false);
	}
		
    return kIOReturnSuccess;
}

IOReturn IOFWIsochChannel::allocateChannel()
{
	UInt64 				portChans;
	UInt64 				allowedChans ;
	IOFWIsochPort*		listen;
	IOFWSpeed 			portSpeed;
	OSIterator *		listenIterator 		= NULL;
	IOReturn 			result 				= kIOReturnSuccess;

    // Get best speed, minimum of requested speed and paths from talker to each listener
	fSpeed = fPrefSpeed;

    do {
        // reduce speed to minimum of so far and what all ports can do,
        // and find valid channels
        allowedChans = ~(UInt64)0;
        if(fTalker) {
            fTalker->getSupported(portSpeed, portChans);
            if(portSpeed < fSpeed)
                fSpeed = portSpeed;
            allowedChans &= portChans;
        }
        listenIterator = OSCollectionIterator::withCollection(fListeners);
        if(listenIterator) {
            while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
                listen->getSupported(portSpeed, portChans);
                if(portSpeed < fSpeed)
                    fSpeed = portSpeed;
                allowedChans &= portChans;
            }
        }

        // reserve bandwidth, allocate a channel
		do {
			result = allocateChannelBegin( fSpeed, allowedChans, fChannel ) ;
		} while ( result == kIOFireWireBusReset || result == kIOReturnCannotLock ) ;
		
		if ( kIOReturnSuccess != result )
			break ;

        // allocate hardware resources for each port
        if(listenIterator) {
            listenIterator->reset();
            while( (listen = (IOFWIsochPort *) listenIterator->getNextObject()) && (result == kIOReturnSuccess)) {
                result = listen->allocatePort(fSpeed, fChannel);
            }
            if(result != kIOReturnSuccess)
                break;
        }
        if(fTalker)
            result = fTalker->allocatePort(fSpeed, fChannel);
    } while (false);

    if(listenIterator)
        listenIterator->release();

    if(result != kIOReturnSuccess) {
        releaseChannel();
    }
    return result;
}


IOReturn IOFWIsochChannel::releaseChannel()
{
    OSIterator *listenIterator;
    IOFWIsochPort *listen;

    if(fTalker) {
        fTalker->releasePort();
    }
    listenIterator = OSCollectionIterator::withCollection(fListeners);
    if(listenIterator) {
        while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
            listen->releasePort();
        }
        listenIterator->release();
    }

/*	// release bandwidth and channel
    if(fDoIRM) {

		//	Tell the controller that we don't need to know about
		//	bus resets before doing anything else, since a bus reset
		//	sets us into the state we want (no allocated bandwidth).

        fControl->removeAllocatedChannel(this);
        updateBandwidth(false);
    } */

    return releaseChannelComplete() ;
}

void IOFWIsochChannel::reallocBandwidth()
{
    updateBandwidth(true);
}

void IOFWIsochChannel::handleBusReset()
{
    thread_call_func(threadFunc, this, true);
}

IOReturn IOFWIsochChannel::start()
{
    OSIterator *listenIterator;
    IOFWIsochPort *listen;

    // Start all listeners, then start the talker
    listenIterator = OSCollectionIterator::withCollection(fListeners);
    if(listenIterator) {
        listenIterator->reset();
        while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
            listen->start();
        }
        listenIterator->release();
    }
    if(fTalker)
	fTalker->start();

    return kIOReturnSuccess;
}

IOReturn IOFWIsochChannel::stop()
{
    OSIterator *listenIterator;
    IOFWIsochPort *listen;

    // Stop all listeners, then stop the talker
    listenIterator = OSCollectionIterator::withCollection(fListeners);
    if(listenIterator) {
         while( (listen = (IOFWIsochPort *) listenIterator->getNextObject())) {
            listen->stop();
        }
        listenIterator->release();
    }
    if(fTalker)
	fTalker->stop();

    return kIOReturnSuccess;
}
