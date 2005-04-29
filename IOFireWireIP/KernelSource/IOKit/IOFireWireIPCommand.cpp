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
#define FIREWIREPRIVATE

#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/IOSyncer.h>

#include "IOFireWireIPCommand.h"
#include "IOFWIPBusInterface.h"

#define BCOPY(s, d, l) do { bcopy((void *) s, (void *) d, l); } while(0)

#pragma mark -
#pragma mark еее IOFWIPAsyncWriteCommand methods еее

/************************************************************************************
	IOFWIPAsyncWriteCommand - Asynchronous write command class
 ***********************************************************************************/

OSDefineMetaClassAndStructors(IOFWIPAsyncWriteCommand, IOFWWriteCommand);
OSMetaClassDefineReservedUnused(IOFWIPAsyncWriteCommand, 0);
OSMetaClassDefineReservedUnused(IOFWIPAsyncWriteCommand, 1);
OSMetaClassDefineReservedUnused(IOFWIPAsyncWriteCommand, 2);
OSMetaClassDefineReservedUnused(IOFWIPAsyncWriteCommand, 3);

/*!
    @function initAll
	Initializes the Asynchronous write command object
	@result true if successfull.
*/
bool IOFWIPAsyncWriteCommand::initAll(IOFireWireIP *networkObject, UInt32 cmdLen, FWAddress devAddress, FWDeviceCallback completion, void *refcon, bool failOnReset)
{    
	fIPLocalNode = networkObject;
    
	if(!fIPLocalNode)
		return false;

	IOFireWireNub *device = fIPLocalNode->getDevice();

    if(!IOFWWriteCommand::initWithController(device->getController()))
        return false;
	
    // Create a buffer descriptor that will hold something more than MTU
    fBuffer = new IOBufferMemoryDescriptor;
    if(fBuffer == NULL)
        return false;

    if(!fBuffer->initWithOptions(kIODirectionOutIn | kIOMemoryUnshared, cmdLen, 1))
        return false;
    
    // Create a Memory descriptor that will hold the buffer descriptor's memory pointer
    fMem = IOMemoryDescriptor::withAddress((void *)fBuffer->getBytesNoCopy(), cmdLen,
                                          kIODirectionOutIn);
    if(!fMem)
        return false;

	fCursorBuf = (UInt8*)getBufferFromDescriptor();

    // Initialize the maxBufLen with current max configuration
    maxBufLen = cmdLen;
    
    fMaxRetries = 0;
    fCurRetries = fMaxRetries;
    fMemDesc = fMem;
    fComplete = completion;
    fSync = completion == NULL;
    fRefCon = refcon;
    fTimeout = 1000*125;
    
    if(fMem)
        fSize = fMem->getLength();
    fBytesTransferred = 0;
	fLinkFragmentType = UNFRAGMENTED;

    fDevice = device;
    device->getNodeIDGeneration(fGeneration, fNodeID);
    fAddressHi = devAddress.addressHi;
    fAddressLo = devAddress.addressLo;
    fMaxPack = 1 << device->maxPackLog(fWrite, devAddress);
    fSpeed = fControl->FWSpeed(fNodeID);
    fFailOnReset = failOnReset;
	
    return true;
}

/*!
	@function free
	@abstract releases the buffer and the command object.
	@param None.
	@result void.
*/
void IOFWIPAsyncWriteCommand::free()
{
    // Release the buffer descriptor
    if(fBuffer){
        fBuffer->release();
        fBuffer = NULL;
    }
    
    // Release the memory descriptor
    if(fMem){
        fMem->release();
        fMem = NULL;
    }

    // Should we free the command
    IOFWWriteCommand::free();
}

void IOFWIPAsyncWriteCommand::wait()
{
	IODelay(fTimeout);
}

