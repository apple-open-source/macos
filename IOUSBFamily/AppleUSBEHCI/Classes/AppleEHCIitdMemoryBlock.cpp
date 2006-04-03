/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include "AppleEHCIitdMemoryBlock.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleEHCIitdMemoryBlock, IOBufferMemoryDescriptor);

AppleEHCIitdMemoryBlock*
AppleEHCIitdMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCIitdMemoryBlock 					*me = new AppleEHCIitdMemoryBlock;
    IOByteCount							len;
    
    if (!me)
	USBError(1, "AppleEHCIitdMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kEHCIPageSize, kEHCIPageSize)) 
    {
	USBError(1, "AppleEHCIitdMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    me->prepare();
    me->_sharedLogical = (EHCIIsochTransferDescriptorSharedPtr)me->getBytesNoCopy();
    bzero(me->_sharedLogical, kEHCIPageSize);
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    return me;
}



UInt32
AppleEHCIitdMemoryBlock::NumTDs(void)
{
    return ITDsPerBlock;
}



IOPhysicalAddress				
AppleEHCIitdMemoryBlock::GetPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    if (index < ITDsPerBlock)
	ret = _sharedPhysical + (index * sizeof(EHCIIsochTransferDescriptorShared));
    return ret;
}


EHCIIsochTransferDescriptorSharedPtr
AppleEHCIitdMemoryBlock::GetLogicalPtr(UInt32 index)
{
    EHCIIsochTransferDescriptorSharedPtr ret = NULL;
    if (index < ITDsPerBlock)
	ret = &_sharedLogical[index];
    return ret;
}


AppleEHCIitdMemoryBlock*
AppleEHCIitdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCIitdMemoryBlock::SetNextBlock(AppleEHCIitdMemoryBlock* next)
{
    _nextBlock = next;
}
