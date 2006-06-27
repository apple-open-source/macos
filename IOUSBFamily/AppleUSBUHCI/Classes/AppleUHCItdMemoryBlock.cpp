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

#include "AppleUSBUHCI.h"
#include "AppleUHCItdMemoryBlock.h"
#include "AppleUHCIListElement.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleUHCItdMemoryBlock, IOBufferMemoryDescriptor);

AppleUHCItdMemoryBlock*
AppleUHCItdMemoryBlock::NewMemoryBlock(void)
{
    AppleUHCItdMemoryBlock					*me = new AppleUHCItdMemoryBlock;
    IOByteCount								len;
    
    if (!me)
		USBError(1, "AppleUHCItdMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kUHCIPageSize, kUHCIPageSize)) 
    {
		USBError(1, "AppleUHCItdMemoryBlock::NewMemoryBlock, initWithOptions failed!");
		me->release();
		return NULL;
    }
    
    me->prepare();
    me->_sharedLogical = (UHCITransferDescriptorSharedPtr)me->getBytesNoCopy();
    bzero(me->_sharedLogical, kUHCIPageSize);
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    return me;
}



UInt32
AppleUHCItdMemoryBlock::NumTDs(void)
{
    return TDsPerBlock;
}



IOPhysicalAddress				
AppleUHCItdMemoryBlock::GetPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    if (index < TDsPerBlock)
		ret = _sharedPhysical + (index * sizeof(UHCITransferDescriptorShared));
    return ret;
}


UHCITransferDescriptorSharedPtr
AppleUHCItdMemoryBlock::GetLogicalPtr(UInt32 index)
{
    UHCITransferDescriptorSharedPtr ret = NULL;
    if (index < TDsPerBlock)
		ret = &_sharedLogical[index];
    return ret;
}


AppleUHCItdMemoryBlock*
AppleUHCItdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleUHCItdMemoryBlock::SetNextBlock(AppleUHCItdMemoryBlock* next)
{
    _nextBlock = next;
}
