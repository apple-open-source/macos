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

#ifndef _IOKIT_AppleUHCIListElement_H
#define _IOKIT_AppleUHCIListElement_H

#include <libkern/c++/OSObject.h>
#include <IOKit/usb/IOUSBControllerListElement.h>

#include "AppleUSBUHCI.h"
#include "UHCI.h"


/* Software queue head structure.
*/
class AppleUHCIQueueHead : public IOUSBControllerListElement
{
    OSDeclareDefaultStructors(AppleUHCIQueueHead)
	
private:
	
public:
	
    // constructor method
    static AppleUHCIQueueHead					*WithSharedMemory(UHCIQueueHeadSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);
	
    // virtual methods
    virtual void								SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress					GetPhysicalLink(void);
    virtual IOPhysicalAddress					GetPhysicalAddrWithType(void);
    virtual void								print(int level);
    
    // not a virtual method, because the return type assumes knowledge of the element type
    UHCIQueueHeadSharedPtr						GetSharedLogical(void);

    UInt16										functionNumber;
    UInt16										endpointNumber;
    UInt16										speed;
    UInt16										maxPacketSize;
    UInt16										pollingRate;			// For interrupt endpoints.
    UInt8										direction;
    UInt8										type;					// Control, interrupt, etc.
	UInt8										interruptSlot;			// index into the interrupt queue head tree iff type is kUSBInterrupt
    bool										stalled;
	bool										aborting;				// this endpoint is in the process of aborting
    
    // AbsoluteTime								timestamp;
        
    AppleUHCITransferDescriptor					*firstTD;				// Request queue.
    AppleUHCITransferDescriptor					*lastTD;
    
};

#define	kQHTypeDummy		0xDD

/* Software transaction descriptor structure.
*/
class AppleUHCITransferDescriptor : public IOUSBControllerListElement
{
    OSDeclareDefaultStructors(AppleUHCITransferDescriptor)
	
private:
	
public:
	
    // constructor method
    static AppleUHCITransferDescriptor			*WithSharedMemory(UHCITransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);
	
    // virtual methods
    virtual void								SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress					GetPhysicalLink(void);
    virtual IOPhysicalAddress					GetPhysicalAddrWithType(void);
    virtual void								print(int level);
    
    // not a virtual method, because the return type assumes knowledge of the element type
    UHCITransferDescriptorSharedPtr				GetSharedLogical(void);
    
    UHCIAlignmentBuffer							*alignBuffer;			// Buffer for unaligned transactions
	IOUSBCommand								*command;				// the command of which this TD is part
	IOMemoryDescriptor							*logicalBuffer;
	AppleUHCIQueueHead							*pQH;					// the queue head i am on
	bool										callbackOnTD;			// this TD kicks off a completion callback
	bool										multiXferTransaction;	// this is a multi transfer (i.e. control) transaction
	bool										finalXferInTransaction;	// this is the final part (i.e. status phase) of a control transaction
	
	// support for timeouts
	UInt32										lastFrame;				// the lower 32 bits the last time we checked this TD
    UInt32										lastRemaining;			//the "remaining" count the last time we checked
	UInt16										direction;
};


class AppleUHCIIsochTransferDescriptor : public IOUSBControllerIsochListElement
{

    OSDeclareDefaultStructors(AppleUHCIIsochTransferDescriptor)

public:
    // constructor method
    static AppleUHCIIsochTransferDescriptor		*WithSharedMemory(UHCITransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);
	
    // virtual methods
    virtual void								SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress					GetPhysicalLink(void);
    virtual IOPhysicalAddress					GetPhysicalAddrWithType(void);
    virtual IOReturn							UpdateFrameList(AbsoluteTime timeStamp);
    virtual IOReturn							Deallocate(IOUSBControllerV2 *uim);
    virtual void								print(int level);
    
    // not a virtual method, because the return type assumes knowledge of the element type
    UHCITransferDescriptorSharedPtr				GetSharedLogical(void);
    
    UHCIAlignmentBuffer							*alignBuffer;				// Buffer for unaligned transactions
	IOMemoryDescriptor							*pBuffer;
	IOReturn									frStatus;
	
};

#endif