/*!
	@function reinit 
	@abstract reinit will re-initialize all the variables for this command object, good
			  when we have to reconfigure our outgoing command objects.
	@result kIOReturnSuccess if successfull.
*/
IOReturn IOFWIPAsyncWriteCommand::reinit(IOFireWireNub *device, UInt32 cmdLen,
                FWAddress devAddress, FWDeviceCallback completion, void *refcon, 
				bool failOnReset, bool deferNotify)
{    
	// Check the cmd len less than the pre-allocated buffer
    if(cmdLen > maxBufLen)
        return kIOReturnNoResources;
	
    fComplete = completion;
    fRefCon = refcon;
   
    if(fMem)
        fSize = cmdLen;
		
    fBytesTransferred = 0;

    fDevice = device;
    device->getNodeIDGeneration(fGeneration, fNodeID);
    fAddressHi = devAddress.addressHi;
    fAddressLo = devAddress.addressLo;
    fMaxPack = 1 << device->maxPackLog(fWrite, devAddress);
    fSpeed = fControl->FWSpeed(fNodeID);
    fFailOnReset = failOnReset;
    fMaxRetries = 0;
    fCurRetries = fMaxRetries;
    fTimeout = 1000*125;
    
	setDeferredNotify(deferNotify);
	
    return kIOReturnSuccess;
}

IOReturn IOFWIPAsyncWriteCommand::transmit(IOFireWireNub *device, UInt32 cmdLen,
											FWAddress devAddress, FWDeviceCallback completion, void *refcon, 
											bool failOnReset, bool deferNotify, bool doQueue, FragmentType fragmentType)
{
	fLinkFragmentType = fragmentType;
	return transmit(device, cmdLen, devAddress, completion, refcon, failOnReset, deferNotify, doQueue);
}
 
IOReturn IOFWIPAsyncWriteCommand::transmit(IOFireWireNub *device, UInt32 cmdLen,
											FWAddress devAddress, FWDeviceCallback completion, void *refcon, 
											bool failOnReset, bool deferNotify, bool doQueue)
{
	IOReturn status = initDescriptor(cmdLen);

	// Initialize the command with new values of device object
	if(status == kIOReturnSuccess)
		status = reinit(device, cmdLen+fHeaderSize, devAddress, completion, refcon, failOnReset, deferNotify);

	if(status == kIOReturnSuccess)
	{
		reInitCount = 0;
		resetCount = 0;
		reInitCount++;
		submit(doQueue);
		status = getStatus();
	}
	
	switch (status)
	{
		case kIOFireWireOutOfTLabels:
		case kIOReturnNoResources:
			fIPLocalNode->fSubmitErrs++;
			break;

		case kIOFireWireIPNoResources:
			resetDescriptor(status, true);
			((IOFWIPBusInterface*)refcon)->returnAsyncCommand(this);
			fIPLocalNode->fNoResources++;
			break;

		default:
			status = kIOReturnSuccess;
			break;
	}
	
	return status;
}

/*!
	@function createFragmentedDescriptors
	@abstract creates IOVirtual ranges for fragmented Mbuf packets.
	@param none.
	@result 0 if copied successfully else non-negative value
*/
IOReturn IOFWIPAsyncWriteCommand::createFragmentedDescriptors()
{
	mbuf_t m = fMBuf;
	mbuf_t srcm = m; 

	SInt32 srcLen = mbuf_len(srcm);
	vm_address_t src = (vm_offset_t)mbuf_data(srcm);
	
	// Mbuf manipulated to point at the correct offset
	SInt32 tempOffset = fOffset;

	((IOFWIPBusInterface*)fRefCon)->moveMbufWithOffset(tempOffset, &srcm, &src, &srcLen);
	
	SInt32 dstLen = fLength;
    SInt32 copylen = dstLen;

    mbuf_t temp = NULL;

    for (;;) 
	{
        if (fIndex > (MAX_ALLOWED_SEGS-3))
		{
			fTailMbuf  = NULL;
			fCursorBuf = fCursorBuf + fHeaderSize;
			// Just copy the remaining length
			fLength = copylen;
			UInt32  residual   = copyToBufferDescriptors();
			if(residual != 0)
				return kIOFireWireIPNoResources;
				
			fVirtualRange[fIndex].address = (IOVirtualAddress)(fCursorBuf);
			fVirtualRange[fIndex].length = fLength;
			fIndex++;
			return kIOReturnSuccess;
		}

        if (srcLen < dstLen) 
		{
			// Set the remainder of src mbuf to current virtual range.
			fVirtualRange[fIndex].address = (IOVirtualAddress)(src);
			fVirtualRange[fIndex].length = srcLen;
			fIndex++;

			dstLen -= srcLen;
			copylen -= srcLen;
			// set the offset
			fOffset = fOffset + srcLen; 
			
			if(copylen == 0)
			{
				// set the new mbuf to point to the new chain
				temp = mbuf_next(srcm); 
				srcm = temp;
				break;
			}
            // Move on to the next source mbuf.
            temp = mbuf_next(srcm); assert(temp);
            srcm = temp;
            srcLen = mbuf_len(srcm);
            src = (vm_offset_t)mbuf_data(srcm);
        }
        else if (srcLen > dstLen) 
		{
            // set some of src mbuf to the next virtual range.
			fVirtualRange[fIndex].address = (IOVirtualAddress)(src);
			fVirtualRange[fIndex].length = dstLen;
			fIndex++;

            src += dstLen;
            srcLen -= dstLen;
            copylen -= dstLen;

			// set the offset
			fOffset = fOffset + dstLen; 

            // Move on to the next destination mbuf.
			if(copylen == 0)
				break;// set the new mbuf to point to the new chain
        }
        else
		{  
			// srcLen == dstLen
            // set remainder of src into the available virtual range
			fVirtualRange[fIndex].address = (IOVirtualAddress)(src);
			fVirtualRange[fIndex].length = srcLen;
			fIndex++;

			copylen -= srcLen;
			
			if(copylen == 0)
			{
				// set the offset
				fOffset = 0; 
				// set the new mbuf to point to the new chain
				temp = mbuf_next(srcm); 
				srcm = temp;
				break;
			}
            // Free current mbuf and move the current onto the next
            srcm = mbuf_next(srcm);

            // Do we have any data left to copy?
            if (dstLen == 0)
				break;

            srcLen = mbuf_len(srcm);
            src = (vm_offset_t)mbuf_data(srcm);
        }
    }
	
	return copylen;
}

