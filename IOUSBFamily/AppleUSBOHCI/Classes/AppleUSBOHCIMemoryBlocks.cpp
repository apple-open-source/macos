/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
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

#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBOHCIMemoryBlocks.h"

#define super OSObject
OSDefineMetaClassAndStructors(AppleUSBOHCIedMemoryBlock, OSObject);

AppleUSBOHCIedMemoryBlock*
AppleUSBOHCIedMemoryBlock::NewMemoryBlock(void)
{
    AppleUSBOHCIedMemoryBlock 	*me = new AppleUSBOHCIedMemoryBlock;
    IOByteCount					len;
	IODMACommand				*dmaCommand = NULL;
	UInt64						offset = 0;
	IODMACommand::Segment32		segments;
	UInt32						numSegments = 1;
	IOReturn					status = kIOReturnSuccess;
    
    if (me)
	{
		// Use IODMACommand to get the physical address
		dmaCommand = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, PAGE_SIZE, (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
		if (!dmaCommand)
		{
			USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - could not create IODMACommand");
			return NULL;
		}
		USBLog(6, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - got IODMACommand %p", dmaCommand);
		
		// allocate one page on a page boundary below the 4GB line
		me->_buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryUnshared | kIODirectionInOut, kOHCIPageSize, kOHCIStructureAllocationPhysicalMask);
		
		// allocate exactly one physical page
		if (me->_buffer) 
		{
			status = me->_buffer->prepare();
			if (status)
			{
				USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - could not prepare buffer");
				me->_buffer->release();
				me->release();
				dmaCommand->release();
				return NULL;
			}
			me->_sharedLogical = (OHCIEndpointDescriptorSharedPtr)me->_buffer->getBytesNoCopy();
			bzero(me->_sharedLogical, kOHCIPageSize);
			status = dmaCommand->setMemoryDescriptor(me->_buffer);
			if (status)
			{
				USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - could not set memory descriptor");
				me->_buffer->complete();
				me->_buffer->release();
				me->release();
				dmaCommand->release();
				return NULL;
			}
			status = dmaCommand->gen32IOVMSegments(&offset, &segments, &numSegments);
			dmaCommand->clearMemoryDescriptor();
			dmaCommand->release();
			if (status || (numSegments != 1) || (segments.fLength != kOHCIPageSize))
			{
				USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - could not get physical segment");
				me->_buffer->complete();
				me->_buffer->release();
				me->release();
				return NULL;
			}
			me->_sharedPhysical = segments.fIOVMAddr;
		}
		else
		{
			USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock, could not allocate buffer!");
			me->release();
			me = NULL;
		}
	}
	else
	{
		USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock, constructor failed!");
    }
    return me;
}



UInt32
AppleUSBOHCIedMemoryBlock::NumEDs(void)
{
    return EDsPerBlock;
}



IOPhysicalAddress				
AppleUSBOHCIedMemoryBlock::GetSharedPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    if (index < EDsPerBlock)
		ret = _sharedPhysical + (index * sizeof(OHCIEndpointDescriptorShared));
    return ret;
}


OHCIEndpointDescriptorSharedPtr
AppleUSBOHCIedMemoryBlock::GetSharedLogicalPtr(UInt32 index)
{
    OHCIEndpointDescriptorSharedPtr ret = NULL;
    
    if (index < EDsPerBlock)
		ret = &_sharedLogical[index];
    return ret;
}


AppleOHCIEndpointDescriptorPtr
AppleUSBOHCIedMemoryBlock::GetED(UInt32 index)
{
    AppleOHCIEndpointDescriptorPtr ret = NULL;
    
    if (index < EDsPerBlock)
		ret = &_eds[index];
	
    return ret;
}


AppleUSBOHCIedMemoryBlock*
AppleUSBOHCIedMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleUSBOHCIedMemoryBlock::SetNextBlock(AppleUSBOHCIedMemoryBlock* next)
{
    _nextBlock = next;
}



void 			
AppleUSBOHCIedMemoryBlock::free()
{
    // IOKit calls this when we are going away
     if (_buffer) 
		_buffer->complete();						// we need to unmap our buffer
    super::free();
}



OSDefineMetaClassAndStructors(AppleUSBOHCIgtdMemoryBlock, OSObject);

