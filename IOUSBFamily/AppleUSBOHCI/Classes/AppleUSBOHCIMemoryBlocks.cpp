/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBOHCIMemoryBlocks.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleUSBOHCIedMemoryBlock, IOBufferMemoryDescriptor);

AppleUSBOHCIedMemoryBlock*
AppleUSBOHCIedMemoryBlock::NewMemoryBlock(void)
{
    AppleUSBOHCIedMemoryBlock 		*me = new AppleUSBOHCIedMemoryBlock;
    IOByteCount				len;
    
    if (!me)
	USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kOHCIPageSize, kOHCIPageSize)) 
    {
	USBError(1, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    USBLog(7, "AppleUSBOHCIedMemoryBlock::NewMemoryBlock, sizeof (me) = %d, sizeof (super) = %d", sizeof(AppleUSBOHCIedMemoryBlock), sizeof(super)); 
    
    me->prepare();
    me->_sharedLogical = (OHCIEndpointDescriptorSharedPtr)me->getBytesNoCopy();
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
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
    complete();				// we need to unmap our buffer
    super::free();
}



OSDefineMetaClassAndStructors(AppleUSBOHCIgtdMemoryBlock, IOBufferMemoryDescriptor);

AppleUSBOHCIgtdMemoryBlock*
AppleUSBOHCIgtdMemoryBlock::NewMemoryBlock(void)
{
    AppleUSBOHCIgtdMemoryBlock 		*me = new AppleUSBOHCIgtdMemoryBlock;
    IOByteCount				len;
    UInt32				*block0;
    
    if (!me)
	USBError(1, "AppleUSBOHCIgtdMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kOHCIPageSize, kOHCIPageSize)) 
    {
	USBError(1, "AppleUSBOHCIgtdMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    USBLog(7, "AppleUSBOHCIgtdMemoryBlock::NewMemoryBlock, sizeof (me) = %d, sizeof (super) = %d", sizeof(AppleUSBOHCIgtdMemoryBlock), sizeof(super)); 
    
    me->prepare();
    me->_sharedLogical = (OHCIGeneralTransferDescriptorSharedPtr)me->getBytesNoCopy();
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    USBLog(5, "AppleUSBOHCIgtdMemoryBlock(%p)::NewMemoryBlock: _sharedLogical (%p), _sharedPhysical(%p)", 
		    me, me->_sharedLogical, me->_sharedPhysical);
    block0 = (UInt32*) me->getBytesNoCopy();
    *block0++ = (UInt32)me;
    *block0 =     kAppleUSBOHCIMemBlockGTD;

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
    {
#ifdef MAP_MEM_IO
	blockType = IOMappedRead32(blockStart + 4);
#else
        blockType = ml_phys_read(blockStart + 4);
#endif
    }
    
    if (blockType == kAppleUSBOHCIMemBlockGTD)
    {
#ifdef MAP_MEM_IO
	me = (AppleUSBOHCIgtdMemoryBlock*)IOMappedRead32(blockStart);
#else
        me = (AppleUSBOHCIgtdMemoryBlock*)ml_phys_read(blockStart);
#endif
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
    complete();				// we need to unmap our buffer
    super::free();
}



OSDefineMetaClassAndStructors(AppleUSBOHCIitdMemoryBlock, IOBufferMemoryDescriptor);

AppleUSBOHCIitdMemoryBlock*
AppleUSBOHCIitdMemoryBlock::NewMemoryBlock(void)
{
    AppleUSBOHCIitdMemoryBlock 		*me = new AppleUSBOHCIitdMemoryBlock;
    IOByteCount				len;
    UInt32				*block0;
    
    if (!me)
	USBError(1, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kOHCIPageSize, kOHCIPageSize)) 
    {
	USBError(1, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    USBLog(7, "AppleUSBOHCIitdMemoryBlock::NewMemoryBlock, sizeof (me) = %d, sizeof (super) = %d", sizeof(AppleUSBOHCIitdMemoryBlock), sizeof(super)); 
    
    me->prepare();
    me->_sharedLogical = (OHCIIsochTransferDescriptorSharedPtr)me->getBytesNoCopy();
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    block0 = (UInt32*)me->getBytesNoCopy();
    *block0++ = (UInt32)me;
    *block0 = kAppleUSBOHCIMemBlockITD;

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
    {
#ifdef MAP_MEM_IO
	blockType = IOMappedRead32(blockStart + 4);
#else
        blockType = ml_phys_read(blockStart + 4);
#endif
    }
    
    if (blockType == kAppleUSBOHCIMemBlockITD)
    {
#ifdef MAP_MEM_IO
	me = (AppleUSBOHCIitdMemoryBlock*)IOMappedRead32(blockStart);
#else
        me = (AppleUSBOHCIitdMemoryBlock*)ml_phys_read(blockStart);
#endif
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
    complete();				// we need to unmap our buffer
    super::free();
}