/*!
	@function createUnFragmentedDescriptors
	@abstract creates IOVirtual ranges for fragmented Mbuf packets.
	@param none.
	@result kIOReturnSuccess if successfull, else kIOFireWireIPNoResources.
*/
IOReturn IOFWIPAsyncWriteCommand::createUnFragmentedDescriptors()
{
	UInt32 offset = 0;
	mbuf_t m = fMBuf;
	mbuf_t n = 0;
	UInt32	totalLength = 0;
	UInt32	residual	= 0;
	UInt32	pktLen		= 0;
	
	fIndex = 0;
	
	if (mbuf_flags(m) & M_PKTHDR)
	{
		pktLen = mbuf_pkthdr_len(m);
		offset = fOffset;
	}

	while (m) 
	{
		
		if(mbuf_data(m) != NULL)
		{
			fVirtualRange[fIndex].address = (IOVirtualAddress)((UInt8*)mbuf_data(m) + offset);
			fVirtualRange[fIndex].length = mbuf_len(m) - offset;
			totalLength += fVirtualRange[fIndex].length; 
			fIndex++;
		}

		offset = 0;

        m = mbuf_next(m);
		
		//
		// If Mbuf chain gets to the last segment
		// it will be copied into the available buffer area
		//
		if ((fIndex > MAX_ALLOWED_SEGS-3) && (m != NULL))
		{
			n = mbuf_next(m);
			// If last mbuf, then use it in segment directly
			if(n == NULL)
			{
				fVirtualRange[fIndex].address = (IOVirtualAddress)(mbuf_data(m));
				fVirtualRange[fIndex].length = mbuf_len(m);
			}
			// unlucky, so lets copy rest into the pre-allocated buffer
			else
			{
				fTailMbuf 	= m;
				// Just copy the remaining length
				fLength = fLength - totalLength + fHeaderSize;
				fCursorBuf = (UInt8*)getBufferFromDescriptor();

				residual = copyToBufferDescriptors();
				if(residual != 0)
					return kIOFireWireIPNoResources;
	
				fVirtualRange[fIndex].address = (IOVirtualAddress)(fCursorBuf);
				fVirtualRange[fIndex].length = fLength;
			}
			fIndex++;
			return kIOReturnSuccess;
		}
    }
	
	return kIOReturnSuccess;
}

