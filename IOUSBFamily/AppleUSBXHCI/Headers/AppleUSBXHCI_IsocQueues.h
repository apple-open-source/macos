//
//  AppleUSBXHCI_IsocQueues.h
//  AppleUSBXHCI
//
//  Copyright 2011-2013 Apple Inc. All rights reserved.
//

#ifndef AppleUSBXHCI_AppleUSBXHCI_IsocQueues_h
#define AppleUSBXHCI_AppleUSBXHCI_IsocQueues_h

#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSObject.h>
#include <IOKit/usb/IOUSBCommand.h>
#include <IOKit/usb/IOUSBControllerListElement.h>
#include <IOKit/IODMACommand.h>

#include "XHCI.h"

enum  
{
	kMaxTransfersPerFrame		= 8,
	kIsocRingSizeinMS			= 100,
	kNumTDSlots					= 128,						// this number should be a power of 2 and larger than isocRingSizeinMS
    kMaxFramesWithoutInterrupt	= 8,
};


class AppleXHCIIsochEndpoint;			// forward declaration

class AppleXHCIIsochTransferDescriptor : public IOUSBControllerIsochListElement
{
    OSDeclareDefaultStructors(AppleXHCIIsochTransferDescriptor)
	
public:
	IOUSBIsocCommand *								command;
	IOByteCount										bufferOffset;
	UInt32											trbIndex[kMaxTransfersPerFrame];			// the index of the first TRB in each outgoing TD
	UInt32											numTRBs[kMaxTransfersPerFrame];				// the total number of (consectutive) TRBs in the outgoing TD
	bool											statusUpdated[kMaxTransfersPerFrame];		// true once this transfer has a returned status
	TRB												eventTRB;									// filled in by the FilterEventRing before calling UpdateFrameList
	bool											newFrame;									// true if this TD does not come on the next frame (frame + 1) after the previous
	bool                                            interruptThisTD;                            // True if we want an interrupt to happen for completing this TD.
	
    // constructor method
    static AppleXHCIIsochTransferDescriptor 	*ForEndpoint(AppleXHCIIsochEndpoint *endpoint);
	
    // virtual methods
    virtual void					SetPhysicalLink(IOPhysicalAddress next);			// must be implemented even though XHCI doesn't use it
    virtual IOPhysicalAddress		GetPhysicalLink(void);								// must be implemented even though XHCI doesn't use it
    virtual IOPhysicalAddress		GetPhysicalAddrWithType(void);						// must be implemented even though XHCI doesn't use it

    virtual IOReturn				UpdateFrameList(AbsoluteTime timeStamp);
    virtual IOReturn				Deallocate(IOUSBControllerV2 *uim);
    virtual void					print(int level);
    
public:
	int					FrameForEventIndex(UInt32 eventIndex);											// which of my TRBs does this one point to

private:
    IOReturn			MungeXHCIIsochTDStatus(UInt32 status, UInt16 *transferLen, UInt32 maxPacketSize, UInt8 direction);
};




class AppleXHCIIsochEndpoint : public IOUSBControllerIsochEndpoint
{
    OSDeclareDefaultStructors(AppleXHCIIsochEndpoint)
	
public:
	virtual bool									init();
	virtual void									free(void);

	void											print(int level);
	AppleXHCIIsochTransferDescriptor *				tdSlots[kNumTDSlots];		// the TDs which have been placed on the ring are stored here
	struct ringStruct *								ring;						// a.k.a. XHCIRing *
	
    volatile AppleXHCIIsochTransferDescriptor *		savedDoneQueueHead;			// saved by the Filter Interrupt routine
    volatile UInt32									producerCount;				// Counter used to synchronize reading of the done queue between filter (producer) and action (consumer)
    volatile UInt32									consumerCount;				// Counter used to synchronize reading of the done queue between filter (producer) and action (consumer)
    IOSimpleLock *									wdhLock;					// used around updates of the producer/consumer counts
	UInt64											lastScheduledFrame;			// keep track of the last frame we sent to the controller
    UInt8                                           maxBurst;                   // for SS endpoints - 1 based
    UInt8											mult;						// how many bursts to do in a microframe - 1 based
	UInt32											ringSizeInPages;			// for the TRB Ring
	UInt16											outSlot;					// compareble to the global iVar in EHCI
	UInt16											maxTRBs;					// the maximum number of TRBs needed to schedule a TD (full frame) on this EP
	UInt8											transactionsPerFrame;		// 8, 4, 2, or 1
	UInt8											msBetweenTDs;				// for EPs with > 1ms interval
	UInt8											xhciInterval;				// the interval for XHCI (the exponent of the power of 2)
	UInt8											speed;						// speed of the device (used in UpdateFrameList)
	bool											waitForRingToRunDry;		// true if we need to wait for the ring to run dry
	volatile bool									ringRunning;				// true once we have rung the doorbell and before we run dry or get stopped
	bool											continuousStream;			// T if the client doesn't really care about frame numbers
};


#endif
