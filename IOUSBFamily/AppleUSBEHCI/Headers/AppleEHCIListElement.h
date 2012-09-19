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


#ifndef _IOKIT_AppleEHCIListElement_H
#define _IOKIT_AppleEHCIListElement_H

#include <libkern/c++/OSObject.h>
#include <IOKit/usb/IOUSBControllerListElement.h>

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

class AppleEHCIIsochEndpoint;
class AppleUSBEHCISplitPeriodicEndpoint;
class AppleUSBEHCI;

class AppleEHCIQueueHead : public IOUSBControllerListElement
{
    OSDeclareDefaultStructors(AppleEHCIQueueHead)

private:

public:

    // constructor method
    static AppleEHCIQueueHead				*WithSharedMemory(EHCIQueueHeadSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);

    // virtual methods
    virtual void							SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress				GetPhysicalLink(void);
    virtual IOPhysicalAddress				GetPhysicalAddrWithType(void);
    virtual void							print(int level, AppleUSBEHCI *ehci);
    
    // not a virtual method, because the return type assumes knowledge of the element type
    EHCIQueueHeadSharedPtr					GetSharedLogical(void);
	
	// helper method
	UInt8									NormalizedPollingRate(void);			// returns the polling rate normalized to ms (frames)
    
    EHCIGeneralTransferDescriptorPtr		_qTD;
    EHCIGeneralTransferDescriptorPtr		_TailTD;
	AppleUSBEHCISplitPeriodicEndpoint		*_pSPE;									// for split interrupt endpoints
    UInt16                                  _maxPacketSize;
    UInt16									_functionNumber;
	UInt16									_endpointNumber;
    UInt8									_direction;
    UInt8									_responseToStall;
    UInt8									_queueType;								// Control, interrupt, etc.
	UInt8									_speed;									// the speed of this EP
	UInt8									_bInterval;								// the "raw" bInterval from the endpoint descriptor
	UInt8									_startFrame;							// beginning ms frame in a 32 ms schedule
	UInt8									_startuFrame;							// first uFrame (HS endpoints only)
	bool									_aborting;								// this queue head is in the process of aborting
	UInt16									_pollingRate;							// converted polling rate in frames for FS/LS and uFrames for HS
	USBPhysicalAddress32					_inactiveTD;							// For inactive detection
	IOPhysicalAddress						_lastSeenTD;							// For inactive QH detection
	UInt64									_lastSeenFrame;							// Also for inactive detection
	UInt32									_numTDs;								// For more intelligent broken queue detection
};


class AppleEHCIIsochTransferDescriptor : public IOUSBControllerIsochListElement
{
    OSDeclareDefaultStructors(AppleEHCIIsochTransferDescriptor)

public:
    // constructor method
    static AppleEHCIIsochTransferDescriptor 	*WithSharedMemory(EHCIIsochTransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);

    // virtual methods
    virtual void					SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress		GetPhysicalLink(void);
    virtual IOPhysicalAddress		GetPhysicalAddrWithType(void);
    virtual IOReturn				UpdateFrameList(AbsoluteTime timeStamp);
    virtual IOReturn				Deallocate(IOUSBControllerV2 *uim);
    virtual void					print(int level);
    
    // not a virtual method, because the return type assumes knowledge of the element type
    EHCIIsochTransferDescriptorSharedPtr	GetSharedLogical(void);
	
private:
    IOReturn mungeEHCIStatus(UInt32 status, UInt16 *transferLen, UInt32 maxPacketSize, UInt8 direction);
    
};



class AppleEHCISplitIsochTransferDescriptor : public IOUSBControllerIsochListElement
{
    OSDeclareDefaultStructors(AppleEHCISplitIsochTransferDescriptor)

public:
	
    // constructor method
    static AppleEHCISplitIsochTransferDescriptor 	*WithSharedMemory(EHCISplitIsochTransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical);

    // virtual methods
    virtual void						SetPhysicalLink(IOPhysicalAddress next);
    virtual IOPhysicalAddress			GetPhysicalLink(void);
    virtual IOPhysicalAddress			GetPhysicalAddrWithType(void);
    virtual IOReturn					UpdateFrameList(AbsoluteTime timeStamp);
    virtual IOReturn					Deallocate(IOUSBControllerV2 *uim);
    virtual void						print(int level);

    // not a virtual method, because the return type assumes knowledge of the element type
    EHCISplitIsochTransferDescriptorSharedPtr		GetSharedLogical(void);
    
	// split Isoch specific varibles
	bool								_isDummySITD;
};


class AppleEHCIIsochEndpoint : public IOUSBControllerIsochEndpoint
{
    OSDeclareDefaultStructors(AppleEHCIIsochEndpoint)

public:
	virtual bool			init();
	void					print(int level);
	
	AppleUSBEHCISplitPeriodicEndpoint	*pSPE;						// for split  endpoints
	void								*ttiPtr;					// pointer to the Transaction Translator (for Split EP)
    short								oneMPS;						// For high bandwidth
    short								mult;						// how many oneMPS sized transactions to do
    USBDeviceAddress					highSpeedHub;
    int									highSpeedPort;
	bool								useBackPtr;
	
	// starting fresh with the new periodic scheduler
	UInt8								_speed;						// the speed of this EP
	UInt8								_startFrame;				// beginning ms frame in a 32 ms schedule
	UInt8								_startuFrame;				// first uFrame, used for HS endpoints only!
};

#endif