/*!
	@function copyToBufferDescriptors
	@abstract copies mbuf data into the buffer pointed by IOMemoryDescriptor.
	@param none.
	@result 0 if copied successfully else non-negative value
*/
IOReturn IOFWIPAsyncWriteCommand::copyToBufferDescriptors()
{
	// Get the source
	mbuf_t srcm = fMBuf; 
	SInt32 srcLen = mbuf_len(srcm);
    vm_address_t src = (vm_offset_t)mbuf_data(srcm);
	
	//
	// Mbuf manipulated to point at the correct offset
	// If its last segment copy, to form the scatter gather list 
	// we don't need to move the cursor to the offset position
	//
	SInt32 tempOffset = 0;
	if(fTailMbuf != NULL)
	{
		srcm = fTailMbuf; 
		srcLen = mbuf_len(srcm);
		src = (vm_offset_t)mbuf_data(srcm);
	}
	else
	{
		tempOffset = fOffset;
		((IOFWIPBusInterface*)fRefCon)->moveMbufWithOffset(tempOffset, &srcm, &src, &srcLen);
	}
	
	SInt32 dstLen = fLength;
    SInt32 copylen = dstLen;
	vm_address_t dst = (vm_address_t)fCursorBuf;

    mbuf_t temp = NULL;

	UInt32 totalLen = 0;

    for (;;) 
	{
	
        if (srcLen < dstLen) 
		{
            // Copy remainder of src mbuf to current dst.
            BCOPY(src, dst, srcLen);
			totalLen += srcLen;
            dst += srcLen;
            dstLen -= srcLen;
			copylen -= srcLen;
			// set the offset
			fOffset = fOffset + srcLen; 
			
			if(copylen == 0)
			{
				// set the new mbuf to point to the new chain
				temp = mbuf_next(srcm); 
				srcm = temp;
				break;
			}
            // Move on to the next source mbuf.
            temp = mbuf_next(srcm); 
			if(temp == NULL)
				break;

			assert(temp);
            srcm = temp;
            srcLen = mbuf_len(srcm);
            src = (vm_offset_t)mbuf_data(srcm);
        }
        else if (srcLen > dstLen) 
		{
            // Copy some of src mbuf to remaining space in dst mbuf.
            BCOPY(src, dst, dstLen);
			totalLen += dstLen;
            src += dstLen;
            srcLen -= dstLen;
            copylen -= dstLen;
			// set the offset
			fOffset = fOffset + dstLen; 

            // Move on to the next destination mbuf.
			if(copylen == 0)
				break;
        }
        else 
		{   
		    /* (srcLen == dstLen) */
            // copy remainder of src into remaining space of current dst
            BCOPY(src, dst, srcLen);
			totalLen += srcLen;
			copylen -= srcLen;
			
			if(copylen == 0)
			{
				// set the offset
				fOffset = 0; 
				// set the new mbuf to point to the new chain
				temp = mbuf_next(srcm); 
				srcm = temp;
				break;
			}
            // Free current mbuf and move the current onto the next
            srcm = mbuf_next(srcm);

            // Do we have any data left to copy?
            if (dstLen == 0)
				break;

            srcLen = mbuf_len(srcm);
            src = (vm_offset_t)mbuf_data(srcm);
        }
    }

	return copylen;
}

/*!
	@function initDescriptor
	@abstract copies mbuf data into the buffer pointed by IOMemoryDescriptor.
	@param unfragmented - indicates whether the packet is fragmented or unfragmented.
	@param length - length to copy.
	@result kIOReturnSuccess, if successfull.
*/
IOReturn IOFWIPAsyncWriteCommand::initDescriptor(UInt32 length)
{
	IOReturn	status = kIOReturnSuccess;
	UInt32		residual = 0;
	bool        ret = false;
	
	fLength = length;

	// if we copy the payload
	if(fCopy == true)
	{
		// Increment the buffer pointer for the unfrag or frag header
		fCursorBuf = fCursorBuf + fHeaderSize;
		residual = copyToBufferDescriptors();
		if(residual != 0)
			return kIOFireWireIPNoResources;
	}
	// if we don't copy the payload
	else
	{
		// if packets are unfragmented
		status = (fLinkFragmentType == UNFRAGMENTED) ?  createUnFragmentedDescriptors() : createFragmentedDescriptors();
		
		if(status != kIOReturnSuccess)
			return status;

		ret = fMem->initWithRanges (fVirtualRange,
									fIndex,
									kIODirectionOutIn,
									kernel_task,
									true);
									
		if(ret == false)
			return kIOFireWireIPNoResources;
	}
	
	return status;
}

