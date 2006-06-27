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

    // unique to UHCI - alignment buffer
	UHCIAlignmentBuffer *						GetAlignmentBuffer();
	void										ReleaseAlignmentBuffer(UHCIAlignmentBuffer*);

    UInt16										functionNumber;
    UInt16										endpointNumber;
    UInt16										speed;
    UInt16										maxPacketSize;
    UInt16										pollingRate;			// For interrupt endpoints.
    UInt8										direction;
    UInt8										type;					// Control, interrupt, etc.
    bool										stalled;
    
    // AbsoluteTime								timestamp;
        
    AppleUHCITransferDescriptor					*firstTD;				// Request queue.
    AppleUHCITransferDescriptor					*lastTD;
    
    queue_head_t								freeBuffers;
    queue_head_t								allocatedBuffers;		// Data blocks for buffering  unaligned transactions.
    int											buffersInUse;
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
    
    struct UHCIAlignmentBuffer					*buffer;				// Buffer for unaligned transactions
	IOUSBCommand								*command;				// the command of which this TD is part
	IOMemoryDescriptor							*logicalBuffer;
	AppleUHCIQueueHead							*pQH;					// the queue head i am on
	bool										lastTDofTransaction;
	
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
    
    struct UHCIAlignmentBuffer					*buffer;				// Buffer for unaligned transactions
	IOMemoryDescriptor							*pBuffer;
	bool										lowLatency;				// is this a LL Isoch transaction
	IOReturn									frStatus;
	
};


class AppleUHCIIsochEndpoint : public IOUSBControllerIsochEndpoint
{
    OSDeclareDefaultStructors(AppleUHCIIsochEndpoint)
	
public:
	virtual bool					init();
	
    // unique to UHCI - alignment buffer
	UHCIAlignmentBuffer *						GetAlignmentBuffer();
	void										ReleaseAlignmentBuffer(UHCIAlignmentBuffer*);
	
	queue_head_t								freeBuffers;
    queue_head_t								allocatedBuffers;		// Data blocks for buffering  unaligned transactions.
    int											buffersInUse;
	
};

#endif