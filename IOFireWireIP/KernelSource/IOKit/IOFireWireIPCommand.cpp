/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#define FIREWIREPRIVATE

#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/IOSyncer.h>

#include "IOFireWireIPCommand.h"

#define BCOPY(s, d, l) do { bcopy((void *) s, (void *) d, l); } while(0)

extern "C"
{
extern void moveMbufWithOffset(SInt32 tempOffset, struct mbuf **srcm, vm_address_t *src, SInt32 *srcLen);
}

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
bool IOFWIPAsyncWriteCommand::initAll(IOFireWireNub *device, UInt32 cmdLen,FWAddress 							devAddress,FWDeviceCallback completion, void *refcon, bool failOnReset)
{    
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
    if(!fMem) {
        return false;
    }
    
	fCursorBuf = (UInt8*)getBufferFromDescriptor();

    // Initialize the maxBufLen with current max configuration
    maxBufLen = cmdLen;
    
    fMaxRetries = 3;
    fCurRetries = 3;
    fMemDesc = fMem;
    fComplete = completion;
    fSync = completion == NULL;
    fRefCon = refcon;
    fTimeout = 5*8*125;
    
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


/*!
	@function reinit 
	@abstract reinit will re-initialize all the variables for this command object, good
			  when we have to reconfigure our outgoing command objects.
	@result kIOReturnSuccess if successfull.
*/
IOReturn IOFWIPAsyncWriteCommand::reinit(IOFireWireNub *device, UInt32 cmdLen,
                FWAddress devAddress, FWDeviceCallback completion, void *refcon, bool failOnReset)
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
    
    return kIOReturnSuccess;
}

/*!
	@function createFragmentedDescriptors
	@abstract creates IOVirtual ranges for fragmented Mbuf packets.
	@param none.
	@result 0 if copied successfully else non-negative value
*/
IOReturn IOFWIPAsyncWriteCommand::createFragmentedDescriptors()
{
	struct mbuf *m = fMBuf;
	vm_address_t src;
    SInt32 srcLen, dstLen, copylen, tempOffset;
    struct mbuf *temp = NULL;
    struct mbuf *srcm = NULL;

    
	// Get the source
	srcm = m; 
	srcLen = srcm->m_len;
    src = mtod(srcm, vm_offset_t);
	
	//
	// Mbuf manipulated to point at the correct offset
	//
	tempOffset = fOffset;
	
	moveMbufWithOffset(tempOffset, &srcm, &src, &srcLen);
	
	dstLen = fLength;
    copylen = dstLen;

    for (;;) 
	{
        if (fIndex > (MAX_ALLOWED_SEGS-2))
		{
			IOLog("IOFWIPCmd: Number of segs unsupported\n");
			return kIOReturnNoResources;
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
				temp = srcm->m_next; 
				srcm = temp;
				break;
			}
            // Move on to the next source mbuf.
            temp = srcm->m_next; assert(temp);
            srcm = temp;
            srcLen = srcm->m_len;
            src = mtod(srcm, vm_offset_t);
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
			{
				// set the new mbuf to point to the new chain
				break;
			}
        }
        else
		{  
			//
			// srcLen == dstLen
            // set remainder of src into the available virtual range
			//
			fVirtualRange[fIndex].address = (IOVirtualAddress)(src);
			fVirtualRange[fIndex].length = srcLen;
			fIndex++;

			copylen -= srcLen;
			
			if(copylen == 0)
			{
				// set the offset
				fOffset = 0; 
				// set the new mbuf to point to the new chain
				temp = srcm->m_next; 
				srcm = temp;
				break;
			}
            // Free current mbuf and move the current onto the next
            srcm = srcm->m_next;

            // Do we have any data left to copy?
            if (dstLen == 0)
				break;

            srcLen = srcm->m_len;
            src = mtod(srcm, vm_offset_t);
        }
    }
	
	return copylen;
}

