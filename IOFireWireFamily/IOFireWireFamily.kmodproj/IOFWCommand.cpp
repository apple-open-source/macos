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
 * 27 May 99 wgulland created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommand.h>

#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/firewire/IOLocalConfigDirectory.h>

#define kDefaultRetries 3

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOFWCmdQ::executeQueue(bool all)
{
    IOFWCommand *cmd;
    cmd = fHead;
    while(cmd) {
        IOFWCommand *newHead;
        newHead = cmd->getNext();
        if(newHead)
            newHead->fQueuePrev = NULL;
        else
            fTail = NULL;
        fHead = newHead;

        cmd->startExecution();
        if(!all)
            break;
        cmd = newHead;
    }
    return fHead != NULL;	// ie. more to do
}


void IOFWCmdQ::headChanged(IOFWCommand *oldHead)
{
    
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOFWCommand, IOCommand )
OSDefineAbstractStructors(IOFWCommand, IOCommand)
OSMetaClassDefineReservedUnused(IOFWCommand, 0);
OSMetaClassDefineReservedUnused(IOFWCommand, 1);

bool IOFWCommand::initWithController(IOFireWireController *control)
{
    if(!IOCommand::init())
        return false;
    fControl = control;
    return true;
}

IOReturn IOFWCommand::submit(bool queue)
{
    IOReturn res;
    IOWorkLoop * workLoop = fControl->getWorkLoop();
    if(workLoop->onThread() && fSync) {
        IOLog("Potential FireWire workloop deadlock!\n");
        IOLog("Naughty cmd is a %s\n", getMetaClass()->getClassName());
    }
    
    if(fSync) {
        fSyncWakeup = IOSyncer::create();
        if(!fSyncWakeup)
            return kIOReturnNoMemory;
    }
    fControl->closeGate();
    if(queue) {
        IOFWCmdQ &pendingQ = fControl->getPendingQ();
        IOFWCommand *prev = pendingQ.fTail;
        if(!prev) {
            setHead(pendingQ);
        }
        else {
            insertAfter(*prev);
        }
        res = fStatus = kIOFireWirePending;
    }
    else {
        res = fStatus = startExecution();
    }
    fControl->openGate();

    if(res == kIOReturnBusy || res == kIOFireWirePending)
        res = kIOReturnSuccess;
    if(fSync) {
	if(res == kIOReturnSuccess)
            res = fSyncWakeup->wait();
        else
            fSyncWakeup->release();
        fSyncWakeup = NULL;
    }
    return res;
}

IOReturn IOFWCommand::startExecution()
{
    if(fTimeout) {
        AbsoluteTime delta;
        IOFWCommand *prev;
        IOFWCmdQ &timeoutQ = fControl->getTimeoutQ();
        clock_interval_to_absolutetime_interval(fTimeout, kMicrosecondScale, &delta);
        clock_get_uptime(&fDeadline);
        ADD_ABSOLUTETIME(&fDeadline, &delta);

        // add cmd to right place in list, which is sorted by deadline
        prev = timeoutQ.fTail;
        while(prev) {
            if(CMP_ABSOLUTETIME(&prev->getDeadline(), &fDeadline) != 1)
                break; // prev command's deadline is before new one, so insert here.
            prev = prev->getPrevious();
        }

        if(!prev) {
            setHead(timeoutQ);
        }
        else {
            insertAfter(*prev);
        }
    }
    return execute();
}

IOReturn IOFWCommand::complete(IOReturn status)
{
    // Remove from current queue
    removeFromQ();
    return fStatus = status;
}

