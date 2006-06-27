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

#include "AppleEHCItdMemoryBlock.h"

#define super IOBufferMemoryDescriptor
OSDefineMetaClassAndStructors(AppleEHCItdMemoryBlock, IOBufferMemoryDescriptor);

AppleEHCItdMemoryBlock*
AppleEHCItdMemoryBlock::NewMemoryBlock(void)
{
    AppleEHCItdMemoryBlock					*me = new AppleEHCItdMemoryBlock;
    EHCIGeneralTransferDescriptorSharedPtr	sharedPtr;
    IOByteCount								len;
    IOPhysicalAddress						sharedPhysical;
    UInt32									i;
    
    if (!me)
		USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock, constructor failed!");
	
    // allocate exactly one physical page
    if (me && !me->initWithOptions(kIOMemorySharingTypeMask, kEHCIPageSize, kEHCIPageSize)) 
    {
		USBError(1, "AppleEHCItdMemoryBlock::NewMemoryBlock, initWithOptions failed!");
		me->release();
		return NULL;
    }
    
    me->prepare();
    sharedPtr = (EHCIGeneralTransferDescriptorSharedPtr)me->getBytesNoCopy();
    bzero(sharedPtr, kEHCIPageSize);
    sharedPhysical = me->getPhysicalSegment(0, &len);
    
    for (i=0; i < TDsPerBlock; i++)
    {
		me->_TDs[i].pPhysical = sharedPhysical+(i * sizeof(EHCIGeneralTransferDescriptorShared));
		me->_TDs[i].pShared = &sharedPtr[i];
    }
    
    return me;
}



UInt32
AppleEHCItdMemoryBlock::NumTDs(void)
{
    return TDsPerBlock;
}



EHCIGeneralTransferDescriptorPtr
AppleEHCItdMemoryBlock::GetTD(UInt32 index)
{
    return (index < TDsPerBlock) ? &_TDs[index] : NULL;
}



AppleEHCItdMemoryBlock*
AppleEHCItdMemoryBlock::GetNextBlock(void)
{
    return _nextBlock;
}



void
AppleEHCItdMemoryBlock::SetNextBlock(AppleEHCItdMemoryBlock* next)
{
    _nextBlock = next;
}
