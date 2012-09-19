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

#include <libkern/c++/OSObject.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "AppleUSBEHCI.h"
#include "USBEHCI.h"

class AppleEHCIedMemoryBlock : public OSObject
{
    
	OSDeclareDefaultStructors(AppleEHCIedMemoryBlock)
	
#define EDsPerBlock	(kEHCIPageSize / sizeof(EHCIQueueHeadShared))

private:
    IOPhysicalAddress			_sharedPhysical;
    EHCIQueueHeadSharedPtr		_sharedLogical;
    AppleEHCIedMemoryBlock		*_nextBlock;
	IOBufferMemoryDescriptor	*_buffer;
    
public:

	// OSObject call used to free the buffer when we are done
    virtual void free();
	
    static AppleEHCIedMemoryBlock 	*NewMemoryBlock(void);
    void							SetNextBlock(AppleEHCIedMemoryBlock *next);
    AppleEHCIedMemoryBlock			*GetNextBlock(void);
    UInt32							NumEDs(void);
    IOPhysicalAddress				GetPhysicalPtr(UInt32 index);
    EHCIQueueHeadSharedPtr			GetLogicalPtr(UInt32 index);
};