/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
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
        addr.nodeID = irm;
        if(fBandwidth != 0) {
            fReadCmd->reinit(generation, addr, &oldVal, 1);
            result = fReadCmd->submit();
            if(kIOReturnSuccess != result) {
                IOLog("IRM read result 0x%x\n", result);
                break;
            }
            do {
				if(claim)
                    newVal = oldVal - fBandwidth;
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
                if(claim)
                    newVal = oldVal & ~mask;
                else
                    newVal = oldVal | mask;
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
    return result;
}

IOReturn IOFWIsochChannel::allocateChannel()
{
    UInt64 portChans;
    UInt64 allowedChans, savedChans;
    UInt16 irm;
    UInt32 generation;
    UInt32 newVal;
    FWAddress addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable);
    OSIterator *listenIterator;
    IOFWIsochPort *listen;
    IOFWSpeed portSpeed;
    UInt32 old[3];
    UInt32 bandwidth;
    UInt32 channel;
    bool tryAgain;	// For locks.
    IOReturn result = kIOReturnSuccess;

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
        if(fDoIRM) {
            fControl->getIRMNodeID(generation, irm);
            savedChans = allowedChans; // In case we have to try a few times
            // bandwidth is in units of quads at 1600 Mbs
            bandwidth = (fPacketSize/4 + 3) * 16 / (1 << fSpeed);
            addr.nodeID = irm;
            fReadCmd->reinit(generation, addr, old, 3);
            // many camera don't like block reads to IRM registers, eg. Canon GL-1
            fReadCmd->setMaxPacket(4);
            result = fReadCmd->submit();
            if(kIOReturnSuccess != result) {
                break;
            }
            
            allowedChans &= ((UInt64)old[1] << 32) | old[2];

            // Claim bandwidth
            tryAgain = false;
            do {
                if(old[0] < bandwidth) {
                    result = kIOReturnNoSpace;
                    break;
                }
                newVal = old[0] - bandwidth;
                fLockCmd->reinit(generation, addr, &old[0], &newVal, 1);
                result = fLockCmd->submit();
                if(kIOReturnSuccess != result)
                    IOLog("bandwidth update result 0x%x\n", result);
                tryAgain = !fLockCmd->locked(&old[0]);
            } while (tryAgain);
            if(kIOReturnSuccess != result)
                break;
            fBandwidth = bandwidth;
        }

        tryAgain = false;
        do {
            for(channel=0; channel<64; channel++) {
                if(allowedChans & (1<<(63-channel))) {
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
                    newVal = *oldPtr & ~(1<<(63-channel));
                }
                fLockCmd->reinit(generation, addr, oldPtr, &newVal, 1);
                result = fLockCmd->submit();
                if(kIOReturnSuccess != result)
                    IOLog("channel update result 0x%x\n", result);
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
        if(listenIterator) {
            listenIterator->reset();
            while( (listen = (IOFWIsochPort *) listenIterator->getNextObject()) && (result == kIOReturnSuccess)) {
                result = listen->allocatePort(fSpeed, fChannel);
            }
            listenIterator->release();
            if(result != kIOReturnSuccess)
                break;
        }
        if(fTalker)
            result = fTalker->allocatePort(fSpeed, fChannel);
    } while (false);

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

    // release bandwidth and channel
    if(fDoIRM) {
        /*
         * Tell the controller that we don't need to know about
         * bus resets before doing anything else, since a bus reset
         * sets us into the state we want (no allocated bandwidth).
         */
        fControl->removeAllocatedChannel(this);
        updateBandwidth(false);
    }
    return kIOReturnSuccess;
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