AppleUSBOHCIgtdMemoryBlock*
AppleUSBOHCIgtdMemoryBlock::NewMemoryBlock(void)
{
    AppleUSBOHCIgtdMemoryBlock 	*me = new AppleUSBOHCIgtdMemoryBlock;
    IOByteCount					len;
	IODMACommand				*dmaCommand = NULL;
	UInt64						offset = 0;
	IODMACommand::Segment32		segments;
	UInt32						numSegments = 1;
	IOReturn					status = kIOReturnSuccess;
    UInt32						*block0;
    
    if (me)
	{
		// Use IODMACommand to get the physical address
		dmaCommand = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, PAGE_SIZE, (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
		if (!dmaCommand)
		{
			USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - could not create IODMACommand");
			return NULL;
		}
		USBLog(6, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - got IODMACommand %p", dmaCommand);
		
		// allocate one page on a page boundary below the 4GB line
		me->_buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryUnshared | kIODirectionInOut, kOHCIPageSize, kOHCIStructureAllocationPhysicalMask);
		
		// allocate exactly one physical page
		if (me->_buffer) 
		{
			status = me->_buffer->prepare();
			if (status)
			{
				USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - could not prepare buffer");
				me->_buffer->release();
				me->release();
				dmaCommand->release();
				return NULL;
			}
			me->_sharedLogical = (OHCIGeneralTransferDescriptorSharedPtr)me->_buffer->getBytesNoCopy();
			bzero(me->_sharedLogical, kOHCIPageSize);
			status = dmaCommand->setMemoryDescriptor(me->_buffer);
			if (status)
			{
				USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - could not set memory descriptor");
				me->_buffer->complete();
				me->_buffer->release();
				me->release();
				dmaCommand->release();
				return NULL;
			}
			status = dmaCommand->gen32IOVMSegments(&offset, &segments, &numSegments);
			dmaCommand->clearMemoryDescriptor();
			dmaCommand->release();
			if (status || (numSegments != 1) || (segments.fLength != kOHCIPageSize))
			{
				USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock - could not get physical segment");
				me->_buffer->complete();
				me->_buffer->release();
				me->release();
				return NULL;
			}
			me->_sharedPhysical = segments.fIOVMAddr;
			block0 = (UInt32*) me->_sharedLogical;
			*block0++ = (uintptr_t)me;
			*block0 =     kAppleUSBOHCIMemBlockGTD;
		}
		else
		{
			USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock, could not allocate buffer!");
			me->release();
			me = NULL;
		}
	}
	else
	{
		USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock, constructor failed!");
    }
    return me;
}



UInt32
AppleUSBOHCIgtdMemoryBlock::NumGTDs(void)
{
    return GTDsPerBlock;
}



IOPhysicalAddress				
AppleUSBOHCIgtdMemoryBlock::GetSharedPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    
    if (index < GTDsPerBlock)
		ret = _sharedPhysical + ((index+1) * sizeof(OHCIGeneralTransferDescriptorShared));
	
    return ret;
}


OHCIGeneralTransferDescriptorSharedPtr
AppleUSBOHCIgtdMemoryBlock::GetSharedLogicalPtr(UInt32 index)
{
    OHCIGeneralTransferDescriptorSharedPtr 	ret = NULL;
    
    if (index < GTDsPerBlock)
		ret = &_sharedLogical[index+1];
	
    return ret;
}


AppleOHCIGeneralTransferDescriptorPtr
AppleUSBOHCIgtdMemoryBlock::GetGTD(UInt32 index)
{
    AppleOHCIGeneralTransferDescriptorPtr ret = NULL;
    
    if (index < GTDsPerBlock)
		ret = &_gtds[index];
	
    return ret;
}


AppleOHCIGeneralTransferDescriptorPtr	
AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(IOPhysicalAddress addr, UInt32 blockType)
{
    // NOTE:  Don't use any USBLogs here, as this is called at primary interrupt time
    //
    IOPhysicalAddress		blockStart;
    AppleUSBOHCIgtdMemoryBlock	*me;
    UInt32			index;
    
    if (!addr)
		return NULL;
	
    blockStart = addr & ~(kOHCIPageSize-1);
    
    if (!blockType)
		blockType = IOMappedRead32(blockStart + 4);
    
    if (blockType == kAppleUSBOHCIMemBlockGTD)
    {
		me = (AppleUSBOHCIgtdMemoryBlock*)IOMappedRead32(blockStart);
		index = ((addr & (kOHCIPageSize-1)) / sizeof(OHCIGeneralTransferDescriptorShared))-1;
		return &me->_gtds[index];
    }
    else if (blockType == kAppleUSBOHCIMemBlockITD)
    {
		return (AppleOHCIGeneralTransferDescriptorPtr)AppleUSBOHCIitdMemoryBlock::GetITDFromPhysical(addr, blockType);
    }
    else
    {
		return NULL;
    }
    
}


AppleUSBOHCIgtdMemoryBlock*
AppleUSBOHCIgtdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleUSBOHCIgtdMemoryBlock::SetNextBlock(AppleUSBOHCIgtdMemoryBlock* next)
{
    _nextBlock = next;
}


void
AppleUSBOHCIgtdMemoryBlock::free()
{
    // IOKit calls this when we are going away
    if (_buffer) 
		_buffer->complete();				// we need to unmap our buffer
    super::free();
}



OSDefineMetaClassAndStructors(AppleUSBOHCIitdMemoryBlock, OSObject);