void IOFWCommand::updateTimer()
{
    if(fTimeout) {
        AbsoluteTime delta;
        IOFWCommand *oldHead = fQueue->fHead;
        IOFWCommand *next;
        clock_interval_to_absolutetime_interval(fTimeout, kMicrosecondScale, &delta);
        clock_get_uptime(&fDeadline);
        ADD_ABSOLUTETIME(&fDeadline, &delta);

        // Now move command down list to keep list sorted
        next = fQueueNext;
        while(next) {
            if(CMP_ABSOLUTETIME(&next->fDeadline, &fDeadline) == 1)
                break;	// Next command's deadline still later than new deadline.
            next = next->fQueueNext;
        }
        if(next != fQueueNext) {
            // Move this command from where it is to just before 'next'
            IOFWCommand *prev;
            if(fQueuePrev) {
                assert(fQueuePrev->fQueueNext == this);
                fQueuePrev->fQueueNext = fQueueNext;
            }
            else {
                // First in list.
                assert(fQueue->fHead == this);
                fQueue->fHead = fQueueNext;
            }
            assert(fQueueNext);	// Can't be last already!
            assert(fQueueNext->fQueuePrev == this);
            fQueueNext->fQueuePrev = fQueuePrev;

            if(!next) {
                prev = fQueue->fTail;
                fQueue->fTail = this;
            }
            else {
                prev = next->fQueuePrev;
                next->fQueuePrev = this;
            }

            assert(prev);	// Must be a command to go after
            prev->fQueueNext = this;
            fQueuePrev = prev;
            fQueueNext = next;
        }
        // if the command was at the head, then either:
        // 1) it still is, but with a new, later, deadline
        // 2) it isn't, another command now is.
        // Either way, need to update the clock timeout.
        if(oldHead == this) {
            fQueue->headChanged(this);
        }
    }
}

IOReturn IOFWCommand::cancel(IOReturn reason)
{
    IOReturn result = kIOReturnSuccess;
    fControl->closeGate();
    result = complete(reason);
    fControl->openGate();
    return result;
}

void IOFWCommand::setHead(IOFWCmdQ &queue)
{
    IOFWCommand *oldHead;
    oldHead = queue.fHead;
    queue.fHead = this;
    fQueue = &queue;
    fQueuePrev = NULL;
    fQueueNext = oldHead;
    if(!oldHead)
        queue.fTail = this;
    else
        oldHead->fQueuePrev = this;
    queue.headChanged(oldHead);		// Tell queue about change
}

void IOFWCommand::insertAfter(IOFWCommand &prev)
{
    IOFWCommand *next;
    next = prev.fQueueNext;
    fQueue = prev.fQueue;
    prev.fQueueNext = this;
    fQueuePrev = &prev;
    fQueueNext = next;
    if(!next)
        fQueue->fTail = this;
    else
        next->fQueuePrev = this;
}