/*!
	@function resetDescriptor
	@abstract resets the IOMemoryDescriptor & reinitializes the cursorbuf.
	@result void.
*/
void IOFWIPAsyncWriteCommand::resetDescriptor(IOReturn status, bool forceFree)
{
	if(fCopy == false)
	{
		fMem->initWithAddress ((void *)fBuffer->getBytesNoCopy(), 
								maxBufLen, 
								kIODirectionOutIn);
		memset(fVirtualRange, 0, sizeof(IOVirtualRange)*MAX_ALLOWED_SEGS);
	}

	fTailMbuf = NULL;
	fCursorBuf = (UInt8*)getBufferFromDescriptor();

	mbuf_t pkt = getMbuf();
	if(pkt != NULL)
	{
		if(fLinkFragmentType == LAST_FRAGMENT || fLinkFragmentType == UNFRAGMENTED || forceFree)
		{
			// If we stalled, don't free Mbuf, the Queue will try retransmit
			if(status != kIOFireWireOutOfTLabels && status != kIOReturnNoResources)
				fIPLocalNode->freePacket((struct mbuf*)pkt);
		}
	}

	// reset the link fragment type
	fLinkFragmentType = UNFRAGMENTED;
	fDevice = NULL;
	fMBuf = NULL;
		
	resetCount++;
}

/*!
	@function getDescriptorHeader
	@abstract returns a descriptor header based on fragmentation and copying
			  of payload.
	@result void.
*/
void* IOFWIPAsyncWriteCommand::getDescriptorHeader(mbuf_t pkt, bool doCopy, FragmentType unfragmented, UInt32 headerSize, UInt32 offset)
{
	fMBuf = pkt;
	fCopy = doCopy;
	fOffset = offset;
	fHeaderSize = headerSize;
	fLinkFragmentType = unfragmented;	

	if(fCopy == false)
	{
		// If we don't copy then return the starting point of the Mbuf
		if(unfragmented == UNFRAGMENTED)
		{
			fOffset = fOffset - fHeaderSize;
			return (void*)((UInt8*)mbuf_data(fMBuf) + fOffset);
		}
		else
		{
			fIndex = 0;
			fVirtualRange[fIndex].address = (IOVirtualAddress)(getCursorBuf());
			fVirtualRange[fIndex].length = fHeaderSize;
			fIndex++;
			return getCursorBuf();
		}
	}
	
	// else return the buffer pointer
	return getBufferFromDescriptor();
}

/*!
	@function getMbuf
	@abstract returns the Mbuf from the current command object.
	@result void.
*/
mbuf_t IOFWIPAsyncWriteCommand::getMbuf()
{
	return fMBuf;
}

/*!
	@function getCursorBuf
	@abstract returns the pointer from the current position of fBuffer.
	@result void* - pre-allocated buffer pointer
*/
void* IOFWIPAsyncWriteCommand::getCursorBuf()
{
    return fCursorBuf;
}

/*!
	@function getBufferFromDescriptor
	@abstract returns the head pointer position of fBuffer. 
	@result void* - pre-allocated buffer pointer
*/
void* IOFWIPAsyncWriteCommand::getBufferFromDescriptor()
{
    return fBuffer->getBytesNoCopy();
}
    
/*!
    @function getMaxBufLen
	@abstract Usefull when MTU changes to a greater value and we need to
	          accomodate more data in the buffer without a 1394 fragmentation
	@result UInt32 - size of the pre-allocated buffer
*/
UInt32 IOFWIPAsyncWriteCommand::getMaxBufLen()
{
    return maxBufLen;
}

bool IOFWIPAsyncWriteCommand::notDoubleComplete()
{
	return (reInitCount == resetCount); 
}

#pragma mark -
#pragma mark еее IOFWIPAsyncStreamTxCommand methods еее

/************************************************************************************
	IOFWIPAsyncStreamTxCommand - AsynStream Trasmit command class
 ***********************************************************************************/

