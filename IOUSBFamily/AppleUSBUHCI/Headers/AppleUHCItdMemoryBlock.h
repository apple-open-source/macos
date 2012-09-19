/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
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


#ifndef _IOKIT_AppleUHCItdMemoryBlock_H
#define _IOKIT_AppleUHCItdMemoryBlock_H

#include <IOKit/IOBufferMemoryDescriptor.h>

#include "UHCI.h"
#include "AppleUSBUHCI.h"
#include "AppleUHCIListElement.h"

// forward declaration
class AppleUHCITransferDescriptor;

class AppleUHCItdMemoryBlock : public OSObject
{
    OSDeclareDefaultStructors(AppleUHCItdMemoryBlock);
    
#define TDsPerBlock	(kUHCIPageSize / sizeof(UHCITransferDescriptorShared))
	
private:
    IOPhysicalAddress							_sharedPhysical;
    UHCITransferDescriptorSharedPtr				_sharedLogical;
    AppleUHCItdMemoryBlock						*_nextBlock;
	IOBufferMemoryDescriptor					*_buffer;
    
public:
		
	static AppleUHCItdMemoryBlock				*NewMemoryBlock(void);
    void										SetNextBlock(AppleUHCItdMemoryBlock *next);
    AppleUHCItdMemoryBlock						*GetNextBlock(void);
    UInt32										NumTDs(void);
    IOPhysicalAddress							GetPhysicalPtr(UInt32 index);
    UHCITransferDescriptorSharedPtr				GetLogicalPtr(UInt32 index);
    
};

#endif