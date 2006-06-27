/*
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

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBUHCI.h"
#include "UHCI.h"
#include "AppleUHCIqhMemoryBlock.h"

#define super IOBufferMemoryDescriptor

OSDefineMetaClassAndStructors(AppleUHCIqhMemoryBlock, IOBufferMemoryDescriptor);

AppleUHCIqhMemoryBlock*
AppleUHCIqhMemoryBlock::NewMemoryBlock(void)
{
    AppleUHCIqhMemoryBlock 		*me = new AppleUHCIqhMemoryBlock;
    IOByteCount					len;
    
    if (!me)
		USBError(1, "AppleUHCIqhMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kUHCIPageSize, kUHCIPageSize)) 
    {
		USBError(1, "AppleUHCIqhMemoryBlock::NewMemoryBlock, initWithOptions failed!");
		me->release();
		return NULL;
    }
    
    USBLog(7, "AppleUHCIqhMemoryBlock::NewMemoryBlock, sizeof (me) = %ld, sizeof (super) = %ld", sizeof(AppleUHCIqhMemoryBlock), sizeof(super)); 
    
    me->prepare();
    me->_sharedLogical = (UHCIQueueHeadSharedPtr)me->getBytesNoCopy();
    bzero(me->_sharedLogical, kUHCIPageSize);
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    return me;
}



UInt32
AppleUHCIqhMemoryBlock::NumQHs(void)
{
    return QHsPerBlock;
}


IOPhysicalAddress				
AppleUHCIqhMemoryBlock::GetPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    if (index < QHsPerBlock)
		ret = _sharedPhysical + (index * sizeof(UHCIQueueHeadShared));
    return ret;
}


UHCIQueueHeadSharedPtr
AppleUHCIqhMemoryBlock::GetLogicalPtr(UInt32 index)
{
    UHCIQueueHeadSharedPtr		ret = NULL;
    if (index < QHsPerBlock)
		ret = &_sharedLogical[index];
    return ret;
}


AppleUHCIqhMemoryBlock*
AppleUHCIqhMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleUHCIqhMemoryBlock::SetNextBlock(AppleUHCIqhMemoryBlock* next)
{
    _nextBlock = next;
}