OSDefineMetaClassAndStructors(IOFWIPAsyncStreamTxCommand, IOFWAsyncStreamCommand);
OSMetaClassDefineReservedUnused(IOFWIPAsyncStreamTxCommand, 0);
OSMetaClassDefineReservedUnused(IOFWIPAsyncStreamTxCommand, 1);
OSMetaClassDefineReservedUnused(IOFWIPAsyncStreamTxCommand, 2);
OSMetaClassDefineReservedUnused(IOFWIPAsyncStreamTxCommand, 3);


/*!
    @function initAll
	Initializes the Asynchronous write command object
	@result true if successfull.
*/
bool IOFWIPAsyncStreamTxCommand::initAll(
										IOFireWireIP			*networkObject,
										IOFireWireController 	*control,
										UInt32					generation, 
										UInt32					channel,
										UInt32					sync,
										UInt32 					tag,
										UInt32					cmdLen,
										int						speed,
										FWAsyncStreamCallback	completion, 
										void					*refcon)

{    
    if(!IOFWAsyncStreamCommand::initWithController(control))
        return false;
    
	fIPLocalNode = networkObject;
    
	if(!fIPLocalNode)
		return false;
		
    // Create a buffer descriptor that will hold something more than MTU
    fBuffer = new IOBufferMemoryDescriptor;
    if(fBuffer == NULL)
        return false;

    if(!fBuffer->initWithOptions(kIODirectionOutIn | kIOMemoryUnshared, cmdLen, 1))
        return false;
    
    // Create a Memory descriptor that will hold the buffer descriptor's memory pointer
    fMem = IOMemoryDescriptor::withAddress((void *)fBuffer->getBytesNoCopy(), cmdLen,
                                          kIODirectionOutIn);
    if(!fMem) {
        return false;
    }

    // Initialize the maxBufLen with current max configuration
    maxBufLen = cmdLen;

    fMaxRetries = 0;
    fCurRetries = fMaxRetries;
    fMemDesc = fMem;
    fComplete = completion;
    fSync = completion == NULL;
    fRefCon = refcon;
    fTimeout = 1000*125;
    
    if(fMem)
        fSize = fMem->getLength();

    fGeneration = generation;
    fChannel = channel;
    fSyncBits = sync;
    fTag = tag;
    fSpeed = speed;
    fFailOnReset = false;
	
    return true;
}

void IOFWIPAsyncStreamTxCommand::wait()
{
	IODelay(fTimeout);
}

void IOFWIPAsyncStreamTxCommand::free()
{
    // Release the buffer descriptor
    if(fBuffer){
        fBuffer->release();
        fBuffer = NULL;
    }
    
    // Release the memory descriptor
    if(fMem){
        fMem->release();
        fMem = NULL;
    }

    // Should we free the command
    IOFWAsyncStreamCommand::free();
}

/*!
	@function reinit
	reinit will re-initialize all the variables for this command object, good
	when we have to reconfigure our outgoing command objects.
	@result true if successfull.
*/
IOReturn IOFWIPAsyncStreamTxCommand::reinit(
								UInt32 					generation, 
                                UInt32 					channel,
                                UInt32					cmdLen,
                                int						speed,
                                FWAsyncStreamCallback 	completion,
                                void 					*refcon)
{        
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
		return fStatus;

    // Check the cmd len less than the pre-allocated buffer
    if(cmdLen > maxBufLen)
        return kIOReturnNoResources;
        
    fComplete = completion;
    fRefCon = refcon;
   
    if(fMem)
        fSize = cmdLen;

    fMaxRetries = 0;
    fCurRetries = fMaxRetries;
    fGeneration = generation;
    fChannel = channel;
    fSpeed = speed;
	fTimeout = 1000*125;

    return fStatus = kIOReturnSuccess;
}

/*!
	@function getBufferFromDescriptor
	Usefull for copying data from the mbuf
	@result void* - pre-allocated buffer pointer
*/
void* IOFWIPAsyncStreamTxCommand::getBufferFromDesc()
{
    return fBuffer->getBytesNoCopy();
}
    
/*!
    @function getMaxBufLen
	Usefull when MTU changes to a greater value and we need to
	accomodate more data in the buffer without a 1394 fragmentation
	@result UInt32 - size of the pre-allocated buffer
*/
UInt32 IOFWIPAsyncStreamTxCommand::getMaxBufLen()
{
    return maxBufLen;
}                                                                                                  