void IOFWCommand::removeFromQ()
{
    // Remove from queue
    if(fQueue) {
        IOFWCmdQ *queue = fQueue;
        IOFWCommand *oldHead = queue->fHead;
        if(fQueuePrev) {
            assert(fQueuePrev->fQueueNext == this);
            fQueuePrev->fQueueNext = fQueueNext;
        }
        else {
            // First in list.
            assert(queue->fHead == this);
            queue->fHead = fQueueNext;
        }
        if(fQueueNext) {
            assert(fQueueNext->fQueuePrev == this);
            fQueueNext->fQueuePrev = fQueuePrev;
        }
        else {
            // Last in list.
            assert(queue->fTail == this);
            queue->fTail = fQueuePrev;
        }
        fQueue = NULL;
        if(oldHead == this) {
            queue->headChanged(this);
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOFWBusCommand, IOFWCommand )
OSDefineAbstractStructors(IOFWBusCommand, IOFWCommand)
OSMetaClassDefineReservedUnused(IOFWBusCommand, 0);

bool IOFWBusCommand::initWithController(IOFireWireController *control, FWBusCallback completion, void *refcon)
{
    if(!IOFWCommand::initWithController(control))
	return false;

    fSync = completion == NULL;
    fComplete = completion;
    fRefCon = refcon;
    return true;
}

IOReturn IOFWBusCommand::reinit(FWBusCallback completion, void *refcon)
{
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
	return fStatus;

    fSync = completion == NULL;
    fComplete = completion;
    fRefCon = refcon;
    return kIOReturnSuccess;
}


IOReturn IOFWBusCommand::complete(IOReturn state)
{
    state = IOFWCommand::complete(state);
    if(fSync)
        fSyncWakeup->signal(state);
    else if(fComplete)
	(*fComplete)(fRefCon, state, fControl, this);
    return state;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOFWDelayCommand, IOFWBusCommand)
OSMetaClassDefineReservedUnused(IOFWDelayCommand, 0);

bool IOFWDelayCommand::initWithDelay(IOFireWireController *control,
        UInt32 delay, FWBusCallback completion, void *refcon)
{
    if(!IOFWBusCommand::initWithController(control, completion, refcon))
        return false;
    fTimeout = delay;
    return true;
}

IOReturn IOFWDelayCommand::reinit(UInt32 delay, FWBusCallback completion, void *refcon)
{
    IOReturn res;
    res = IOFWBusCommand::reinit(completion, refcon);
    if(res != kIOReturnSuccess)
        return res;
    fTimeout = delay;
    return kIOReturnSuccess;
}



IOReturn IOFWDelayCommand::execute()
{
    fStatus = kIOReturnBusy;
    return fStatus;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClass( IOFWAsyncCommand, IOFWCommand )
OSDefineAbstractStructors(IOFWAsyncCommand, IOFWCommand)
OSMetaClassDefineReservedUnused(IOFWAsyncCommand, 0);
OSMetaClassDefineReservedUnused(IOFWAsyncCommand, 1);
OSMetaClassDefineReservedUnused(IOFWAsyncCommand, 2);
OSMetaClassDefineReservedUnused(IOFWAsyncCommand, 3);

bool IOFWAsyncCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    if(!IOFWCommand::initWithController(device->getController()))
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
    fBytesTransferred = 0;

    fDevice = device;
    device->getNodeIDGeneration(fGeneration, fNodeID);
    fAddressHi = devAddress.addressHi;
    fAddressLo = devAddress.addressLo;
    fMaxPack = 1 << device->maxPackLog(fWrite, devAddress);
    fSpeed = fControl->FWSpeed(fNodeID);
    fFailOnReset = failOnReset;

    return true;
}

bool IOFWAsyncCommand::initAll(IOFireWireController *control,
        UInt32 generation, FWAddress devAddress,
        IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
        void *refcon)
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
    fBytesTransferred = 0;

    fDevice = NULL;
    fGeneration = generation;
    fNodeID = devAddress.nodeID;
    fAddressHi = devAddress.addressHi;
    fAddressLo = devAddress.addressLo;
    fMaxPack = 1 << fControl->maxPackLog(fWrite, fNodeID);
    fSpeed = fControl->FWSpeed(fNodeID);
    fFailOnReset = true;
    return true;
}


IOReturn IOFWAsyncCommand::reinit(FWAddress devAddress, IOMemoryDescriptor *hostMem,
			FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
	return fStatus;

    fComplete = completion;
    fRefCon = refcon;
    if(hostMem)
        fSize = hostMem->getLength();
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

IOReturn IOFWAsyncCommand::reinit(UInt32 generation, FWAddress devAddress, IOMemoryDescriptor *hostMem,
                        FWDeviceCallback completion, void *refcon)
{
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
        return fStatus;
    if(fDevice)
        return kIOReturnBadArgument;
    fComplete = completion;
    fRefCon = refcon;
    if(hostMem)
        fSize = hostMem->getLength();
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
    if((status == kIOFireWireBusReset) && !fFailOnReset && fControl->scanningBus()) {
        setHead(fControl->getAfterResetHandledQ());
        return fStatus = kIOFireWirePending;	// On a queue waiting to execute
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
                return fStatus = startExecution();
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

void IOFWAsyncCommand::gotAck(int ackCode)
{
    int rcode;
    switch (ackCode) {
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOFWReadCommand, IOFWAsyncCommand)
OSMetaClassDefineReservedUnused(IOFWReadCommand, 0);
OSMetaClassDefineReservedUnused(IOFWReadCommand, 1);

void IOFWReadCommand::gotPacket(int rcode, const void* data, int size)
{
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

bool IOFWReadCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    return IOFWAsyncCommand::initAll(device, devAddress,
                          hostMem, completion, refcon, failOnReset);
}

bool IOFWReadCommand::initAll(IOFireWireController *control,
                              UInt32 generation, FWAddress devAddress,
        IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
        void *refcon)
{
    return IOFWAsyncCommand::initAll(control, generation, devAddress,
                          hostMem, completion, refcon);
}

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

IOReturn IOFWReadCommand::execute()
{
    IOReturn result;
    int transfer;

    fStatus = kIOReturnBusy;

    if(!fFailOnReset) {
        // Update nodeID and generation
        fDevice->getNodeIDGeneration(fGeneration, fNodeID);
    }

    transfer = fSize;
    if(transfer > fMaxPack)
	transfer = fMaxPack;

    fTrans = fControl->allocTrans(this);
    if(fTrans) {
        result = fControl->asyncRead(fGeneration, fNodeID, fAddressHi,
                        fAddressLo, fSpeed, fTrans->fTCode, transfer, this);
    }
    else {
        IOLog("IOFWReadCommand::execute: Out of tcodes?");
        result = kIOReturnInternalError;
    }

    if(result != kIOReturnSuccess)
        complete(result);
    return (fStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOFWReadQuadCommand, IOFWAsyncCommand)
OSMetaClassDefineReservedUnused(IOFWReadQuadCommand, 0);
OSMetaClassDefineReservedUnused(IOFWReadQuadCommand, 1);

void IOFWReadQuadCommand::gotPacket(int rcode, const void* data, int size)
{
    if(rcode != kFWResponseComplete) {
		// Sony CRX1600XL responses address error to breads to ROM
        if( (rcode == kFWResponseTypeError || rcode == kFWResponseAddressError) && fMaxPack > 4) {
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
        int i;
        UInt32 *src = (UInt32 *)data;
        for(i=0; i<size/4; i++)
            *fQuads++ = *src++;
        fSize -= size;
		
		// nwg: should update bytes transferred count
		fBytesTransferred += size ;
    }

    if(fSize > 0) {
        fAddressLo += size;
        updateTimer();
        fCurRetries = fMaxRetries;
        fControl->freeTrans(fTrans);  // Free old tcode

        execute();
    }
    else {
        complete(kIOReturnSuccess);
    }
}

bool IOFWReadQuadCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	UInt32 *quads, int numQuads, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    if(!IOFWAsyncCommand::initAll(device, devAddress,
	NULL, completion, refcon, failOnReset))
	return false;
    fSize = 4*numQuads;
    fQuads = quads;
    return true;
}

bool IOFWReadQuadCommand::initAll(IOFireWireController *control,
                                  UInt32 generation, FWAddress devAddress,
        UInt32 *quads, int numQuads, FWDeviceCallback completion,
        void *refcon)
{
    if(!IOFWAsyncCommand::initAll(control, generation, devAddress,
        NULL, completion, refcon))
        return false;
    fSize = 4*numQuads;
    fQuads = quads;
    return true;
}

IOReturn IOFWReadQuadCommand::reinit(FWAddress devAddress,
	UInt32 *quads, int numQuads, FWDeviceCallback completion,
					void *refcon, bool failOnReset)
{
    IOReturn res;
    res = IOFWAsyncCommand::reinit(devAddress,
	NULL, completion, refcon, failOnReset);
    if(res != kIOReturnSuccess)
	return res;

    fSize = 4*numQuads;
    fQuads = quads;
    return res;
}

IOReturn IOFWReadQuadCommand::reinit(UInt32 generation, FWAddress devAddress,
        UInt32 *quads, int numQuads, FWDeviceCallback completion, void *refcon)
{
    IOReturn res;
    res = IOFWAsyncCommand::reinit(generation, devAddress,
        NULL, completion, refcon);
    if(res != kIOReturnSuccess)
        return res;

    fSize = 4*numQuads;
    fQuads = quads;
    return res;
}

IOReturn IOFWReadQuadCommand::execute()
{
    IOReturn result;
    int transfer;

    fStatus = kIOReturnBusy;
    
    if(!fFailOnReset) {
        // Update nodeID and generation
        fDevice->getNodeIDGeneration(fGeneration, fNodeID);
    }

    transfer = fSize;
    if(transfer > fMaxPack)
	transfer = fMaxPack;

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans = fControl->allocTrans(this);
    if(fTrans) {
        result = fControl->asyncRead(fGeneration, fNodeID, fAddressHi,
                        fAddressLo, fSpeed, fTrans->fTCode, transfer, this);
    }
    else {
        IOLog("IOFWReadQuadCommand::execute: Out of tcodes?");
        result = kIOReturnInternalError;
    }

    if(result != kIOReturnSuccess)
        complete(result);
    return (fStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOFWWriteCommand, IOFWAsyncCommand)
OSMetaClassDefineReservedUnused(IOFWWriteCommand, 0);
OSMetaClassDefineReservedUnused(IOFWWriteCommand, 1);

void IOFWWriteCommand::gotPacket(int rcode, const void* data, int size)
{
    if(rcode != kFWResponseComplete) {
        complete(kIOFireWireResponseBase+rcode);
        return;
    }
    else {
	fBytesTransferred += fPackSize;
        fSize -= fPackSize;
    }

    if(fSize > 0) {
        fAddressLo += fPackSize;

        updateTimer();
        fCurRetries = fMaxRetries;
        fControl->freeTrans(fTrans);  // Free old tcode
        execute();
    }
    else {
        complete(kIOReturnSuccess);
    }
}

bool IOFWWriteCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	IOMemoryDescriptor *hostMem, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    fWrite = true;
    return IOFWAsyncCommand::initAll(device, devAddress,
                          hostMem, completion, refcon, failOnReset);
}

bool IOFWWriteCommand::initAll(IOFireWireController *control,
                               UInt32 generation, FWAddress devAddress,
        IOMemoryDescriptor *hostMem, FWDeviceCallback completion, void *refcon)
{
    fWrite = true;
    return IOFWAsyncCommand::initAll(control, generation, devAddress,
                          hostMem, completion, refcon);
}

IOReturn IOFWWriteCommand::reinit(FWAddress devAddress,
	IOMemoryDescriptor *hostMem,
	FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    return IOFWAsyncCommand::reinit(devAddress,
	hostMem, completion, refcon, failOnReset);
}

IOReturn IOFWWriteCommand::reinit(UInt32 generation, FWAddress devAddress,
        IOMemoryDescriptor *hostMem,
        FWDeviceCallback completion, void *refcon)
{
    return IOFWAsyncCommand::reinit(generation, devAddress,
        hostMem, completion, refcon);
}

IOReturn IOFWWriteCommand::execute()
{
    IOReturn result;
    fStatus = kIOReturnBusy;

    if(!fFailOnReset) {
        // Update nodeID and generation
        fDevice->getNodeIDGeneration(fGeneration, fNodeID);
    }

    fPackSize = fSize;
    if(fPackSize > fMaxPack)
	fPackSize = fMaxPack;

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans = fControl->allocTrans(this);
    if(fTrans) {
        result = fControl->asyncWrite(fGeneration, fNodeID, fAddressHi, fAddressLo, fSpeed,
                    fTrans->fTCode, fMemDesc, fBytesTransferred, fPackSize, this);
    }
    else {
        IOLog("IOFWWriteCommand::execute: Out of tcodes?");
        result = kIOReturnInternalError;
    }

    if(result != kIOReturnSuccess)
        complete(result);
    return (fStatus);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOFWWriteQuadCommand, IOFWAsyncCommand)
OSMetaClassDefineReservedUnused(IOFWWriteQuadCommand, 0);
OSMetaClassDefineReservedUnused(IOFWWriteQuadCommand, 1);

void IOFWWriteQuadCommand::gotPacket(int rcode, const void* data, int size)
{
    if(rcode != kFWResponseComplete) {
        //kprintf("Received rcode %d for command 0x%x\n", rcode, this);
        complete(kIOFireWireResponseBase+rcode);
        return;
    }
    else {
        fQPtr += fPackSize/4;
        fSize -= fPackSize;
		fBytesTransferred += fPackSize ;
    }

    if(fSize > 0) {
        fAddressLo += fPackSize;

        updateTimer();
        fCurRetries = fMaxRetries;
        fControl->freeTrans(fTrans);  // Free old tcode

        execute();
    }
    else {
        complete(kIOReturnSuccess);
    }
}

bool IOFWWriteQuadCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	UInt32 *quads, int numQuads, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    int i;
    if(numQuads > kMaxWriteQuads)
	return false;
    fWrite = true;

    if(!IOFWAsyncCommand::initAll(device, devAddress,
	NULL, completion, refcon, failOnReset))
	return false;
    fSize = 4*numQuads;
    for(i=0; i<numQuads; i++)
        fQuads[i] = *quads++;
    fQPtr = fQuads;
    return true;
}

bool IOFWWriteQuadCommand::initAll(IOFireWireController *control,
                                   UInt32 generation, FWAddress devAddress,
        UInt32 *quads, int numQuads, FWDeviceCallback completion, void *refcon)
{
    int i;
    if(numQuads > kMaxWriteQuads)
        return false;
    fWrite = true;

    if(!IOFWAsyncCommand::initAll(control, generation, devAddress,
        NULL, completion, refcon))
        return false;
    fSize = 4*numQuads;
    for(i=0; i<numQuads; i++)
        fQuads[i] = *quads++;
    fQPtr = fQuads;
    return true;
}

IOReturn IOFWWriteQuadCommand::reinit(FWAddress devAddress,
	UInt32 *quads, int numQuads, FWDeviceCallback completion,
					void *refcon, bool failOnReset)
{
    IOReturn res;
    int i;
    if(numQuads > kMaxWriteQuads)
	return kIOReturnUnsupported;
    res = IOFWAsyncCommand::reinit(devAddress,
	NULL, completion, refcon, failOnReset);
    if(res != kIOReturnSuccess)
	return res;

    fSize = 4*numQuads;
    for(i=0; i<numQuads; i++)
        fQuads[i] = *quads++;
    fQPtr = fQuads;
    return res;
}

IOReturn IOFWWriteQuadCommand::reinit(UInt32 generation, FWAddress devAddress,
        UInt32 *quads, int numQuads, FWDeviceCallback completion, void *refcon)
{
    IOReturn res;
    int i;
    if(numQuads > kMaxWriteQuads)
        return kIOReturnUnsupported;
    res = IOFWAsyncCommand::reinit(generation, devAddress, NULL, completion, refcon);
    if(res != kIOReturnSuccess)
        return res;

    fSize = 4*numQuads;
    for(i=0; i<numQuads; i++)
        fQuads[i] = *quads++;
    fQPtr = fQuads;
    return res;
}


IOReturn IOFWWriteQuadCommand::execute()
{
    IOReturn result;
    
    fStatus = kIOReturnBusy;

    if(!fFailOnReset) {
        // Update nodeID and generation
        fDevice->getNodeIDGeneration(fGeneration, fNodeID);
    }

    fPackSize = fSize;
    if(fPackSize > fMaxPack)
	fPackSize = fMaxPack;

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans = fControl->allocTrans(this);
    if(fTrans) {
        result = fControl->asyncWrite(fGeneration, fNodeID, fAddressHi, fAddressLo,
            fSpeed, fTrans->fTCode, fQPtr, fPackSize, this);
    }
    else {
        IOLog("IOFWWriteQuadCommand::execute: Out of tcodes?");
        result = kIOReturnInternalError;
    }

    if(result != kIOReturnSuccess)
        complete(result);
		
    return (fStatus);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOFWCompareAndSwapCommand, IOFWAsyncCommand)
OSMetaClassDefineReservedUnused(IOFWCompareAndSwapCommand, 0);
OSMetaClassDefineReservedUnused(IOFWCompareAndSwapCommand, 1);
OSMetaClassDefineReservedUnused(IOFWCompareAndSwapCommand, 2);
OSMetaClassDefineReservedUnused(IOFWCompareAndSwapCommand, 3);

void IOFWCompareAndSwapCommand::gotPacket(int rcode, const void* data, int size)
{
    int i;
    if(rcode != kFWResponseComplete) {
        IOLog("Received rcode %d for lock command %p, nodeID %x\n", rcode, this, fNodeID);
        complete(kIOFireWireResponseBase+rcode);
        return;
    }
    for(i=0; i<size/4; i++) {
        fOldVal[i] = ((UInt32 *)data)[i];
    }
    complete(kIOReturnSuccess);
}

bool IOFWCompareAndSwapCommand::initAll(IOFireWireNub *device, FWAddress devAddress,
	const UInt32 *cmpVal, const UInt32 *newVal, int size, FWDeviceCallback completion,
	void *refcon, bool failOnReset)
{
    int i;
    if(!IOFWAsyncCommand::initAll(device, devAddress,
	NULL, completion, refcon, failOnReset))
	return false;
    for(i=0; i<size; i++) {
        fInputVals[i] = cmpVal[i];
        fInputVals[size+i] = newVal[i];
    }
    fSize = 8*size;
    return true;
}

bool IOFWCompareAndSwapCommand::initAll(IOFireWireController *control,
                            UInt32 generation, FWAddress devAddress,
                            const UInt32 *cmpVal, const UInt32 *newVal, int size,
                            FWDeviceCallback completion, void *refcon)
{
    int i;
    if(!IOFWAsyncCommand::initAll(control, generation, devAddress,
                                NULL, completion, refcon))
        return false;
    for(i=0; i<size; i++) {
        fInputVals[i] = cmpVal[i];
        fInputVals[size+i] = newVal[i];
    }
    fSize = 8*size;
    return true;
}

IOReturn IOFWCompareAndSwapCommand::reinit(FWAddress devAddress,
                const UInt32 *cmpVal, const UInt32 *newVal, int size,
                    FWDeviceCallback completion, void *refcon, bool failOnReset)
{
    int i;
    IOReturn res;
    res = IOFWAsyncCommand::reinit(devAddress, NULL, completion, refcon, failOnReset);
    if(res != kIOReturnSuccess)
        return res;

    for(i=0; i<size; i++) {
        fInputVals[i] = cmpVal[i];
        fInputVals[size+i] = newVal[i];
    }
    fSize = 8*size;
    return res;
}

IOReturn IOFWCompareAndSwapCommand::reinit(UInt32 generation, FWAddress devAddress,
                const UInt32 *cmpVal, const UInt32 *newVal, int size,
                                FWDeviceCallback completion, void *refcon)
{
    int i;
    IOReturn res;
    res = IOFWAsyncCommand::reinit(generation, devAddress, NULL, completion, refcon);
    if(res != kIOReturnSuccess)
        return res;

    for(i=0; i<size; i++) {
        fInputVals[i] = cmpVal[i];
        fInputVals[size+i] = newVal[i];
    }
    fSize = 8*size;
    return res;
}


IOReturn IOFWCompareAndSwapCommand::execute()
{
    IOReturn result;
    fStatus = kIOReturnBusy;

    if(!fFailOnReset) {
        // Update nodeID and generation
        fDevice->getNodeIDGeneration(fGeneration, fNodeID);
    }

    // Do this when we're in execute, not before,
    // so that Reset handling knows which commands are waiting a response.
    fTrans = fControl->allocTrans(this);
    if(fTrans) {
        result = fControl->asyncLock(fGeneration, fNodeID, fAddressHi, fAddressLo, fSpeed,
                    fTrans->fTCode, kFWExtendedTCodeCompareSwap,
                    fInputVals, fSize, this);
    }
    else {
        IOLog("IOFWCompareAndSwapCommand::execute: Out of tcodes?");
        result = kIOReturnInternalError;
    }

    if(result != kIOReturnSuccess)
        complete(result);
	else
		fBytesTransferred = fSize ;

    return (fStatus);
}

bool IOFWCompareAndSwapCommand::locked(UInt32 *oldVal)
{
    int i;
    bool ret = true;
    for(i = 0; i<fSize/8; i++) {
        ret = ret && (fInputVals[i] == fOldVal[i]);
	oldVal[i] = fOldVal[i];
    }
    return ret;
}
