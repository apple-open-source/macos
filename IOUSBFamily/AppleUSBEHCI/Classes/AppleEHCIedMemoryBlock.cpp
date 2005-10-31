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

#include "AppleEHCIedMemoryBlock.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleEHCIedMemoryBlock, IOBufferMemoryDescriptor);

AppleEHCIedMemoryBlock*
AppleEHCIedMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCIedMemoryBlock 		*me = new AppleEHCIedMemoryBlock;
    IOByteCount				len;
    
    if (!me)
	USBError(1, "AppleEHCIedMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kEHCIPageSize, kEHCIPageSize)) 
    {
	USBError(1, "AppleEHCIedMemoryBlock::NewMemoryBlock, initWithOptions failed!");
	me->release();
	return NULL;
    }
    
    USBLog(7, "AppleEHCIedMemoryBlock::NewMemoryBlock, sizeof (me) = %d, sizeof (super) = %d", (int)sizeof(AppleEHCIedMemoryBlock), (int)sizeof(super)); 
    
    me->prepare();
    me->_sharedLogical = (EHCIQueueHeadSharedPtr)me->getBytesNoCopy();
    bzero(me->_sharedLogical, kEHCIPageSize);
    me->_sharedPhysical = me->getPhysicalSegment(0, &len);
    
    return me;
}



UInt32
AppleEHCIedMemoryBlock::NumEDs(void)
{
    return EDsPerBlock;
}



IOPhysicalAddress				
AppleEHCIedMemoryBlock::GetPhysicalPtr(UInt32 index)
{
    IOPhysicalAddress		ret = NULL;
    if (index < EDsPerBlock)
	ret = _sharedPhysical + (index * sizeof(EHCIQueueHeadShared));
    return ret;
}


EHCIQueueHeadSharedPtr
AppleEHCIedMemoryBlock::GetLogicalPtr(UInt32 index)
{
    EHCIQueueHeadSharedPtr ret = NULL;
    if (index < EDsPerBlock)
	ret = &_sharedLogical[index];
    return ret;
}


AppleEHCIedMemoryBlock*
AppleEHCIedMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCIedMemoryBlock::SetNextBlock(AppleEHCIedMemoryBlock* next)
{
    _nextBlock = next;
}
