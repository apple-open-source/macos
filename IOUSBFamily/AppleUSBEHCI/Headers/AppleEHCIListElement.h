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

#ifndef _IOKIT_AppleEHCIListElement_H
#define _IOKIT_AppleEHCIListElement_H

#include <libkern/c++/OSObject.h>

#include "AppleUSBEHCI.h"
#include "USBEHCI.h"

/*
	// I'm (barry) getting confused by all these bits of structures scattered
	// all over the place, so here's my map of what's in a queue head.

	AppleEHCIQueueHead (this file)
		AppleEHCIListElement (This file)
			_sharedPhysical->
			_sharedLogical->
		EHCIQueueHeadSharedPtr GetSharedLogical(void);  (USBEHCI.h)
			-> EHCIQueueHeadShared (USBEHCI.h)
				IOPhysicalAddress CurrqTDPtr;
				IOPhysicalAddress NextqTDPtr; 
				IOPhysicalAddress AltqTDPtr;

		EHCIGeneralTransferDescriptorPtr qTD;  (AppleUSBEHCI.h)
		EHCIGeneralTransferDescriptorPtr TailTD;
			->EHCIGeneralTransferDescriptor (AppleUSBEHCI.h)
				EHCIGeneralTransferDescriptorSharedPtr pShared;
					->EHCIGeneralTransferDescriptorShared (USBEHCI.h)
						IOPhysicalAddress nextTD;
						IOPhysicalAddress altTD;
				IOPhysicalAddress pPhysical;

				
*/


class AppleEHCIListElement : public OSObject
{
    OSDeclareDefaultStructors(AppleEHCIListElement)

private:    

public:

    virtual void 			SetPhysicalLink(IOPhysicalAddress next) = 0;
    virtual IOPhysicalAddress		GetPhysicalLink(void) = 0;
    virtual IOPhysicalAddress		GetPhysicalAddrWithType(void) = 0;
    virtual void			print(int level);

    IOPhysicalAddress			_sharedPhysical;			// phys address of the memory shared with the controller			
    void *				_sharedLogical;				// logical address of the above
    AppleEHCIListElement		*_logicalNext;				// the next element in the list
    
};


class AppleEHCIQueueHead : public AppleEHCIListElement
{
    OSDeclareDefaultStructors(AppleEHCIQueueHead)

private:

public:

    // constructor method
    static AppleEHCIQueueHead 			*WithSharedMemory(EHCIQueueHeadSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);

    // virtual methods
    virtual void 				SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress			GetPhysicalLink(void);
    virtual IOPhysicalAddress			GetPhysicalAddrWithType(void);
    virtual void				print(int level);
    
    // not a virtual method, because the return type assumes knowledge of the element type
    EHCIQueueHeadSharedPtr			GetSharedLogical(void);
    
    EHCIGeneralTransferDescriptorPtr		qTD;
    EHCIGeneralTransferDescriptorPtr		TailTD;
    UInt16                                      maxPacketSize;
    UInt8					direction;
    UInt8					pollM1;	
    UInt8					offset;	
    UInt8					responseToStall;
    UInt8					pad2;	

};


class AppleUSBEHCI;

class AppleEHCIIsochListElement : public AppleEHCIListElement
{
    OSDeclareDefaultStructors(AppleEHCIIsochListElement)

private:
    
public:

    virtual void			print(int level);

    AppleEHCIIsochEndpointPtr		myEndpoint;
    IOUSBIsocFrame 			*myFrames;
    IOUSBIsocCompletion 		completion;
    Boolean 				lowLatency;
    UInt64				frameNumber;			// frame number for scheduling purposes
    UInt32				frameIndex;			// index into the myFrames array
    AppleEHCIIsochListElement *		doneQueueLink;			// linkage used by done queue processing

    // pure virtual methods which must be implemented by descendants
    virtual IOReturn				UpdateFrameList(void) = 0;
    virtual IOReturn				Deallocate(AppleUSBEHCI *uim) = 0;
};

class AppleEHCIIsochTransferDescriptor : public AppleEHCIIsochListElement
{
    OSDeclareDefaultStructors(AppleEHCIIsochTransferDescriptor)

public:
    // constructor method
    static AppleEHCIIsochTransferDescriptor 	*WithSharedMemory(EHCIIsochTransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);

    // virtual methods
    virtual void 				SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress			GetPhysicalLink(void);
    virtual IOPhysicalAddress			GetPhysicalAddrWithType(void);
    virtual IOReturn				UpdateFrameList(void);
    virtual IOReturn				Deallocate(AppleUSBEHCI *uim);
    virtual void				print(int level);
    
    // not a virtual method, because the return type assumes knowledge of the element type
    EHCIIsochTransferDescriptorSharedPtr	GetSharedLogical(void);

private:
    IOReturn mungeEHCIStatus(UInt32 status, UInt16 *transferLen, UInt32 maxPacketSize, UInt8 direction);
    
};


class AppleEHCISplitIsochTransferDescriptor : public AppleEHCIIsochListElement
{
    OSDeclareDefaultStructors(AppleEHCISplitIsochTransferDescriptor)

public:

    // constructor method
    static AppleEHCISplitIsochTransferDescriptor 	*WithSharedMemory(EHCISplitIsochTransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);

    // virtual methods
    virtual void 					SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress				GetPhysicalLink(void);
    virtual IOPhysicalAddress				GetPhysicalAddrWithType(void);
    virtual IOReturn					UpdateFrameList(void);
    virtual IOReturn					Deallocate(AppleUSBEHCI *uim);
    virtual void					print(int level);

    // not a virtual method, because the return type assumes knowledge of the element type
    EHCISplitIsochTransferDescriptorSharedPtr		GetSharedLogical(void);
    
};

#endif
