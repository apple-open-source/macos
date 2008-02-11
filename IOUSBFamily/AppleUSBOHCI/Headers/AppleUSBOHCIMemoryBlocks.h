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

 
#ifndef _APPLEUSBOHCIMEMORYBLOCKS_H_
#define _APPLEUSBOHCIMEMORYBLOCKS_H_

#include <IOKit/IOBufferMemoryDescriptor.h>

#include "AppleUSBOHCI.h"
#include "USBOHCI.h"

enum
{
    kAppleUSBOHCIMemBlockGTD	= 	' gtd',
    kAppleUSBOHCIMemBlockITD	=	' itd'
};


class AppleUSBOHCIedMemoryBlock : public OSObject
{
    OSDeclareDefaultStructors(AppleUSBOHCIedMemoryBlock);
    
#define EDsPerBlock	(kOHCIPageSize / sizeof(OHCIEndpointDescriptorShared))

private:
    IOPhysicalAddress					_sharedPhysical;
    OHCIEndpointDescriptorSharedPtr		_sharedLogical;
    AppleUSBOHCIedMemoryBlock			*_nextBlock;
    AppleOHCIEndpointDescriptor			_eds[EDsPerBlock];	// the non shared data
	IOBufferMemoryDescriptor			*_buffer;
    
public:

    virtual void						free();
    static AppleUSBOHCIedMemoryBlock 	*NewMemoryBlock(void);
    void								SetNextBlock(AppleUSBOHCIedMemoryBlock *next);
    AppleUSBOHCIedMemoryBlock			*GetNextBlock(void);
    UInt32								NumEDs(void);
    IOPhysicalAddress					GetSharedPhysicalPtr(UInt32 index);
    OHCIEndpointDescriptorSharedPtr		GetSharedLogicalPtr(UInt32 index);
    AppleOHCIEndpointDescriptorPtr		GetED(UInt32 index);
};



class AppleUSBOHCIgtdMemoryBlock : public OSObject
{
    OSDeclareDefaultStructors(AppleUSBOHCIgtdMemoryBlock);
    
#define GTDsPerBlock	((kOHCIPageSize / sizeof(OHCIGeneralTransferDescriptorShared)) - 1)

private:
    IOPhysicalAddress							_sharedPhysical;
    OHCIGeneralTransferDescriptorSharedPtr		_sharedLogical;
    AppleUSBOHCIgtdMemoryBlock					*_nextBlock;
    AppleOHCIGeneralTransferDescriptor			_gtds[GTDsPerBlock];	// the non shared data
	IOBufferMemoryDescriptor					*_buffer;
    
public:

    virtual void									free();
    static AppleUSBOHCIgtdMemoryBlock				*NewMemoryBlock(void);
    static AppleOHCIGeneralTransferDescriptorPtr	GetGTDFromPhysical(IOPhysicalAddress addr, UInt32 blockType = 0);
    void											SetNextBlock(AppleUSBOHCIgtdMemoryBlock *next);
    AppleUSBOHCIgtdMemoryBlock						*GetNextBlock(void);
    UInt32											NumGTDs(void);
    IOPhysicalAddress								GetSharedPhysicalPtr(UInt32 index);
    OHCIGeneralTransferDescriptorSharedPtr			GetSharedLogicalPtr(UInt32 index);
    AppleOHCIGeneralTransferDescriptorPtr			GetGTD(UInt32 index);
};



class AppleUSBOHCIitdMemoryBlock : public OSObject
{
    OSDeclareDefaultStructors(AppleUSBOHCIitdMemoryBlock);
    
#define ITDsPerBlock	((kOHCIPageSize / sizeof(OHCIIsochTransferDescriptorShared)) - 1)

private:
    IOPhysicalAddress								_sharedPhysical;
    OHCIIsochTransferDescriptorSharedPtr			_sharedLogical;
    AppleUSBOHCIitdMemoryBlock						*_nextBlock;
    AppleOHCIIsochTransferDescriptor				_itds[ITDsPerBlock];	// the non shared data
	IOBufferMemoryDescriptor						*_buffer;
    
public:

    virtual void									free();
    static AppleUSBOHCIitdMemoryBlock				*NewMemoryBlock(void);
    static AppleOHCIIsochTransferDescriptorPtr		GetITDFromPhysical(IOPhysicalAddress addr, UInt32 blockType = 0);
    void											SetNextBlock(AppleUSBOHCIitdMemoryBlock *next);
    AppleUSBOHCIitdMemoryBlock						*GetNextBlock(void);
    UInt32											NumITDs(void);
    IOPhysicalAddress								GetSharedPhysicalPtr(UInt32 index);
    OHCIIsochTransferDescriptorSharedPtr			GetSharedLogicalPtr(UInt32 index);
    AppleOHCIIsochTransferDescriptorPtr				GetITD(UInt32 index);
};