/*!
	@function createUnFragmentedDescriptors
	@abstract creates IOVirtual ranges for fragmented Mbuf packets.
	@param none.
	@result kIOReturnSuccess if successfull, else kIOReturnError.
*/
IOReturn IOFWIPAsyncWriteCommand::createUnFragmentedDescriptors()
{
	UInt32 offset = 0;
	struct mbuf *m = fMBuf;
	struct mbuf *n = 0;
	UInt32	totalLength = 0;
	UInt32	amtCopied	= 0;
	UInt32	pktLen		= 0;
	
	fIndex = 0;
	
	if (m->m_flags & M_PKTHDR)
	{
//		IOLog("m_pkthdr.len  : %d\n", (UInt) m->m_pkthdr.len);
		pktLen = m->m_pkthdr.len;
		offset = fOffset;
	}

	while (m) 
	{
		
		if(m->m_data != NULL)
		{
			fVirtualRange[fIndex].address = (IOVirtualAddress)(m->m_data + offset);
			fVirtualRange[fIndex].length = m->m_len - offset;
//			IOLog("VR=%x len=%lx totalLength=%lx\n", fVirtualRange[fIndex].address,
//										fVirtualRange[fIndex].length,
//										totalLength+=fVirtualRange[fIndex].length);
			totalLength += fVirtualRange[fIndex].length; 
			fIndex++;
		}

		offset = 0;

        m = m->m_next;
		
		//
		// If Mbuf chain gets to the last segment
		// it will be copied into the available buffer area
		//
		if ((fIndex > MAX_ALLOWED_SEGS-3) && (m != NULL))
		{
			n = m->m_next;
			// If last mbuf, then use it in segment directly
			if(n == NULL)
			{
				fVirtualRange[fIndex].address = (IOVirtualAddress)(m->m_data);
				fVirtualRange[fIndex].length = m->m_len;
			}
			// unlucky, so lets copy rest into the pre-allocated buffer
			else
			{
				fTailMbuf 	= m;
				// Just copy the remaining length
				fLength = fLength - totalLength + fHeaderSize;
				fCursorBuf = (UInt8*)getBufferFromDescriptor();

				amtCopied = copyToBufferDescriptors();
				if(amtCopied != 0)
					return kIOReturnError;
	
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
	vm_address_t src, dst;
    SInt32 srcLen, dstLen, copylen, tempOffset;
    struct mbuf *temp = NULL;
    struct mbuf *srcm = NULL;
	UInt32 totalLen = 0;
	
	// Get the source
	srcm = fMBuf; 
	srcLen = srcm->m_len;
    src = mtod(srcm, vm_offset_t);
	
	//
	// Mbuf manipulated to point at the correct offset
	// If its last segment copy, to form the scatter gather list 
	// we don't need to move the cursor to the offset position
	//
	if(fTailMbuf != NULL)
	{
		srcm = fTailMbuf; 
		srcLen = srcm->m_len;
		src = mtod(srcm, vm_offset_t);
		tempOffset = 0;
	}
	else
	{
		tempOffset = fOffset;
		moveMbufWithOffset(tempOffset, &srcm, &src, &srcLen);
	}
	
	dstLen = fLength;
    copylen = dstLen;
	dst = (vm_address_t)fCursorBuf;

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
				temp = srcm->m_next; 
				srcm = temp;
				break;
			}
            // Move on to the next source mbuf.
            temp = srcm->m_next; 
			if(temp == NULL)
			{
				break;
			}	
			assert(temp);
            srcm = temp;
            srcLen = srcm->m_len;
            src = mtod(srcm, vm_offset_t);
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
			{
				break;
			}
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
				temp = srcm->m_next; 
				srcm = temp;
				break;
			}
            // Free current mbuf and move the current onto the next
            srcm = srcm->m_next;

            // Do we have any data left to copy?
            if (dstLen == 0)
			{
				break;
			}

            srcLen = srcm->m_len;
            src = mtod(srcm, vm_offset_t);
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
IOReturn IOFWIPAsyncWriteCommand::initDescriptor(bool unfragmented, UInt32 length)
{
	IOReturn	status = kIOReturnSuccess;
	UInt32		amtCopied = 0;
	bool        ret = false;
	
	fUnfragmented = unfragmented;
	fLength = length;

	// if we copy the payload
	if(fCopy == true)
	{
		// Increment the buffer pointer for the unfrag or frag header
		fCursorBuf = fCursorBuf + fHeaderSize;
		amtCopied = copyToBufferDescriptors();
		if(amtCopied != 0)
			return kIOReturnError;
	}
	// if we don't copy the payload
	else
	{
		// if packets are unfragmented
		if(fUnfragmented)
		{
			status = createUnFragmentedDescriptors();
		}
		// if packets are fragmented
		else
		{
			status = createFragmentedDescriptors();
		}
		
		if(status != kIOReturnSuccess)
		{
			return status;
		}
		ret = fMem->initWithRanges (fVirtualRange,
									fIndex,
									kIODirectionOutIn,
									kernel_task,
									true);
									
		if(ret == false)
		{
			IOLog("IOFWIPCmd : initWithRanges ret %d\n", ret);
			return kIOReturnError;
		}
	}
	
	return status;
}

/*!
	@function resetDescriptor
	@abstract resets the IOMemoryDescriptor & reinitializes the cursorbuf.
	@result void.
*/
void IOFWIPAsyncWriteCommand::resetDescriptor()
{
	if(fCopy == false)
	{
		fMem->initWithAddress ((void *)fBuffer->getBytesNoCopy(), 
								maxBufLen, 
								kIODirectionOutIn);
		memset(fVirtualRange, 0, MAX_ALLOWED_SEGS);
		fTailMbuf = NULL;
	}
	else
	{
		fCursorBuf = (UInt8*)getBufferFromDescriptor();
	}
	// reset the link fragment type
	fLinkFragmentType = UNFRAGMENTED;
	setDeviceObject(NULL);
	setMbuf(NULL, false);
}

/*!
	@function getDescriptorHeader
	@abstract returns a descriptor header based on fragmentation and copying
			  of payload.
	@result void.
*/
void* IOFWIPAsyncWriteCommand::getDescriptorHeader(bool unfragmented)
{
	if(fCopy == false)
	{
		// If we don't copy then return the starting point of the Mbuf
		if(unfragmented)
		{
			fOffset = fOffset - fHeaderSize;
			return fMBuf->m_data + fOffset;
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
	@function setOffset
	@abstract offset to traverse into the Mbuf.
	@result void.
*/
void IOFWIPAsyncWriteCommand::setOffset(UInt32 offset, bool fFirst)
{
	fOffset = offset;
//	fFirstPacket = fFirst;
}

/*!
	@function setHeaderSize
	@abstract Header size to account for in the IOVirtual range and buffer descriptor.
	@result void.
*/
void IOFWIPAsyncWriteCommand::setHeaderSize(UInt32 headerSize)
{
	fHeaderSize = headerSize;
}

/*!
	@function setLinkFragmentType
	@abstract sets the link fragment type.
	@result void.
*/
void IOFWIPAsyncWriteCommand::setLinkFragmentType(UInt32 fType)
{
	fLinkFragmentType = fType;
}

/*!
	@function getLinkFragmentType
	@abstract gets the link fragment type.
	@result void.
*/
UInt32 IOFWIPAsyncWriteCommand::getLinkFragmentType()
{
	return fLinkFragmentType;
}

/*!
	@function setMbuf
	@abstract returns the Mbuf from the current command object.
	@result void.
*/
void IOFWIPAsyncWriteCommand::setMbuf(struct mbuf * pkt, bool doCopy)
{
	fCopy = doCopy;
	fMBuf = pkt;
}

/*!
	@function getMbuf
	@abstract returns the Mbuf from the current command object.
	@result void.
*/
struct mbuf *IOFWIPAsyncWriteCommand::getMbuf()
{
	return fMBuf;
}

/*!
	@function setDeviceObject
	@abstract The Target device object is set, so we can 
			  send a Asynchronous write to the device.
	@result void.
*/
void IOFWIPAsyncWriteCommand::setDeviceObject(IOFireWireNub *device)
{
    fDevice = device;
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
    if(!IOFWCommand::initWithController(control))
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
    
    fMaxRetries = 3;
    fCurRetries = fMaxRetries;
    fMemDesc = fMem;
    fComplete = completion;
    fSync = completion == NULL;
    fRefCon = refcon;
    fTimeout = 1000*125;	// 1000 frames, 125mSec
    
    if(fMem)
        fSize = fMem->getLength();
    //fBytesTransferred = 0;

    fGeneration = generation;
    fChannel = channel;
    fSyncBits = sync;
    fTag = tag;
    fSpeed = speed;
    fFailOnReset = false;

	//IOLog("%s:%d tag %d chan %d sy %d \n", __FILE__, __LINE__, fTag, fChannel, fSyncBits);

    return true;
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

    fCurRetries = fMaxRetries;
    //fBytesTransferred = 0;

    fGeneration = generation;
    fChannel = channel;
    fSpeed = speed;

	//IOLog("%s:%d tag %d chan %d sy %d \n", __FILE__, __LINE__, fTag, fChannel, fSyncBits);

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

void IOFWIPAsyncStreamTxCommand::setMbuf(struct mbuf * pkt)
{
	fMBuf = pkt;
}

struct mbuf *IOFWIPAsyncStreamTxCommand::getMbuf()
{
	return fMBuf;
}


                                                                                                    