AppleUSBOHCIitdMemoryBlock*
AppleUSBOHCIitdMemoryBlock::NewMemoryBlock(void)
{
    AppleUSBOHCIitdMemoryBlock 	*me = new AppleUSBOHCIitdMemoryBlock;
    IOByteCount					len;
	IODMACommand				*dmaCommand = NULL;
	UInt64						offset = 0;
	IODMACommand::Segment32		segments;
	UInt32						numSegments = 1;
	IOReturn					status = kIOReturnSuccess;
    UInt32						*block0;
    
    if (me)
	{
		// Use IODMACommand to get the physical address
		dmaCommand = IODMACommand::withSpecification(kIODMACommandOutputHost32, 32, PAGE_SIZE, (IODMACommand::MappingOptions)(IODMACommand::kMapped | IODMACommand::kIterateOnly));
		if (!dmaCommand)
		{
			USBError(1, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock - could not create IODMACommand");
			return NULL;
		}
		USBLog(6, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock - got IODMACommand %p", dmaCommand);
		
		// allocate one page on a page boundary below the 4GB line
		me->_buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, kIOMemoryUnshared | kIODirectionInOut, kOHCIPageSize, kOHCIStructureAllocationPhysicalMask);
		
		// allocate exactly one physical page
		if (me->_buffer) 
		{
			status = me->_buffer->prepare();
			if (status)
			{
				USBError(1, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock - could not prepare buffer");
				me->_buffer->release();
				me->release();
				dmaCommand->release();
				return NULL;
			}
			me->_sharedLogical = (OHCIIsochTransferDescriptorSharedPtr)me->_buffer->getBytesNoCopy();
			bzero(me->_sharedLogical, kOHCIPageSize);
			status = dmaCommand->setMemoryDescriptor(me->_buffer);
			if (status)
			{
				USBError(1, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock - could not set memory descriptor");
				me->_buffer->complete();
				me->_buffer->release();
				me->release();
				dmaCommand->release();
				return NULL;
			}
			status = dmaCommand->gen32IOVMSegments(&offset, &segments, &numSegments);
			dmaCommand->clearMemoryDescriptor();
			dmaCommand->release();
			if (status || (numSegments != 1) || (segments.fLength != kOHCIPageSize))
			{
				USBError(1, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock - could not get physical segment");
				me->_buffer->complete();
				me->_buffer->release();
				me->release();
				return NULL;
			}
			me->_sharedPhysical = segments.fIOVMAddr;
			block0 = (UInt32*) me->_sharedLogical;
			*block0++ = (uintptr_t)me;
			*block0 =     kAppleUSBOHCIMemBlockITD;
		}
		else
		{
			USBError(1, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock, could not allocate buffer!");
			me->release();
			me = NULL;
		}
	}
	else
	{
		USBError(1, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock, constructor failed!");
    }
    return me;
}



UInt32
AppleUSBOHCIitdMemoryBlock::NumITDs(void)
{
    return ITDsPerBlock;
}



IOPhysicalAddress				
AppleUSBOHCIitdMemoryBlock::GetSharedPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    
    if (index < ITDsPerBlock)
		ret = _sharedPhysical + ((index+1) * sizeof(OHCIIsochTransferDescriptorShared));
	
    return ret;
}


OHCIIsochTransferDescriptorSharedPtr
AppleUSBOHCIitdMemoryBlock::GetSharedLogicalPtr(UInt32 index)
{
    OHCIIsochTransferDescriptorSharedPtr ret = NULL;
    
    if (index < ITDsPerBlock)
		ret = &_sharedLogical[index+1];
	
    return ret;
}


AppleOHCIIsochTransferDescriptorPtr
AppleUSBOHCIitdMemoryBlock::GetITD(UInt32 index)
{
    AppleOHCIIsochTransferDescriptorPtr ret = NULL;
    
    if (index < ITDsPerBlock)
		ret = &_itds[index];
	
    return ret;
}



AppleOHCIIsochTransferDescriptorPtr	
AppleUSBOHCIitdMemoryBlock::GetITDFromPhysical(IOPhysicalAddress addr, UInt32 blockType)
{
    IOPhysicalAddress		blockStart;
    AppleUSBOHCIitdMemoryBlock	*me;
    UInt32			index;
    
    if (!addr)
		return NULL;
	
    blockStart = addr & ~(kOHCIPageSize-1);
	
    if (!blockType)
		blockType = IOMappedRead32(blockStart + 4);
	
    if (blockType == kAppleUSBOHCIMemBlockITD)
    {
		me = (AppleUSBOHCIitdMemoryBlock*)IOMappedRead32(blockStart);
		index = ((addr & (kOHCIPageSize-1)) / sizeof(OHCIIsochTransferDescriptorShared))-1;
		return &me->_itds[index];
    }
    else if (blockType == kAppleUSBOHCIMemBlockGTD)
    {
		return (AppleOHCIIsochTransferDescriptorPtr)AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(addr, blockType);
    }
    else
    {
		return NULL;
    }
    
}


AppleUSBOHCIitdMemoryBlock*
AppleUSBOHCIitdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleUSBOHCIitdMemoryBlock::SetNextBlock(AppleUSBOHCIitdMemoryBlock* next)
{
    _nextBlock = next;
}


void 			
AppleUSBOHCIitdMemoryBlock::free()
{
    // IOKit calls this when we are going away
    if (_buffer) 
		_buffer->complete();				// we need to unmap our buffer
    super::free();
}
