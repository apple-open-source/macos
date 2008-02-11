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



#include <libkern/OSByteOrder.h>

#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMemoryCursor.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>

#include <IOKit/usb/IOUSBLog.h>
#include "AppleUSBEHCI.h"

// Convert USBLog to use kprintf debugging
// The switch is in the header file, but the work is done here because the header is included by the companion controllers
#if EHCI_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...)
__attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= EHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif

#define super IOUSBControllerV3

static IOReturn TranslateStatusToUSBError(UInt32 status);


// Add to USB.h
IOReturn 
AppleUSBEHCI::UIMCreateControlEndpoint(UInt8 functionAddress, UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed)
{
    IOLog("EHCIUIM -- UIMCreateControlEndpoint old version called with no split params\n");
    return(kIOReturnInternalError);
}



IOReturn 
AppleUSBEHCI::UIMCreateControlEndpoint(UInt8 functionAddress, UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed,
									   USBDeviceAddress highSpeedHub, int highSpeedPort)
{
    AppleEHCIQueueHead	*pEHCIEndpointDescriptor;
    AppleEHCIQueueHead	*pED;
	
    USBLog(3, "AppleUSBEHCI[%p]::UIMCreateControlEndpoint(%d, %d, %d, %d @(%d, %d))\n", this,
		   functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort);
	
    if (_rootHubFuncAddress == functionAddress)
    {
        return(kIOReturnSuccess);
    }
	
    if( (speed == kUSBDeviceSpeedLow) && (maxPacketSize > 8) )
    {
		if(functionAddress != 0)
		{
			USBError(1, "AppleUSBEHCI[%p]::UIMCreateControlEndpoint - incorrect max packet size (%d) for low speed", this, maxPacketSize);
			return kIOReturnBadArgument;
		}
		USBLog(3, "AppleUSBEHCI[%p]::UIMCreateControlEndpoint - changing low speed max packet from %d to 8 for dev 0", this, maxPacketSize);
		maxPacketSize = 8;
    }
	
    pED = _AsyncHead;
	
    pEHCIEndpointDescriptor = AddEmptyCBEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, kEHCIEDDirectionTD, pED);
	if (!pEHCIEndpointDescriptor)
	{
		USBError(1, "AppleUSBEHCI[%p]::UIMCreateControlEndpoint - could not add empty endpoint", this);
		return kIOReturnNoResources;
	}

	pEHCIEndpointDescriptor->_queueType = kEHCITypeControl;
    if(pED == NULL)
    {
		_AsyncHead = pEHCIEndpointDescriptor;
		_pEHCIRegisters->AsyncListAddr = HostToUSBLong( pEHCIEndpointDescriptor->_sharedPhysical );
    }
	printAsyncQueue(7, "UIMCreateControlEndpoint");
    return kIOReturnSuccess;
}



AppleEHCIQueueHead * 
AppleUSBEHCI::MakeEmptyEndPoint(
								UInt8 						functionAddress,
								UInt8						endpointNumber,
								UInt16						maxPacketSize,
								UInt8						speed,
								USBDeviceAddress			highSpeedHub,
								int							highSpeedPort,
								UInt8						direction)
{
	UInt32									myFunctionAddress;
	UInt32									myEndPointNumber;
	UInt32									myMaxPacketSize;
	UInt32									mySpeed = 0;
	AppleEHCIQueueHead *					pED;
	EHCIGeneralTransferDescriptorPtr		pTD;
    
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - Addr: %d, EPT#: %d, MPS: %d, speed: %d, hubAddr: %d, hubPort: %d, dir: %d", this, 
		   functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
	
    if( (highSpeedHub == 0) && (speed != kUSBDeviceSpeedHigh) )
    {
		USBLog(1, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - new endpoint NOT fixing up speed", this);
		// speed = kUSBDeviceSpeedHigh;
    }
	
    pED = FindControlBulkEndpoint(functionAddress, endpointNumber, NULL, direction);
    if(pED != NULL)
    {
		USBLog(1, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - old endpoint found, abusing %p", this, pED);
        pED->GetSharedLogical()->flags = 0xffffffff;
    }
	
    pED = AllocateQH();
    
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - new endpoint %p", this, pED);
	
    myFunctionAddress = ((UInt32) functionAddress) << kEHCIEDFlags_FAPhase;
    myEndPointNumber = ((UInt32) endpointNumber) << kEHCIEDFlags_ENPhase;
	
	// Note: unlike in 9, the speed bit patterns between USB.h and the EHCI spec, don't match
    // From USB.h
    //    kUSBDeviceSpeedLow		= 0,		// low speed device
    //    kUSBDeviceSpeedFull		= 1,		// full speed device
    //    kUSBDeviceSpeedHigh		= 2			// high speed device
    // EHCI:
    //   00b Full-Speed (12Mbs)
    //   01b Low-Speed (1.5Mbs)
    //   10b High-Speed (480 Mb/s)
	
    if(speed == kUSBDeviceSpeedFull)
    {
        mySpeed = 0;
    }
    else if(speed == kUSBDeviceSpeedLow)
	{
		mySpeed = 1  << kEHCIEDFlags_SPhase;
	}
    else if(speed == kUSBDeviceSpeedHigh)
	{
		mySpeed = 2  << kEHCIEDFlags_SPhase;
	}
    
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - mySpeed %ld", this, mySpeed);
	
    myMaxPacketSize = ((UInt32) maxPacketSize) << kEHCIEDFlags_MPSPhase;
	
    pED->_direction = direction;
	pED->_functionNumber = functionAddress;
	
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - MPS = %d, setting flags to 0x%lx", this, maxPacketSize, myFunctionAddress | myEndPointNumber | myMaxPacketSize | mySpeed);
    
    pED->GetSharedLogical()->flags = HostToUSBLong(myFunctionAddress | myEndPointNumber | myMaxPacketSize | mySpeed);
	
	
    pED->GetSharedLogical()->splitFlags = HostToUSBLong( (1<< kEHCIEDSplitFlags_MultPhase)	// 1 transaction per traversal
														 | (highSpeedHub << kEHCIEDSplitFlags_HubAddrPhase)
														 | (highSpeedPort <<  kEHCIEDSplitFlags_PortPhase) );
	
    pED->GetSharedLogical()->CurrqTDPtr = 0;
    pED->GetSharedLogical()->AltqTDPtr = HostToUSBLong(kEHCITermFlag);	// Invalid address	
    pED->GetSharedLogical()->qTDFlags = 0;					// this sets the active and halted bits to zero.
    pED->GetSharedLogical()->BuffPtr[0] = 0;	
    pED->GetSharedLogical()->BuffPtr[1] = 0;	
    pED->GetSharedLogical()->BuffPtr[2] = 0;
    pED->GetSharedLogical()->BuffPtr[3] = 0;
    pED->GetSharedLogical()->BuffPtr[4] = 0;
    pED->GetSharedLogical()->extBuffPtr[0] = 0;	
    pED->GetSharedLogical()->extBuffPtr[1] = 0;	
    pED->GetSharedLogical()->extBuffPtr[2] = 0;
    pED->GetSharedLogical()->extBuffPtr[3] = 0;
    pED->GetSharedLogical()->extBuffPtr[4] = 0;
    
	// ??    pED->GetSharedLogical()->qTDFlags = 0;					// this sets the active and halted bits to zero.
	
    // Put in a blank TD now, this will be fetched everytime the endpoint is accessed 
    // so we can use it to drag in a real transaction.
	
    pTD = AllocateTD(); 	// ***** What if this fails?????? see also OHCI
    pTD->pShared->flags = 0;	// make sure active is 0
    pTD->pShared->nextTD = HostToUSBLong(kEHCITermFlag);
    pTD->pShared->altTD = HostToUSBLong(kEHCITermFlag);
    pTD->command = NULL;
    
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - pointing NextqTDPtr to %lx", this, pTD->pPhysical);
    pED->GetSharedLogical()->NextqTDPtr = HostToUSBLong( (UInt32)pTD->pPhysical & ~0x1F);
	
    pED->_qTD = pTD;
    pED->_TailTD =  pED->_qTD;
	
    pED->_responseToStall = 0;
	
    return pED;
}



AppleEHCIQueueHead * 
AppleUSBEHCI::MakeEmptyIntEndPoint(UInt8 			functionAddress,
								   UInt8			endpointNumber,
								   UInt16			maxPacketSize,
								   UInt8			speed,
								   USBDeviceAddress highSpeedHub,
								   int              highSpeedPort,
								   UInt8			direction)
{
    AppleEHCIQueueHead *			intED;
    UInt32							mySMask;
    
    intED = MakeEmptyEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
	
    if (intED == NULL)
		return NULL;
	
	intED->_queueType = kEHCITypeInterrupt;
	
    return intED;
}



void 
AppleUSBEHCI::linkAsyncEndpoint(AppleEHCIQueueHead *CBED, AppleEHCIQueueHead *pEDHead)
{
    IOPhysicalAddress			newHorizPtr;
	
    if(pEDHead == NULL)	// No previous EDs, make this head of queue.
    {
		CBED->GetSharedLogical()->flags |= HostToUSBLong(kEHCIEDFlags_H);
    }
	
    // Point first endpoint to itself
    newHorizPtr = CBED->GetPhysicalAddrWithType();		// Its a queue head
	
    USBLog(7, "AppleUSBEHCI[%p]::linkAsynEndpoint pEDHead %p", this, pEDHead);
	
    if(pEDHead == NULL)
    {
		CBED->SetPhysicalLink(newHorizPtr);
		CBED->_logicalNext = NULL;
    }
    else
    {
		// New endpoints are inserted just after queuehead (not at beginning or end).
		
		// Point new endpoint at same endpoint old queuehead was pointing
		CBED->SetPhysicalLink(pEDHead->GetPhysicalLink());
		CBED->_logicalNext = pEDHead->_logicalNext;
		
		// Point queue head to new endpoint
		pEDHead->_logicalNext = CBED;
		pEDHead->SetPhysicalLink(newHorizPtr);
    }
}



AppleEHCIQueueHead * 
AppleUSBEHCI::AddEmptyCBEndPoint(UInt8					functionAddress,
								 UInt8					endpointNumber,
								 UInt16					maxPacketSize,
								 UInt8					speed,
								 USBDeviceAddress       highSpeedHub,
								 int                  	highSpeedPort,
								 UInt8					direction,
								 AppleEHCIQueueHead *	pED)
{
    AppleEHCIQueueHead *	CBED;
    UInt32					cFlag;
    UInt32					myDataToggleCntrl;
	
    USBLog(7, "AppleUSBEHCI[%p]::AddEmptyCBEndPoint speed %d @(%d, %d)", this, speed, highSpeedHub, highSpeedPort);
	
    CBED = MakeEmptyEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
	
    if (CBED == NULL)
		return(NULL);
	
    cFlag = 0;
    if(kEHCIEDDirectionTD == direction)
    {
		myDataToggleCntrl = 1 << kEHCIEDFlags_DTCPhase;
		if(speed != kUSBDeviceSpeedHigh)
		{
			cFlag = 1 << kEHCIEDFlags_CPhase;
		}
    }
    else
    {
		myDataToggleCntrl = 0;
    }
	
    CBED->GetSharedLogical()->flags |= HostToUSBLong(myDataToggleCntrl | cFlag);
	linkAsyncEndpoint(CBED, pED);
    
    return CBED;
}



IOReturn 
AppleUSBEHCI::UIMCreateControlTransfer(
									   short				functionAddress,
									   short				endpointNumber,
									   IOUSBCompletion			completion,
									   void*				CBP,
									   bool				bufferRounding,
									   UInt32				bufferSize,
									   short				direction)
{
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "AppleUSBEHCI[%p]::UIMCreateControlTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
}



void
AppleUSBEHCI::printAsyncQueue(int level, const char* str)
{
    AppleEHCIQueueHead *pED = _AsyncHead;
    
    if (pED)
    {
		USBLog(level, "AppleUSBEHCI[%p]::printAsyncQueue called from %s", this, str);
		USBLog(level, "--------------------");
		USBLog(level, "AppleUSBEHCI[%p]::printAsyncQueue: _AsyncHead[%p], AsyncListAddr[0x%x]", this, _AsyncHead, USBToHostLong(_pEHCIRegisters->AsyncListAddr));
		while (pED)
		{
			pED->print(level);
			pED = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext);
		}
    }
    else
    {
		USBLog(level, "AppleUSBEHCI[%p]::printAsyncQueue: NULL Async Queue called from %s", this, str);
    }
}



void 
AppleUSBEHCI::printTD(EHCIGeneralTransferDescriptorPtr pTD, int level)
{
    if(pTD == 0)
    {
        USBLog(level, "Attempt to print null TD");
        return;
    }
	USBLog(level, "AppleUSBEHCI[%p]::printTD: ------pTD at %p", this, pTD);
	USBLog(level, "AppleUSBEHCI[%p]::printTD: shared.nextTD:  0x%x", this, USBToHostLong(pTD->pShared->nextTD));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: shared.altTD:   0x%x", this, USBToHostLong(pTD->pShared->altTD));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: shared.flags:   0x%x", this, USBToHostLong(pTD->pShared->flags));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: shared.BuffPtr0: 0x%x", this, USBToHostLong(pTD->pShared->BuffPtr[0]));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: pEndpt:  %p",  this, (pTD->pQH));
	//	USBLog(level, "AppleUSBEHCI[%p]::printED: bufSiz:  %p", this, (UInt32)(pTD->bufferSize));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: pPhysical:   0x%lx", this, (pTD->pPhysical));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: pLogicalNext: %p", this, (pTD->pLogicalNext));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: logicalBuffer:   %p", this, (pTD->logicalBuffer));	
	USBLog(level, "AppleUSBEHCI[%p]::printTD: lastTDofTransaction: %s", this, pTD->lastTDofTransaction ? "TRUE" : "FALSE");	
}



IOReturn 
AppleUSBEHCI::UIMCreateControlTransfer(short					functionAddress,
									   short					endpointNumber,
									   IOUSBCommand*			command,
									   IOMemoryDescriptor*		CBP,
									   bool						bufferRounding,
									   UInt32					bufferSize,
									   short					direction)
{
    AppleEHCIQueueHead *		pEDQueue;
    AppleEHCIQueueHead *		pEDDummy;
    IOReturn					status;
    IOUSBCompletion				completion = command->GetUSLCompletion();
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateControlTransfer adr=%d:%d cbp=%lx:%lx br=%s cback=[%p:%p] dir=%d)",
		   this, functionAddress, endpointNumber, (UInt32)CBP, bufferSize,
		   bufferRounding ? "YES":"NO", completion.target, completion.parameter, direction);
    
    // search for endpoint descriptor
    pEDQueue = FindControlBulkEndpoint(functionAddress, endpointNumber, &pEDDummy, kEHCIEDDirectionTD);
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateControlTransfer -- found endpoint at %p", this, pEDQueue);
	
    if (pEDQueue == NULL)
    {
        USBLog(3, "AppleUSBEHCI[%p]::UIMCreateControlTransfer- Could not find endpoint!", this);
        return kIOUSBEndpointNotFound;
    }

    status = allocateTDs(pEDQueue, command, CBP, bufferSize, direction, true);
	
    if(status == kIOReturnSuccess)
    {
		USBLog(7, "AppleUSBEHCI[%p]::UIMCreateControlTransfer allocateTDS done - CMD = 0x%x, STS = 0x%x", this, USBToHostLong(_pEHCIRegisters->USBCMD), USBToHostLong(_pEHCIRegisters->USBSTS));
		printAsyncQueue(7, "UIMCreateControlTransfer");
		EnableAsyncSchedule(false);
    }
    else
    {
        USBError(1, "AppleUSBEHCI[%p]::UIMCreateControlTransfer - allocateTDs returned error %x", this, status);
    }
    
    return status;
}



AppleEHCIQueueHead * 
AppleUSBEHCI::FindControlBulkEndpoint (short				functionNumber, 
									   short				endpointNumber, 
									   AppleEHCIQueueHead	**pEDBack,
									   short				direction)
{
    UInt32					unique;
    AppleEHCIQueueHead *	pEDQueue;
    AppleEHCIQueueHead *	pEDQueueBack;
    short					EDDirection;
	
    // search for endpoint descriptor
    unique = (UInt32) ((((UInt32) endpointNumber) << kEHCIEDFlags_ENPhase) | ((UInt32) functionNumber));
    
    
    pEDQueue = _AsyncHead;
    pEDQueueBack = NULL;
    if(pEDQueue == NULL)
    {
		USBLog(7, "AppleUSBEHCI[%p]::FindControlBulkEndpoint - pEDQueue is NULL", this);
		return NULL;
    }
    
    do {
		EDDirection = pEDQueue->_direction;
		if( ( (USBToHostLong(pEDQueue->GetSharedLogical()->flags) & kEHCIUniqueNumNoDirMask) == unique) && ( ((EDDirection == kEHCIEDDirectionTD) || (EDDirection) == direction)) ) 
		{
			if (pEDBack)
				*pEDBack = pEDQueueBack;
			return pEDQueue;
		} else 
		{
			pEDQueueBack = pEDQueue;
			pEDQueue = OSDynamicCast(AppleEHCIQueueHead, pEDQueue->_logicalNext);
		}
    } while (pEDQueue != NULL);
    return NULL;
}



IOReturn 
AppleUSBEHCI::allocateTDs(AppleEHCIQueueHead* pEDQueue, IOUSBCommand *command, IOMemoryDescriptor* CBP, UInt32 bufferSize, UInt16 direction, Boolean controlTransaction)
{
	
    EHCIGeneralTransferDescriptorPtr	pTD1, pTD, pTDnew, pTDLast;
    UInt32								myToggle = 0;
    UInt32								myDirection = 0;
    IOByteCount							transferOffset;
    UInt32								pageCount;
    UInt32								flags;
    IOReturn							status = kIOReturnSuccess;
    UInt32								maxPacket;
    UInt32								bytesThisTD, segment;
    UInt32								curTDsegment;
    UInt32								totalPhysLength;
    IOPhysicalAddress					dmaStartAddr;
    UInt32								dmaStartOffset;
	UInt32								dmaAddrHighBits;
    UInt32								bytesToSchedule;
    bool								needNewTD = false;
    UInt32								maxTDLength;
	UInt16								endpoint;
	IODMACommand						*dmaCommand = command->GetDMACommand();
	UInt64								offset;
	IODMACommand::Segment64				segments;
	UInt32								numSegments;
				
	/* *********** Note: Always put the flags in the TD last. ************** */
	/* *********** This is what kicks off the transaction if  ************** */
	/* *********** the next and alt pointers are not set up   ************** */
	/* *********** then the controller will pick up and cache ************** */
	/* *********** crap for the TD.                           ************** */
	
    if(controlTransaction)
    {
		myToggle = 0;	// Use data0 for setup 
		if(direction != kEHCIEDDirectionTD)
		{	// Setup uses Data 0, data & status use Data1 
            myToggle |= kEHCITDFlags_DT;	// use Data1 
		}
    }
	
    myDirection = (UInt32) direction << kEHCITDFlags_PIDPhase;
    maxPacket   =  (USBToHostLong(pEDQueue->GetSharedLogical()->flags)  & kEHCIEDFlags_MPS) >> kEHCIEDFlags_MPSPhase;
    endpoint   =  (USBToHostLong(pEDQueue->GetSharedLogical()->flags)  & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase;
	
	if ( controlTransaction && maxPacket == 0 )
	{
		USBLog(1, "AppleUSBEHCI[%p]::allocateTDs - maxPacket for control endpoint (%d) was 0! - returning kIOReturnNotPermitted", this, endpoint); 
		return kIOReturnNotPermitted;
	}
	
    if ((USBToHostLong(pEDQueue->GetSharedLogical()->qTDFlags) & kEHCITDStatus_Halted) && !(controlTransaction && (endpoint == 0)))
	{
		USBLog(3, "AppleUSBEHCI[%p]::allocateTDs - queue for endpoint (%d) halted - returning kIOUSBPipeStalled", this, endpoint); 
        return kIOUSBPipeStalled;
	}

    // Slight change of plan.
    // I was filling in the tail, then pointing that to a new TD which then
    // became the tail. Repeat until entire buffer is covered.
    // The problem with that is AltNextqTDptr. I was leaving this as invalid
    // in the theory that a short packet would halt the queue and it would 
    // then get kicked back into life by scavengeAnEnpoint. It was abit  tricky
    // to make it point to the new tail TD when you don't know where that's
    // going to be in a multi TD situation.
    
    // New plan. Allocate all the TDs in a chain unlinked to the endpoint
    // then link them in by copying the data from the first new TD to the
    // tail TD, then making the first new TD the tail TD. All along
    // AltNextqTDs will be pointed to this first new TD (qTD1), 
    // Its easy to point to something when you know where it is.
    
    // First allocate the first of the new bunch
    pTD1 = AllocateTD();
	
    if (pTD1 == NULL)
    {
		USBError(1, "AppleUSBEHCI[%p]::allocateTDs can't allocate 1st new TD", this);
		return kIOReturnNoMemory;
    }
    pTD = pTD1;	// We'll be working with pTD
	
	if (CBP && bufferSize)
	{
		if (!dmaCommand)
		{
			USBError(1, "AppleUSBEHCI[%p]::allocateTDs - no dmaCommand", this);
			DeallocateTD(pTD);
			return kIOReturnNoMemory;
		}
		if (dmaCommand->getMemoryDescriptor() != CBP)
		{
			USBError(1, "AppleUSBEHCI[%p]::allocateTDs - mismatched CBP (%p) and dmaCommand memory descriptor (%p)", this, CBP, dmaCommand->getMemoryDescriptor());
			DeallocateTD(pTD);
			return kIOReturnInternalError;
		}
	}
	
    if (bufferSize != 0)
    {	    
        transferOffset = 0;
		curTDsegment = 0;
		bytesThisTD = 0;
        while (transferOffset < bufferSize)
        {
			// first, calculate the maximum possible transfer of the given segment. note that this was already checked for
			// being disjoint, so we don't have to worry about that.
			
			offset = transferOffset;
			numSegments = 1;
			status = dmaCommand->gen64IOVMSegments(&offset, &segments, &numSegments);
			dmaAddrHighBits = (UInt32)(segments.fIOVMAddr >> 32);
			if (status || (numSegments != 1) || (dmaAddrHighBits && !_is64bit))
			{
				USBError(1, "AppleUSBEHCI[%p]::allocateTDs - could not generate segments err (%p) numSegments (%d) fLength (%d)", this, (void*)status, (int)numSegments, (int)segments.fLength);
				status = status ? status : kIOReturnInternalError;
				dmaStartAddr = 0;
				totalPhysLength = 0;
				return kIOReturnInternalError;
			}
			else
			{
				dmaStartAddr = segments.fIOVMAddr;
				totalPhysLength = segments.fLength;
			}			
			
			USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - gen64IOVMSegments returned length of %ld (out of %ld) and start of %p:%p", this, totalPhysLength, bufferSize, (void*)dmaAddrHighBits, (void*)dmaStartAddr);
			dmaStartOffset = (dmaStartAddr & (kEHCIPageSize-1));			
			bytesToSchedule = 0;
			
			// only the first segment can start on a non page boundary
			if ((curTDsegment == 0) || (dmaStartOffset == 0))
			{
                needNewTD = false;
                
				if (totalPhysLength > bufferSize)
				{
					USBLog(5, "AppleUSBEHCI[%p]::allocateTDs - segment physical length > buffer size - truncating", this);
					totalPhysLength = bufferSize;
				}
				// each TD can transfer at most four full pages plus from the initial offset to the end of the first page
				maxTDLength = ((kEHCIPagesPerTD-curTDsegment) * kEHCIPageSize) - dmaStartOffset;
				if (totalPhysLength > maxTDLength)
				{
					if ((curTDsegment == 0) && (dmaStartOffset != 0))
					{
						// truncate this TD to exactly 4 pages, which will always be a multiple of MPS
						USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - segment won't fit - using 4 pages for this TD", this);
						bytesToSchedule = (kEHCIPagesPerTD-1) * kEHCIPageSize;
					}
					else
					{
						// truncate this TD to however many pages are left, which will always be a multiple of MPS
						USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - segment is larger than 1 TD - using %ld pages for this TD", this, kEHCIPagesPerTD - curTDsegment);
						bytesToSchedule = (kEHCIPagesPerTD - curTDsegment) * kEHCIPageSize;
					}
				}
				else
					bytesToSchedule = totalPhysLength;
				
				bytesThisTD += bytesToSchedule;
				transferOffset += bytesToSchedule;
				
                // If our transfer for this TD does not end on a page boundary, we need to close the TD
                //
                if ( (transferOffset < bufferSize) && ((dmaStartOffset+bytesToSchedule) & kEHCIPageOffsetMask) )
                {
                    USBLog(6, "AppleUSBEHCI[%p]::allocateTDs - non-last transfer didn't end at end of page (%ld, %ld)", this, dmaStartOffset, bytesToSchedule);
                    needNewTD = true;
                }
				
                // now schedule all of the bytes I just discovered
				while (bytesToSchedule)
				{
					pTD->pShared->extBuffPtr[curTDsegment] = HostToUSBLong(dmaAddrHighBits);
					pTD->pShared->BuffPtr[curTDsegment++] = HostToUSBLong(dmaStartAddr);
					dmaStartAddr += (kEHCIPageSize - dmaStartOffset);
					if (_is64bit && (dmaStartAddr == 0))
						dmaAddrHighBits++;
					if (bytesToSchedule > (kEHCIPageSize - dmaStartOffset))
						bytesToSchedule -= (kEHCIPageSize - dmaStartOffset);
					else
						bytesToSchedule = 0;
					dmaStartOffset = 0;
				}
				
                if ( ((curTDsegment < kEHCIPagesPerTD) && (transferOffset < bufferSize)) && !needNewTD )
				{
					USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - didn't fill up this TD (segment %ld) - going back for more", this, curTDsegment);
					continue;
				}
			}
			
            flags = kEHCITDioc;				// Want to interrupt on completion
			
			USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - i have %ld bytes in %ld segments", this, bytesThisTD, curTDsegment);
			for (segment = 0; segment < curTDsegment; segment++)
			{
				USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - addr[%ld]:0x%x", this, segment, USBToHostLong(pTD->pShared->BuffPtr[segment]));
			}
			
            flags |= (bytesThisTD << kEHCITDFlags_BytesPhase);
            pTD->pShared->altTD = HostToUSBLong(pTD1->pPhysical);	// point alt to first TD, will be fixed up later
			flags |= myDirection | myToggle | kEHCITDStatus_Active | (3<<kEHCITDFlags_CerrPhase);
			
			// this is for debugging
            // pTD->traceFlag = trace;
            pTD->traceFlag = false;
            pTD->pQH = pEDQueue;
			
			
            // only supply a callback when the entire buffer has been
            // transfered.
	
            // USBLog(5, "AppleUSBEHCI[%p]:new control path for data toggle");
			// adjust data toggle for multi-TD control transfers
			// this will adjust toggle on the next TD since the flags are already set above.
			if(controlTransaction)
			{
				int numPackets = (bytesToSchedule +maxPacket-1)/maxPacket;
				// if number of packets is odd toggle the toggle
				if (numPackets&1) 
				{
					myToggle = myToggle ? 0 : (UInt32)kEHCITDFlags_DT;
				}
			}
			else 
			{
				// toggle is controlled by queue head for bulk/int case
				myToggle = 0;
			}
			
			USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - putting command into TD (%p) on ED (%p)", this, pTD, pEDQueue);
			pTD->command = command;						// Do like OHCI, link to command from each TD
            if (transferOffset >= bufferSize)
            {
				//myToggle = 0;							// Only set toggle on first TD				
				pTD->lastTDofTransaction = true;
				pTD->logicalBuffer = CBP;
				pTD->pShared->flags = HostToUSBLong(flags);
				if ((pEDQueue->_queueType == kEHCITypeControl) || (pEDQueue->_queueType == kEHCITypeBulk))
					_controlBulkTransactionsOut++;
				//if(trace)printTD(pTD);
            }
			else
            {
				pTD->lastTDofTransaction = false;
				//myToggle = 0;	// Only set toggle on first TD
				pTDnew = AllocateTD();
				if (pTDnew == NULL)
				{
					status = kIOReturnNoMemory;
					USBError(1, "AppleUSBEHCI[%p]::allocateTDs can't allocate new TD", this);
				}
				else
				{
					pTD->pShared->nextTD = HostToUSBLong(pTDnew->pPhysical);
					pTD->pLogicalNext = pTDnew;
					pTD->pShared->flags = HostToUSBLong(flags);		// Doesn't matter about flags, not linked in yet
																	// if(trace)printTD(pTD);
					pTD = pTDnew;
					curTDsegment = 0;
					bytesThisTD = 0;
					USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - got another TD - going to fill it up too (%ld, %ld)", this, transferOffset, bufferSize);
				}
            }
        }
    }
    else
    {
		// no buffer to transfer
		pTD->pShared->altTD = HostToUSBLong(pTD1->pPhysical);	// point alt to first TD, will be fixed up later
		USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - (no buffer)- putting command into TD (%p) on ED (%p)", this, pTD, pEDQueue);
		pTD->command = command;
		pTD->lastTDofTransaction = true;
		if ((pEDQueue->_queueType == kEHCITypeControl) || (pEDQueue->_queueType == kEHCITypeBulk))
			_controlBulkTransactionsOut++;
		pTD->logicalBuffer = CBP;
		// pTD->traceFlag = trace;
		pTD->traceFlag = false;
		pTD->pQH = pEDQueue;
		flags = kEHCITDioc | myDirection | myToggle | kEHCITDStatus_Active | (3<<kEHCITDFlags_CerrPhase);
		// if(trace)printTD(pTD);
		pTD->pShared->flags = HostToUSBLong(flags);
    }
	
    pTDLast = pEDQueue->_TailTD;
    pTD->pShared->nextTD = HostToUSBLong(pTD1->pPhysical);
    pTD->pLogicalNext = pTD1;
	
    
    // We now have a new chain of TDs. link it in.
    // pTD1, pointer to first TD
    // pTD, pointer to last TD
    // pTDLast is last currently on endpoint, will be made first
	
    flags = pTD1->pShared->flags;
    pTD1->pShared->flags = 0;
	
    // Copy contents of first TD to old tail
    pTDLast->pShared->nextTD = pTD1->pShared->nextTD;
    pTDLast->pShared->altTD = pTD1->pShared->altTD;
    pTDLast->pShared->flags = 0;
    pTDLast->pShared->BuffPtr[0] = pTD1->pShared->BuffPtr[0];
    pTDLast->pShared->BuffPtr[1] = pTD1->pShared->BuffPtr[1];
    pTDLast->pShared->BuffPtr[2] = pTD1->pShared->BuffPtr[2];
    pTDLast->pShared->BuffPtr[3] = pTD1->pShared->BuffPtr[3];
    pTDLast->pShared->BuffPtr[4] = pTD1->pShared->BuffPtr[4];
    pTDLast->pShared->extBuffPtr[0] = pTD1->pShared->extBuffPtr[0];
    pTDLast->pShared->extBuffPtr[1] = pTD1->pShared->extBuffPtr[1];
    pTDLast->pShared->extBuffPtr[2] = pTD1->pShared->extBuffPtr[2];
    pTDLast->pShared->extBuffPtr[3] = pTD1->pShared->extBuffPtr[3];
    pTDLast->pShared->extBuffPtr[4] = pTD1->pShared->extBuffPtr[4];
    
    pTDLast->pQH = pTD1->pQH;
    USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - transfering command from TD (%p) to TD (%p)", this, pTD1, pTDLast);
    pTDLast->command = pTD1->command;
    pTDLast->lastTDofTransaction = pTD1->lastTDofTransaction;
    //pTDLast->bufferSize = pTD1->bufferSize;
    pTDLast->traceFlag = pTD1->traceFlag;
    pTDLast->pLogicalNext = pTD1->pLogicalNext;
    pTDLast->logicalBuffer = pTD1->logicalBuffer;
    
    // Note not copying
    
    //  flags
    //  unused
    //  index      (pertains to TD)
    //  pPhysical  (pretains to TD)
	
    
    // squash pointers on new tail
    pTD1->pShared->nextTD = HostToUSBLong(kEHCITermFlag);
    pTD1->pShared->altTD = HostToUSBLong(kEHCITermFlag);
    pTD1->pLogicalNext = 0;
    USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - zeroing out command in  TD (%p)", this, pTD1);
    pTD1->command = NULL;
    
    // Point end of new TDs to first TD, now new tail
    pTD->pShared->nextTD = HostToUSBLong(pTD1->pPhysical);
    pTD->pLogicalNext = pTD1;

    // This is (of course) copied from the 9 UIM. It has this cryptic note.
    // **** THIS NEEDS TO BE CHANGED, SEE NOTE
    // we have good status, so let's kick off the machine
    // Make new descriptor the tail
    pEDQueue->_TailTD = pTD1;
    pTDLast->pShared->flags = flags;
	
    if (status)
    {
		USBLog(3, "AppleUSBEHCI[%p] CreateGeneralTransfer: returning status 0x%x", this, status);
    }
    return status;
}



static UInt32
mungeECHIStatus(UInt32 flags, EHCIGeneralTransferDescriptorPtr pHCDoneTD)
{
    AppleEHCIQueueHead * pEndpoint;
	
#if 0
	
	/* Mail from John Howard */
	
	Active CErr Halted XactErr Bytes2Xfer     
		-- These will be no error, halted not set
		
		0     >0    0      0        0      normal (nc)
		0     >0    0      0        >0     short packet (sp)
		0     >0    0      1        >=0    (nc or sp) with 
		one or more retries for
			XactErrors detected
			
			-- These will be stalled, CErr still has some left
			
			0     >0    1      0        >0     assuming no other status bits
			are set, this was a STALL
			response. Bytes2Xfer is a 
			don''t care.
				0     >0    1      1        >0     STALL response (same assumption)
				during some bus transaction
				during the buffer a timeout,
				etc. was encountered
				
				-- Finally we have misc transaction error.
				
				0     0     1      1        >=0    three consecutive bus transaction
				errors (any of bad pid, timeout, 
						data crc, etc.)
				
Also:
				If a transaction transaltor gets an error on an Int or Isoc
				it returns an Err handshake. Then you find:
				
				Ping State/Err == 1
				
				
				If a TT gets 3 errors on a control or bulk transaction you get
				a STALL from the TT. Ugh! how do you cope with that.
					
					
#endif
					
					
					pEndpoint = pHCDoneTD->pQH;
	
    if((flags & kEHCITDStatus_Halted) == 0)
    {
		// transaction completed sucessfully
		// Clear error state flag
		pEndpoint->_responseToStall = 0;
		return	kIOReturnSuccess;
    }
	
    if( (flags & kEHCITDStatus_BuffErr) != 0)
    {	
		// Buffer over or under run error - i.e. the controller could not keep up from or to system memory
		if( (flags & kEHCITDFlags_PID) == (1 << kEHCITDFlags_PIDPhase) )	
		{	
			// An in token
			return kOHCIITDConditionBufferOverrun;
		}
		else
		{	
			// OUT/Status, data going out 
			return kOHCIITDConditionBufferUnderrun;
		}
    }
    
    if ( (flags & kEHCITDStatus_Babble) != 0)
    {
		// Babble means that we had a data overrun
		// Buffer over or under run error
		if( (flags & kEHCITDFlags_PID) == (1 << kEHCITDFlags_PIDPhase) )	
		{	
			// An in token
			return kOHCIITDConditionDataOverrun;
		}
		// for out token, we let the other error processing handle it, since the data over/underrun conditions are IN only
    }
	
    if( (flags & kEHCITDFlags_Cerr) != 0)
    {	
		// A STALL
		// Check endpoint to see if we need to send a not responding instead
		if(  ((USBToHostLong(pEndpoint->GetSharedLogical()->flags) & kEHCIEDFlags_S) >> kEHCIEDFlags_SPhase) != 2)
		{
			// A full/low speed transaction, must be behind a transaction translator
			if( (USBToHostLong(pEndpoint->GetSharedLogical()->splitFlags) & kEHCIEDSplitFlags_SMask) == 0)
			{
				// Not an Int transaction
				// Commute first and every other STALL to a not responding, you can't tell the differernce
				// according to the 2.0 spec where it goes completely off the rails.
				// See section 11.17.1 and appendicies A-14 and A-23
				
				pEndpoint->_responseToStall = 1 - pEndpoint->_responseToStall;	// Switch 0 - 1
				if(pEndpoint->_responseToStall == 1)
				{
					return kOHCIITDConditionStall;
				}
				else
				{
					return kOHCIITDConditionDeviceNotResponding;
				}
			}
		}
		return kOHCIITDConditionStall;
    }	
	
    if( (flags & kEHCITDStatus_XactErr) != 0)
    {
		
		if(  ((USBToHostLong(pEndpoint->GetSharedLogical()->flags) & kEHCIEDFlags_S) >> kEHCIEDFlags_SPhase) != 2)
		{       // A split transaction
			return(kIOUSBHighSpeedSplitError);
		}
		USBLog(5, "mungeEHCIStatus - XactErr - not responding");
		return kOHCIITDConditionDeviceNotResponding;	// Can't tell this from any other transaction err
    }	
    
    if( (flags & kEHCITDStatus_PingState) != 0)			// Int/Isoc gave error to transaction translator
    {
		return kOHCIITDConditionDeviceNotResponding;
    }	
	
    USBLog(1, "mungeECHIStatus condition we're not expecting 0x%lx", flags);
    
    return kOHCIITDConditionCRC;
}



IOReturn 
TranslateStatusToUSBError(UInt32 status)
{
    static const UInt32 statusToErrorMap[] = {
        /* OHCI Error */     /* USB Error */
        /*  0 */		kIOReturnSuccess,
        /*  1 */		kIOUSBCRCErr,
        /*  2 */ 		kIOUSBBitstufErr,
        /*  3 */ 		kIOUSBDataToggleErr,
        /*  4 */ 		kIOUSBPipeStalled,
        /*  5 */ 		kIOReturnNotResponding,
        /*  6 */ 		kIOUSBPIDCheckErr,
        /*  7 */ 		kIOUSBWrongPIDErr,
        /*  8 */ 		kIOReturnOverrun,
        /*  9 */ 		kIOReturnUnderrun,
        /* 10 */ 		kIOUSBReserved1Err,
        /* 11 */ 		kIOUSBReserved2Err,
        /* 12 */ 		kIOUSBBufferOverrunErr,
        /* 13 */ 		kIOUSBBufferUnderrunErr,
        /* 14 */		kIOUSBNotSent1Err,
        /* 15 */		kIOUSBNotSent2Err
    };
	
    if (status > 15)
    {
		if(status == (UInt32) kIOUSBHighSpeedSplitError)
		{
			return(kIOUSBHighSpeedSplitError);
		}
		return kIOReturnInternalError;
    }
    
    return statusToErrorMap[status];
}


IOReturn
AppleUSBEHCI::EHCIUIMDoDoneQueueProcessing(EHCIGeneralTransferDescriptorPtr pHCDoneTD, OSStatus forceErr,  
										   IOUSBCompletionAction safeAction, EHCIGeneralTransferDescriptorPtr stopAt)
{
    UInt32								flags, transferStatus;
    UInt32								bufferSizeRemaining = 0;
    EHCIGeneralTransferDescriptorPtr	nextTD;
    OSStatus							accumErr = kIOReturnSuccess;
	UInt32								currentQueue;
	Boolean								doOnce, doLoop;
					 
	_doneQueueParams[_nextDoneQueue].pHCDoneTD = pHCDoneTD;
	_doneQueueParams[_nextDoneQueue].forceErr = forceErr;
	_doneQueueParams[_nextDoneQueue].safeAction = safeAction;
	_doneQueueParams[_nextDoneQueue].stopAt = stopAt;
	
	doLoop = TRUE;
	doOnce = FALSE;
	currentQueue = 0;
	if (_nextDoneQueue)
	{
		if (_nextDoneQueue < 19)
		{
			USBLog(_nextDoneQueue > 0 ? 3 : 7, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing queued level %ld", this, _nextDoneQueue);
			_nextDoneQueue++;
			return kIOReturnSuccess;
		} else 
		{
			USBLog(3, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing queue buffer overflow (%d)", this, (int)_nextDoneQueue);
			currentQueue = _nextDoneQueue--;
			doOnce = TRUE;
			doLoop = FALSE;
		}
		
	}
	_nextDoneQueue++;
	
    while (doOnce || (doLoop && (currentQueue < _nextDoneQueue)))
	{
		doOnce = FALSE;
	
		USBLog(7, "+AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing (now at level %ld)", this, currentQueue);

		pHCDoneTD = _doneQueueParams[currentQueue].pHCDoneTD;
		forceErr = _doneQueueParams[currentQueue].forceErr;
		safeAction = _doneQueueParams[currentQueue].safeAction;
		stopAt = _doneQueueParams[currentQueue].stopAt;
		currentQueue++;

		bufferSizeRemaining = 0;	// So next queue starts afresh.
		accumErr = kIOReturnSuccess;


		while (pHCDoneTD != NULL)
		{
			IOReturn	errStatus;
			if(pHCDoneTD == stopAt)
			{
				// Don't process this one or any further
				USBLog(5, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing stop at %p", this, pHCDoneTD);
				break;
			}
			
			nextTD	= (EHCIGeneralTransferDescriptorPtr)pHCDoneTD->pLogicalNext;
			flags = USBToHostLong(pHCDoneTD->pShared->flags);
			if (forceErr != kIOReturnSuccess)
			{
				errStatus = forceErr;
			}
			else if (accumErr != kIOReturnSuccess)
			{
				errStatus = accumErr;
			}
			else
			{
				transferStatus = mungeECHIStatus(flags, pHCDoneTD);
				if (transferStatus)
				{
					USBLog(4, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - TD (%p) - got transferStatus 0x%lx with flags (0x%lx)", this, pHCDoneTD, transferStatus, flags);
				}
				errStatus = TranslateStatusToUSBError(transferStatus);
				accumErr = errStatus;
				if (errStatus)
				{
					UInt32	myFlags = USBToHostLong(pHCDoneTD->pQH->GetSharedLogical()->flags);
					USBLog(4, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - got error %s (0x%x), for bus: 0x%lx, addr:  %ld, ep: %ld", this, USBErrorToString(errStatus), errStatus, _busNumber, ((myFlags & kEHCIEDFlags_FA) >> kEHCIEDFlags_FAPhase), ((myFlags & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase));
				}
			}
			
			bufferSizeRemaining += (flags & kEHCITDFlags_Bytes) >> kEHCITDFlags_BytesPhase;
			
			if (pHCDoneTD->lastTDofTransaction)
			{
				if ( pHCDoneTD->command == NULL )
				{
					USBError (1, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing pHCDoneTD->command is NULL (%p)", this, pHCDoneTD);
				}
				else
				{
					IOUSBCompletion completion = pHCDoneTD->command->GetUSLCompletion();
					if(!safeAction || (safeAction == completion.action))
					{
						// remove flag before completing
						pHCDoneTD->lastTDofTransaction = 0;
						
						Complete(completion, errStatus, bufferSizeRemaining);
						if ((pHCDoneTD->pQH->_queueType == kEHCITypeControl) || (pHCDoneTD->pQH->_queueType == kEHCITypeBulk))
						{
							if (!_controlBulkTransactionsOut)
							{
								USBError(1, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - _controlBulkTransactionsOut underrun!", this);
							}
							else
							{
								_controlBulkTransactionsOut--;
								USBLog(7, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - _controlBulkTransactionsOut(%p) pHCDoneTD(%p)", this, (void*)_controlBulkTransactionsOut, pHCDoneTD);
								if (!_controlBulkTransactionsOut)
								{
									USBLog(7, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - no more _controlBulkTransactionsOut - halting AsyncQueue", this);
									DisableAsyncSchedule(false);
								}
							}
						}
					}
					else
					{	
						USBError(1, "The EHCI driver has detected an error [safeAction != NULL]");
					}
				}
				bufferSizeRemaining = 0;									// So next transaction starts afresh.
				accumErr = kIOReturnSuccess;
			}
			pHCDoneTD->logicalBuffer = NULL;
			USBLog(7, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - deallocating TD (%p)", this, pHCDoneTD);
			DeallocateTD(pHCDoneTD);
			pHCDoneTD = nextTD;	// New qHead
		}
	
		USBLog(7, "-AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing", this);
	}
	if (doLoop)
		_nextDoneQueue = 0;  // we've processed all queues
    return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::scavengeIsocTransactions(IOUSBCompletionAction safeAction, bool reQueueTransactions)
{
    IOUSBControllerIsochListElement 	*pDoneEl;
    UInt32								cachedProducer;
    UInt32								cachedConsumer;
    AppleEHCIIsochEndpoint				*pEP;
    IOUSBControllerIsochListElement		*prevEl;
    IOUSBControllerIsochListElement		*nextEl;
    IOInterruptState					intState;
	
    // Get the values of the Done Queue Head and the producer count.  We use a lock and disable interrupts
    // so that the filter routine does not preempt us and updates the values while we're trying to read them.
    //
    intState = IOSimpleLockLockDisableInterrupt( _wdhLock );
    
    pDoneEl = (IOUSBControllerIsochListElement*)_savedDoneQueueHead;
    cachedProducer = _producerCount;
    
    IOSimpleLockUnlockEnableInterrupt( _wdhLock, intState );
    
    cachedConsumer = _consumerCount;
	
    if (pDoneEl && (cachedConsumer != cachedProducer))
    {
		// there is real work to do - first reverse the list
		prevEl = NULL;
		USBLog(7, "AppleUSBEHCI[%p]::scavengeIsocTransactions - before reversal, cachedConsumer = 0x%lx", this, cachedConsumer);
		while (true)
		{
			pDoneEl->_logicalNext = prevEl;
			prevEl = pDoneEl;
			cachedConsumer++;
			if (pDoneEl->_pEndpoint)
			{
				pDoneEl->_pEndpoint->onProducerQ--;
				pDoneEl->_pEndpoint->onReversedList++;
			}
			if ( cachedProducer == cachedConsumer)
				break;
			
			pDoneEl = pDoneEl->_doneQueueLink;
		}
		
		// update the consumer count
		_consumerCount = cachedConsumer;
		
		USBLog(7, "AppleUSBEHCI[%p]::scavengeIsocTransactions - after reversal, cachedConsumer[0x%lx]", this, cachedConsumer);
		// now cachedDoneQueueHead points to the head of the done queue in the right order
		while (pDoneEl)
		{
			nextEl = OSDynamicCast(IOUSBControllerIsochListElement, pDoneEl->_logicalNext);
			pDoneEl->_logicalNext = NULL;
			if (pDoneEl->_pEndpoint)
			{
				pDoneEl->_pEndpoint->onReversedList--;
			}
			USBLog(7, "AppleUSBEHCI[%p]::scavengeIsocTransactions - about to scavenge TD %p", this, pDoneEl);
			scavengeAnIsocTD(pDoneEl, safeAction);
			pDoneEl = nextEl;
		}
    }
    
    // Go through all EP's -- don't do if we are called from abort()
    //
    if ( reQueueTransactions )
    {
        pEP = OSDynamicCast(AppleEHCIIsochEndpoint, _isochEPList);
        while (pEP)
        {
            if (pEP->onReversedList)
			{
                USBLog(1, "AppleUSBEHCI[%p]::scavengeIsocTransactions - EP (%p) still had %ld TDs on the reversed list!!", this, pEP, pEP->onReversedList);
			}
            ReturnIsochDoneQueue(pEP);
            AddIsocFramesToSchedule(pEP);
            pEP = OSDynamicCast(AppleEHCIIsochEndpoint, pEP->nextEP);
        }
    }
    
    return kIOReturnSuccess;
	
}



IOReturn
AppleUSBEHCI::scavengeAnIsocTD(IOUSBControllerIsochListElement *pTD, IOUSBCompletionAction safeAction)
{
    IOUSBControllerIsochEndpoint* 		pEP;
    IOReturn						ret;
    AbsoluteTime					timeStamp;
	
    pEP = pTD->_pEndpoint;
    clock_get_uptime(&timeStamp);
    if(pEP == NULL)
    {
		USBError(1, "AppleUSBEHCI[%p]::scavengeAnIsocEndPoint - could not find endpoint associated with iTD (%p)", this, pTD->_pEndpoint);
    }
    else
    {	
		if (!pTD->_lowLatency)
			ret = pTD->UpdateFrameList(timeStamp);		// TODO - accumulate the return values
		PutTDonDoneQueue(pEP, pTD, true);
    }

#if 0
    if(pTD->_completion.action != NULL)
    {
		ReturnIsocDoneQueue(pEP);
    }
#endif
	
    return(kIOReturnSuccess);
}



IOReturn
AppleUSBEHCI::scavengeAnEndpointQueue(IOUSBControllerListElement *pListElem, IOUSBCompletionAction safeAction)
{
    EHCIGeneralTransferDescriptorPtr	doneQueue = NULL, doneTail= NULL, qHead, qTD, qEnd;
    UInt32								flags, count = 0;
    Boolean								TDisHalted, shortTransfer, foundNextTD, foundAltTD;
    AppleEHCIQueueHead					*pQH;
    
    while( (pListElem != NULL) && (count++ < 150000) )
    {
		pQH = OSDynamicCast(AppleEHCIQueueHead, pListElem);
		if(pQH)
		{
			qTD = qHead = pQH->_qTD;
			qEnd = pQH->_TailTD;
			if (((qTD == NULL) || (qEnd == NULL)) && (qTD != qEnd))
			{
				USBError(1, "The EHCI driver found a device queue with invalid head (%p) or tail (%p) - flags 0x%lx", qTD, qEnd, pQH->GetSharedLogical()->flags);
			}
			TDisHalted = false;
			shortTransfer = false;
			foundNextTD = false;
			foundAltTD = false;
			while( qTD && (qTD != qEnd) && (count++ < 150000) )
			{	
				// This end point has transactions
				flags = USBToHostLong(qTD->pShared->flags);
				if(!TDisHalted && !shortTransfer)
				{
					if((flags & kEHCITDStatus_Active) != 0)
					{	// Command is still alive, go to next queue
						break;
					}
					// check for halted
					TDisHalted = ((flags & kEHCITDStatus_Halted) != 0) ;
					if (!TDisHalted)
					{
						// this TD is not active, and was not halted, so check to see if it was short
						// if so - we can ignore that state of the remaining TDs until the lastTD
						// since the harwdare skipped them
						shortTransfer = ((flags & kEHCITDFlags_Bytes) >> kEHCITDFlags_BytesPhase) ? true : false;
					}
				}
				if (qTD->pPhysical == USBToHostLong(pQH->GetSharedLogical()->NextqTDPtr))
				{
					foundNextTD = true;
				}
				if (qTD->pPhysical == USBToHostLong(pQH->GetSharedLogical()->AltqTDPtr))
				{
					foundAltTD = true;
				}
				if(qTD->lastTDofTransaction)
				{
					// We have the complete command
					
					USBLog(7, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue - TD (%p) on ED (%p)", this, qTD, pQH); 
					if(doneQueue == NULL)
					{
						doneQueue = qHead;
					}
					else
					{
						doneTail->pLogicalNext = qHead;
					}
					doneTail = qTD;
					qTD = qTD->pLogicalNext;					// qTD now points to the next TD AFTER the last TD of the trasnaction
					qHead = qTD;
					doneTail->pLogicalNext = NULL;
					pQH->_qTD = qTD;
					if (qTD == NULL)
					{
						USBError(1, "The EHCI driver found a NULL Transfer Descriptor - Queue flags 0x%lx", pQH->GetSharedLogical()->flags);
						break;
					}
					flags = USBToHostLong(pQH->GetSharedLogical()->qTDFlags);
					if (flags & kEHCITDStatus_Halted)
					{
						if(TDisHalted || shortTransfer)
						{
							flags = USBToHostLong(qTD->pShared->flags);
							if (foundAltTD)
							{
								pQH->GetSharedLogical()->AltqTDPtr = HostToUSBLong(qTD->pPhysical);
							}
							if (foundNextTD)
							{
								pQH->GetSharedLogical()->NextqTDPtr = HostToUSBLong(qTD->pPhysical);
							}
							if((flags & kEHCITDStatus_Active) != 0)	// Next command is still active
							{
								break;
							}
						}
					}
					else
					{
						USBLog(7, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue - not changing live pQH[%p]", this, pQH);
					}
                    // Reset our loop variables
                    //
                    TDisHalted = false;
                    shortTransfer = false;
					foundNextTD = false;
					foundAltTD = false;
                } 
				else
				{
					USBLog(7, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue - looking past TD (%p) on ED (%p)", this, qTD, pQH); 
					qTD = qTD->pLogicalNext;
					if (qTD == NULL)
					{
						USBError(1, "The EHCI driver found a NULL Transfer Descriptor - Queue flags 0x%lx", pQH->GetSharedLogical()->flags);
						break;
					}
				}
			}
		}
		pListElem = (IOUSBControllerListElement*)pListElem->_logicalNext;
    }
    if(doneQueue != NULL)
    {
		EHCIUIMDoDoneQueueProcessing(doneQueue, kIOReturnSuccess, safeAction, NULL);
    }
    if(count > 1000)
    {
		USBLog(1, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue looks like bad ed queue %lx", this, count);
    }
    
    return kIOReturnSuccess;
}



void
AppleUSBEHCI::scavengeCompletedTransactions(IOUSBCompletionAction safeAction)
{
    IOReturn 			err, err1;
    int 			i;
	
    safeAction = 0;
    err = scavengeIsocTransactions(safeAction, true);
    if(err != kIOReturnSuccess)
    {
		USBLog(1, "AppleUSBEHCI[%p]::scavengeCompletedTransactions err isoch list %x", this, err);
    }
	
    if ( _AsyncHead != NULL )
    {
		err = scavengeAnEndpointQueue(_AsyncHead, safeAction);
		if(err != kIOReturnSuccess)
		{
			USBLog(1, "AppleUSBEHCI[%p]::scavengeCompletedTransactions err async queue %x", this, err);
		}
    }
	
    if ( _logicalPeriodicList != NULL )
    {
        for(i= 0; i<kEHCIPeriodicListEntries; i++)
        {
            if(_logicalPeriodicList[i] != NULL)
            {
                err1 = scavengeAnEndpointQueue(_logicalPeriodicList[i], safeAction);
                if(err1 != kIOReturnSuccess)
                {
                    err = err1;
                    USBLog(1, "AppleUSBEHCI[%p]::scavengeCompletedTransactions err periodic queue[%d]:0x%x", this, i, err);
                }
            }
        }
    }
}



IOReturn 
AppleUSBEHCI::UIMCreateControlTransfer(short				functionAddress,
									   short				endpointNumber,
									   IOUSBCommand*		command,
									   void*				CBP,
									   bool					bufferRounding,
									   UInt32				bufferSize,
									   short				direction)
{
    USBLog(1, "AppleUSBEHCI[%p]::UIMCreateControlTransfer- calling the wrong method (buffer)!", this);
    return kIOReturnIPCError;
}



IOReturn 
AppleUSBEHCI::UIMCreateControlTransfer(short				functionAddress,
									   short				endpointNumber,
									   IOUSBCompletion		completion,
									   IOMemoryDescriptor*	CBP,
									   bool					bufferRounding,
									   UInt32				bufferSize,
									   short				direction)
{
    USBLog(1, "AppleUSBEHCI[%p]::UIMCreateControlTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
}



IOReturn
AppleUSBEHCI::UIMCreateBulkEndpoint(UInt8				functionAddress,
									UInt8				endpointNumber,
									UInt8				direction,
									UInt8				speed,
									UInt8				maxPacketSize)
{
    USBLog(1, "AppleUSBEHCI[%p] UIMCreateBulkEndpoint- calling the wrong method!", this);
    return kIOReturnInternalError;
}



IOReturn
AppleUSBEHCI::UIMCreateBulkEndpoint(UInt8				functionAddress,
									UInt8				endpointNumber,
									UInt8				direction,
									UInt8				speed,
									UInt16				maxPacketSize,
									USBDeviceAddress    highSpeedHub,
									int                 highSpeedPort)
{
    AppleEHCIQueueHead 		*pEHCIEndpointDescriptor;
    AppleEHCIQueueHead		*pED;
    
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateBulkEndpoint(adr=%d:%d, max=%d, dir=%d)", this, functionAddress, endpointNumber, maxPacketSize, direction);
	
    if( (direction != kUSBOut) && (direction != kUSBIn) )
    {
		USBLog(1, "AppleUSBEHCI[%p]::UIMCreateBulkEndpoint - wrong direction %d", this, direction);
		return kIOReturnBadArgument;
    }	
	
    if(highSpeedHub == 0)
		speed = kUSBDeviceSpeedHigh;
    else
		speed = kUSBDeviceSpeedFull;
	
    pED = _AsyncHead;
	
    if ((speed == kUSBDeviceSpeedHigh) && (maxPacketSize != 512))
    {	
		// This shouldn't happen any more, this has been fixed.
		USBError(1, "AppleUSBEHCI[%p]::UIMCreateBulkEndpoint: USB 2.0 Spec (5.8.3) converting MPS from %d to 512", this, maxPacketSize);
        maxPacketSize = 512;
    }
	
    pEHCIEndpointDescriptor = AddEmptyCBEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction, pED);
    if (pEHCIEndpointDescriptor == NULL)
	    return kIOReturnNoResources;
	
	pEHCIEndpointDescriptor->_queueType = kEHCITypeBulk;

    if(pED == NULL)
    {
		_AsyncHead = pEHCIEndpointDescriptor;
		_pEHCIRegisters->AsyncListAddr = HostToUSBLong(pEHCIEndpointDescriptor->_sharedPhysical);
    }
	
	printAsyncQueue(7, "UIMCreateBulkEndpoint");
    return kIOReturnSuccess;
	
}



IOReturn 
AppleUSBEHCI::UIMCreateBulkTransfer(IOUSBCommand* command)
{
    AppleEHCIQueueHead 			*pEDQueue;
    AppleEHCIQueueHead 			*pEDDummy;
    IOReturn					status;
    IOMemoryDescriptor*			buffer = command->GetBuffer();
    short						direction = command->GetDirection();
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer - adr=%d:%d(%s) cbp=%lx:%x cback=[%lx:%lx:%lx])", this,
		   command->GetAddress(), command->GetEndpoint(), direction == kUSBIn ? "in" : "out",(UInt32)buffer, (int)command->GetReqCount(), 
		   (UInt32)command->GetUSLCompletion().action, (UInt32)command->GetUSLCompletion().target, (UInt32)command->GetUSLCompletion().parameter);
    
    pEDQueue = FindControlBulkEndpoint(command->GetAddress(), command->GetEndpoint(), &pEDDummy, direction);
    
    if (pEDQueue == NULL)
    {
        USBLog(3, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer- Could not find endpoint for addr(%d) ep (%d)!", this, (int)command->GetAddress(), (int)command->GetEndpoint());
        return kIOUSBEndpointNotFound;
    }
	
	status = allocateTDs(pEDQueue, command, buffer, command->GetReqCount(), direction, false );
    if(status == kIOReturnSuccess)
	{
		USBLog(7, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer allocateTDS done - CMD = 0x%x, STS = 0x%x", this, USBToHostLong(_pEHCIRegisters->USBCMD), USBToHostLong(_pEHCIRegisters->USBSTS));
		EnableAsyncSchedule(false);
	}
    else
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer- allocateTDs returned error %x", this, status);
    }
    
    return status;
	
}


IOReturn 
AppleUSBEHCI::UIMCreateBulkTransfer(short					functionAddress,
									short					endpointNumber,
									IOUSBCompletion			completion,
									IOMemoryDescriptor *	CBP,
									bool					bufferRounding,
									UInt32					bufferSize,
									short					direction)
{
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
}



UInt16
AppleUSBEHCI::validatePollingRate(short rawPollingRate,  short speed, int *offset, UInt16 *bytesAvailable)
{
    UInt16 	pollingRate;
    int 	i;
    UInt16	availableBandwidth[kEHCIMaxPoll];
	
    pollingRate = rawPollingRate;
	
    if(speed == kUSBDeviceSpeedHigh)
    {
		USBLog(5,"AppleUSBEHCI[%p]::validatePollingRate HS pollingRate raw: %d", this, pollingRate);
		if(pollingRate <= 3)
		{
			// this would be a sub frame polling rate (per microframe) and we will only do frame resolution for now
			pollingRate = 1;
		}
		else
		{
			if(pollingRate > 16)
			{
				int	count = 0;
				// this is an illegal polling rate. however, some HS devices have it
				// so we will treat this like an OLD polling rate, i.e. a # of ms to poll at
				// we need to find the lowest power of 2 to accomodate this, just like FS
				USBLog(1,"AppleUSBEHCI[%p]::validatePollingRate: illegal HS polling rate (%d), cooking like FS", this, pollingRate);
				while( (pollingRate >> count) != 1)
					count++;
				
				pollingRate = (1 <<  count);
				USBLog(1,"AppleUSBEHCI[%p]::validatePollingRate: (illegal)new cooked polling rate (%d)", this, pollingRate);
			}
			else
			{
				// the polling rate is in microframes and is the exponent in 2^(rate-1)
				pollingRate = 1 << (pollingRate - 4);
			}
		}
		if(pollingRate > kEHCIMaxPoll)
		{
			pollingRate = kEHCIMaxPoll;
		}
		USBLog(5,"AppleUSBEHCI[%p]::validatePollingRate HS pollingRate cooked: %d", this, pollingRate);
    }
    else
    {
		// full/low speed device
		int	count = 0;
		// pollingRate is good
		USBLog(5,"AppleUSBEHCI[%p]::validatePollingRate LS/FS pollingRate raw: %d", this, pollingRate);
		
		// Find the next lowest power of 2 for the polling rate
		while( (pollingRate >> count) != 1)
			count++;
		
		pollingRate = (1 <<  count);
		
        // The following is a fix for rdar://3645770.  OHCI controllers had a limit on the polling rate that they poll at -- 32ms. Anything above that
        // would get clamped to 32 ms.  The USB spec indicates that the controller should poll at the polling rate OR lower, so, for FS/LS devices 
        // that are attached to a HS Hub, we are going to clamp the polling rate to 32ms so that those devices behave the same as when attached to
        // an OHCI controller.  The following line used to be:  if(pollingRate > 255) and then set the rate to 128.
        // 
		if(pollingRate > 32)
		{
			USBLog(5,"AppleUSBEHCI[%p]::validatePollingRate -  Oops LS/FS pollingRate too high: %d, setting to 128", this, pollingRate);
			pollingRate = 32;
		}
		else if( (pollingRate < 8) && (speed == kUSBDeviceSpeedLow) )
		{
			USBLog(5,"AppleUSBEHCI[%p]::validatePollingRate Oops LS pollingRate too low: %d, setting to 8.", this, pollingRate);
			pollingRate = 8;
		}
		else if(pollingRate == 0)
		{
			USBLog(5,"AppleUSBEHCI[%p]::validatePollingRate Oops HS pollingRate too low: %d, setting to 1.", this, pollingRate);
			pollingRate = 1;
		}
		USBLog(5, "AppleUSBEHCI[%p]::validatePollingRate LS/FS pollingRate cooked: %d", this, pollingRate);
    }
    
    // now that we have the "cooked" polling rate, we need to "balance" the load
    // among the [kEHCIMaxPoll % pollingrate] nodes which might contain this endpoint
    
    for (i=0; i < pollingRate; i++)
		availableBandwidth[i] = kEHCIMaxPeriodicBandwidth;
	
    for (i=0; i < kEHCIMaxPoll; i++)
		if (availableBandwidth[i % pollingRate] > _periodicBandwidth[i])
			availableBandwidth[i % pollingRate] = _periodicBandwidth[i];
	
    *offset = 0;		// start at 0
    *bytesAvailable = 0;
    
    for (i=0; i < pollingRate; i++)
	if (availableBandwidth[i] > *bytesAvailable)
	{
		*offset = i;
		*bytesAvailable = availableBandwidth[i];
	}
		
	return pollingRate;
}



static IOUSBControllerListElement* 
FindIntEDqueue(IOUSBControllerListElement *start, UInt8 pollingRate)
{
    AppleEHCIQueueHead			*pQH;
    IOUSBControllerIsochListElement	*pIsoch = OSDynamicCast(IOUSBControllerIsochListElement, start);
    
    if (pIsoch)
    {
		// the list starts with some Isoch elements and we need to find the end of them
		while (OSDynamicCast(IOUSBControllerIsochListElement, start->_logicalNext))
		{
			pIsoch = OSDynamicCast(IOUSBControllerIsochListElement, start->_logicalNext);
			start = (IOUSBControllerListElement*)start->_logicalNext;
		}
		// now pIsoch and start both point to the final Isoch element in the list
		start = (IOUSBControllerListElement*)start->_logicalNext;
    }
    
    // so either we went through the above loop or pIsoch is NULL. if start is also NULL, then we are 
    // at a list which consists of 0 or more Isoch elements
    if (!start)
		return pIsoch;
	
    pQH = OSDynamicCast(AppleEHCIQueueHead, start);
    
    pollingRate--;
    if(pQH && (pQH->_pollM1 < pollingRate))
    {
		return pIsoch;
    }
	
    while(start->_logicalNext != NULL)
    {
		pQH = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
		if(pQH && (pQH->_pollM1 < pollingRate))
		{
			break;
		}
		start = (IOUSBControllerListElement*)start->_logicalNext;
    }
	
    return start;
}



void  
AppleUSBEHCI::linkInterruptEndpoint(AppleEHCIQueueHead *pEHCIEndpointDescriptor)
{
    short							pollingRate;
    UInt16							maxPacketSize;
    int								offset;
    IOUSBControllerListElement		*pLE;
    UInt32							newHorizPtr;
	
    maxPacketSize = pEHCIEndpointDescriptor->_maxPacketSize;
    offset = pEHCIEndpointDescriptor->_offset;
    pollingRate = pEHCIEndpointDescriptor->_pollM1+1;
	
    USBLog(7, "AppleUSBEHCI[%p]::linkInterruptEndpoint %p rate %d", this, pEHCIEndpointDescriptor, pollingRate);
    newHorizPtr = pEHCIEndpointDescriptor->GetPhysicalAddrWithType();
    while( offset < kEHCIPeriodicListEntries)
    {
		pLE = FindIntEDqueue(_logicalPeriodicList[offset], pollingRate);
		if(pLE == NULL)
		{	// Insert as first in queue
			if(pEHCIEndpointDescriptor->_logicalNext == NULL)
			{
				pEHCIEndpointDescriptor->_logicalNext = _logicalPeriodicList[offset];
				pEHCIEndpointDescriptor->SetPhysicalLink(USBToHostLong(_periodicList[offset]));
			}
			else if (pEHCIEndpointDescriptor->_logicalNext != _logicalPeriodicList[offset])
			{
				USBError(1, "The Apple EHCI driver has found an endpoint with an incorrect link at the begining.");
			}
			
			_logicalPeriodicList[offset] = pEHCIEndpointDescriptor;
			_periodicList[offset] = HostToUSBLong(newHorizPtr);
			USBLog(7, "AppleUSBEHCI[%p]::linkInterruptEndpoint - inserted at top of list %d - next logical (%p) next physical (%p)", this, offset, pEHCIEndpointDescriptor->_logicalNext, (void*)pEHCIEndpointDescriptor->GetPhysicalLink());
		}
		else if (pEHCIEndpointDescriptor != pLE)
		{	// Insert in middle/end of queue
			
			if(pEHCIEndpointDescriptor->_logicalNext == NULL)
			{
				pEHCIEndpointDescriptor->_logicalNext = pLE->_logicalNext;
				pEHCIEndpointDescriptor->SetPhysicalLink(pLE->GetPhysicalLink());
			}
			else if (pEHCIEndpointDescriptor->_logicalNext != pLE->_logicalNext)
			{
				USBError(1, "The Apple EHCI driver has found an endpoint with an incorrect link in the middle.");
			}
			// Point queue element to new endpoint
			pLE->_logicalNext = pEHCIEndpointDescriptor;
			pLE->SetPhysicalLink(newHorizPtr);
			USBLog(7, "AppleUSBEHCI[%p]::linkInterruptEndpoint - inserted into list %d - next logical (%p) next physical (%p)", this, offset, pEHCIEndpointDescriptor->_logicalNext, (void*)pEHCIEndpointDescriptor->GetPhysicalLink());
		}
		else
		{
			// Else was already linked
			USBLog(7, "AppleUSBEHCI[%p]::linkInterruptEndpoint - (%p) already linked into %d (%p)", this, pEHCIEndpointDescriptor, offset, pLE);
		}
		
		// this gets repeated, but we only subtract up to MaxPoll
		if (offset < kEHCIMaxPoll)
			_periodicBandwidth[offset] -= maxPacketSize;
		offset += pollingRate;
    } 
    _periodicEDsInSchedule++;
}



IOReturn 
AppleUSBEHCI::UIMCreateInterruptEndpoint(short		functionAddress,
										 short		endpointNumber,
										 UInt8		direction,
										 short		speed,
										 UInt16		maxPacketSize,
										 short		pollingRate)
{
    USBError(1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - old version called with no split params", this);
    return kIOReturnInternalError;
}


IOReturn 
AppleUSBEHCI::UIMCreateInterruptEndpoint(short					functionAddress,
										 short					endpointNumber,
										 UInt8					direction,
										 short					speed,
										 UInt16					maxPacketSize,
										 short					pollingRate,
										 USBDeviceAddress    	highSpeedHub,
										 int                 	highSpeedPort)
{
    int								offset;
    UInt16							availableBandwidth;
    AppleEHCIQueueHead *			pEHCIEndpointDescriptor;
    IOUSBControllerListElement *	pLE;
    IOUSBControllerListElement *	temp;
    AppleUSBEHCIHubInfo *			hiPtr = NULL;
	short							rawPollingRate = pollingRate;
	
    if (_rootHubFuncAddress == functionAddress)
	{
        return RootHubStartTimer(32);						// 32 ms
	}
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint (%d, %d, %s, %d, %d)", this,
		   functionAddress, endpointNumber, (speed == kUSBDeviceSpeedLow) ? "lo" : "full", maxPacketSize,
		   pollingRate);
	
    if( (speed == kUSBDeviceSpeedLow) && (maxPacketSize > 8) )
    {
		USBLog (1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - incorrect max packet size (%d) for low speed", this, maxPacketSize);
		return kIOReturnBadArgument;
    }
    
    if(pollingRate == 0)
		return kIOReturnBadArgument;
	
    // If the interrupt already exists, then we need to delete it first, as we're probably trying
    // to change the Polling interval via SetPipePolicy().
    //
    pEHCIEndpointDescriptor = FindInterruptEndpoint(functionAddress, endpointNumber, direction, &temp);
    if ( pEHCIEndpointDescriptor != NULL )
    {
        IOReturn ret;
        USBLog(3, "AppleUSBEHCI[%p]: UIMCreateInterruptEndpoint endpoint already existed -- deleting it", this);
        ret = UIMDeleteEndpoint(functionAddress, endpointNumber, direction);
        if ( ret != kIOReturnSuccess)
        {
            USBLog(3, "AppleUSBEHCI[%p]: UIMCreateInterruptEndpoint deleting endpoint returned 0x%x", this, ret);
            return ret;
        }
    }
    else
	{
        USBLog(3, "AppleUSBEHCI[%p]: UIMCreateInterruptEndpoint endpoint does NOT exist", this);
	}
	
    // find my offset and check the HIGH SPEED bandwidth to the hub
    pollingRate = validatePollingRate(pollingRate, speed, &offset, &availableBandwidth);
	
    if (maxPacketSize > availableBandwidth)
    {
		USBLog (1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - no bandwidth", this);
		return kIOReturnNoBandwidth;
    }
    
    if (highSpeedHub)
    {
		// get the "master" hub Info for this hub to check the flags
		hiPtr = AppleUSBEHCIHubInfo::GetHubInfo(&_hsHubs, highSpeedHub, highSpeedPort);
		if (!hiPtr)
		{
			USBLog (1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - no hub in list", this);
			return kIOReturnInternalError;
		}
		
		if (maxPacketSize > hiPtr->AvailableInterruptBandwidth())
		{
			USBLog (1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - split transaction - no bandwidth on hub or port (MPS %d, bw %ld)", this, maxPacketSize, hiPtr->AvailableInterruptBandwidth());
			return kIOReturnNoBandwidth;
		}
    }
    else
    {
		USBLog(5, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - using offset %d with HS bandwidth of %d", this, offset, availableBandwidth);
    }
    
	
    pEHCIEndpointDescriptor = MakeEmptyIntEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
	
    if (pEHCIEndpointDescriptor == NULL)
    {
        USBError(1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - could not create empty endpoint", this);
		return kIOReturnNoResources;
    }
	
	pEHCIEndpointDescriptor->_queueType = kEHCITypeInterrupt;
	
	if (hiPtr)
		hiPtr->AllocateInterruptBandwidth(pEHCIEndpointDescriptor, maxPacketSize);
	else
	{
		if (rawPollingRate < 4)
		{
			// validatePollingRate will have treated this as a 1ms polling rate (every frame) and would have balanced the load
			// accordingly. this code will now deal with the microframe rates..
			USBLog(1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - found a raw polling rate of %d", this, rawPollingRate);
			switch (rawPollingRate)
			{
				case 1:				// every microframe
					pEHCIEndpointDescriptor->GetSharedLogical()->splitFlags |= HostToUSBLong(0x000000FF);
					break;
				case 2:				// every other microframe
					pEHCIEndpointDescriptor->GetSharedLogical()->splitFlags |= HostToUSBLong(0x000000055);
					break;
				case 3:				// twice per frame
					pEHCIEndpointDescriptor->GetSharedLogical()->splitFlags |= HostToUSBLong(0x00000011);
					break;
				default:
					// this should never happen, but just in case..
					pEHCIEndpointDescriptor->GetSharedLogical()->splitFlags |= HostToUSBLong(0x00000001);	// HS interrupt goes on frame 1
					
			}
		}
		else
		{
			// this is a polling rate which is greater than or equal to 1 ms, so we will always put the traffic on microframe 0
			pEHCIEndpointDescriptor->GetSharedLogical()->splitFlags |= HostToUSBLong(0x00000001);	// HS interrupt goes on frame 1
		}
	}
	
    pEHCIEndpointDescriptor->_pollM1 = pollingRate-1;
    pEHCIEndpointDescriptor->_offset = offset;
    pEHCIEndpointDescriptor->_maxPacketSize = maxPacketSize;
	
    if(_greatestPeriod < pollingRate)
    {
		_greatestPeriod = pollingRate;
    }
	
    linkInterruptEndpoint(pEHCIEndpointDescriptor);
	
    USBLog(5, "-AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint", this);
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::UIMCreateIsochEndpoint(short		functionAddress,
									 short		endpointNumber,
									 UInt32		maxPacketSize,
									 UInt8		direction)
{
    USBError(1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint -- old version called with no split params", this);
    return kIOReturnIPCError;
}



IOReturn 
AppleUSBEHCI::UIMCreateIsochEndpoint(short					functionAddress,
									 short					endpointNumber,
									 UInt32					maxPacketSize,
									 UInt8					direction,
									 USBDeviceAddress    	highSpeedHub,
									 int               		highSpeedPort)
{
    USBError(1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint -- old version called with no interval", this);
    return kIOReturnIPCError;
}



IOReturn 
AppleUSBEHCI::UIMCreateIsochEndpoint(short					functionAddress,
									 short					endpointNumber,
									 UInt32					maxPacketSize,
									 UInt8					direction,
									 USBDeviceAddress    	highSpeedHub,
									 int               		highSpeedPort,
									 UInt8					interval)
{
    AppleEHCIIsochEndpoint			*pEP;
    UInt32							curMaxPacketSize;
    AppleUSBEHCIHubInfo				*hiPtr = NULL;
    UInt32							xtraRequest;
	IOReturn						res;
	UInt32							decodedInterval;
    
    // we do not create an isoch endpoint in the hardware itself. isoch transactions are handled by 
    // TDs linked firectly into the Periodic List. There are two types of isoch TDs - split (siTD)
    // and high speed (iTD). This method will allocate an internal data structure which will be used
    // to track bandwidth, etc.
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint(%d, %d, %ld, %d, %d, %d)", this, functionAddress, endpointNumber, maxPacketSize, direction, highSpeedHub, highSpeedPort);
    	
    if (highSpeedHub == 0)
    {
        // Use a UInt16 to keep track of isoc bandwidth, then you can use
        // a pointer to it or to hiPtr->bandwidthAvailable for full speed.
		if ((interval == 0) || (interval >= 128))
		{
			USBError(1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint: bad interval %d", this, interval);
			return kIOReturnBadArgument;
		}
		decodedInterval = (1 << (interval - 1));
    }
    else
    {
        // in this case we have a FS/LS device connected through a HS hub
        hiPtr = AppleUSBEHCIHubInfo::GetHubInfo(&_hsHubs, highSpeedHub, highSpeedPort);
        if (!hiPtr)
        {
            USBLog (1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint - setting new hub info(%p) bandwidth to kUSBMaxIsocFrameReqCount", this, hiPtr);
			return kIOReturnInternalError;
        }
		decodedInterval = interval;
    }
    
	// see if the endpoint already exists - if so, this is a SetPipePolicy
    pEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL));
	
    if (pEP) 
    {
        // this is the case where we have already created this endpoint, and now we are adjusting the maxPacketSize
        //
        USBLog(6,"AppleUSBEHCI[%p]::UIMCreateIsochEndpoint endpoint already exists, attempting to change maxPacketSize to %ld", this, maxPacketSize);
		
		// if this is a Split transaction device, then let the hubInfo code handle the reallocation
		if (hiPtr)
		{
			UInt32	currentMaxPacketSize = pEP->maxPacketSize;
			
			res =  hiPtr->ReallocateIsochBandwidth(pEP, maxPacketSize);
			
			// If we get an error, it means that we successfully deallocated the bandwidth, but could not allocate it
			if ( res != kIOReturnSuccess )
			{
				// Give back the current bandwidth only
				_isochBandwidthAvail += currentMaxPacketSize;
				USBLog(4, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint returned bandwidth %ld, new available: %ld", this, currentMaxPacketSize, _isochBandwidthAvail);	
				
			}
			else
			{
				// Give back the current bandwidth and take back the reallocated one
				_isochBandwidthAvail = _isochBandwidthAvail + currentMaxPacketSize - maxPacketSize;
				USBLog(4, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint returned bandwidth %ld, claiming %ld, new available: %ld", this, currentMaxPacketSize, maxPacketSize, _isochBandwidthAvail);	
			}
			
			return res;
		}
		
		// this is for High Speed devices
        curMaxPacketSize = pEP->maxPacketSize;
        if (maxPacketSize == curMaxPacketSize) 
		{
            USBLog(4, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint maxPacketSize (%ld) the same, no change", this, maxPacketSize);
            return kIOReturnSuccess;
        }
        if (maxPacketSize > curMaxPacketSize) 
		{
            // client is trying to get more bandwidth
            xtraRequest = maxPacketSize - curMaxPacketSize;
			if (xtraRequest > _isochBandwidthAvail)
			{
				USBLog(1,"AppleUSBEHCI[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %ld, available: %ld", this, xtraRequest, _isochBandwidthAvail);
				return kIOReturnNoBandwidth;
			}
			_isochBandwidthAvail -= xtraRequest;
			USBLog(5, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint grabbing additional bandwidth: %ld, new available: %ld", this, xtraRequest, _isochBandwidthAvail);
        } else 
		{
            // client is trying to return some bandwidth
            xtraRequest = curMaxPacketSize - maxPacketSize;
            _isochBandwidthAvail += xtraRequest;
            USBLog(5, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint returning some bandwidth: %ld, new available: %ld", this, xtraRequest, _isochBandwidthAvail);
        }
        pEP->maxPacketSize = maxPacketSize;
		if(maxPacketSize >1024)
		{
			pEP->mult = ((maxPacketSize-1)/1024)+1;
			pEP->oneMPS = (maxPacketSize+(pEP->mult-1))/pEP->mult;
		}
		else
		{
			pEP->mult = 1;
			pEP->oneMPS = maxPacketSize;
		}
		USBLog(5,"AppleUSBEHCI[%p]::UIMCreateIsochEndpoint high speed size %ld, mult %d: %d", this, maxPacketSize, pEP->mult, pEP->oneMPS);
        
        return kIOReturnSuccess;
    }
    
    // we neeed to create a new EP structure
	if (maxPacketSize > (hiPtr ? hiPtr->AvailableIsochBandwidth(direction) : _isochBandwidthAvail)) 
	{
		USBLog(1,"AppleUSBEHCI[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %ld, available: %ld", this, maxPacketSize, (hiPtr ? hiPtr->AvailableIsochBandwidth(direction) : _isochBandwidthAvail));
		return kIOReturnNoBandwidth;
	}
	
    pEP = OSDynamicCast(AppleEHCIIsochEndpoint, CreateIsochronousEndpoint(functionAddress, endpointNumber, direction));
    if (pEP == NULL) 
        return kIOReturnNoMemory;
	
	pEP->highSpeedHub = highSpeedHub;
	pEP->highSpeedPort = highSpeedPort;
	if (hiPtr)
	{
		if (hiPtr->AllocateIsochBandwidth(pEP, maxPacketSize) != kIOReturnSuccess)
		{
			USBError(1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint - could not allocate bandwidth", this);
			return kIOReturnNoBandwidth;
		}
	}
	else
	{
		// This is the High Speed Case
		if(maxPacketSize >1024)
		{
			pEP->mult = ((maxPacketSize-1)/1024)+1;
			pEP->oneMPS = (maxPacketSize+(pEP->mult-1))/pEP->mult;
		}
		else
		{
			pEP->mult = 1;
			pEP->oneMPS = maxPacketSize;
		}
		pEP->maxPacketSize = maxPacketSize;
		USBLog(5,"AppleUSBEHCI[%p]::UIMCreateIsochEndpoint high speed 2 size %ld, mult %d: %d", this, maxPacketSize, pEP->mult, pEP->oneMPS);
	}
    pEP->inSlot = kEHCIPeriodicListEntries+1;
	pEP->interval = decodedInterval;
	pEP->hiPtr = hiPtr;
	
	
    _isochBandwidthAvail -= maxPacketSize;
    USBLog(4, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint success. bandwidth used = %ld, new available: %ld", this, maxPacketSize, _isochBandwidthAvail);	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::AbortIsochEP(AppleEHCIIsochEndpoint* pEP)
{
    UInt32								slot;
    IOReturn							err;
    IOUSBControllerIsochListElement		*pTD;
    AbsoluteTime						timeStamp;
    
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(6, "+AppleUSBEHCI[%p]::AbortIsochEP[%p] - start - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%ld) onToDoList (%ld) todo (%p) deferredTDs (%ld) deferred(%p) scheduledTDs (%ld) onProducerQ (%ld) consumer (%ld) producer (%ld) onReversedList (%ld) onDoneQueue (%ld)  doneQueue (%p)", this, pEP,  _outSlot, pEP->inSlot, pEP->activeTDs, pEP->onToDoList, pEP->toDoList, pEP->deferredTDs, pEP->deferredQueue, pEP->scheduledTDs, pEP->onProducerQ, _consumerCount, _producerCount, pEP->onReversedList, pEP->onDoneQueue, pEP->doneQueue);
	}
	
    USBLog(7, "AppleUSBEHCI[%p]::AbortIsochEP (%p)", this, pEP);
	
    // we need to make sure that the interrupt routine is not processing the periodic list
	_inAbortIsochEP = true;
	pEP->aborting = true;
	
    // DisablePeriodicSchedule();
    clock_get_uptime(&timeStamp);
	
    // now make sure we finish any periodic processing we were already doing (for MP machines)
    while (_filterInterruptActive)
		;
	
    // now scavange any transactions which were already on the done queue, but don't put any new ones onto the scheduled queue, since we
    // are aborting
    err = scavengeIsocTransactions(NULL, false);
    if (err)
    {
		USBLog(1, "AppleUSBEHCI[%p]::AbortIsochEP - err (0x%x) from scavengeIsocTransactions", this, err);
    }
    
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(6, "+AppleUSBEHCI[%p]::AbortIsochEP[%p] - after scavenge - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%ld) onToDoList (%ld) todo (%p) deferredTDs (%ld) deferred(%p) scheduledTDs (%ld) onProducerQ (%ld) consumer (%ld) producer (%ld) onReversedList (%ld) onDoneQueue (%ld)  doneQueue (%p)", this, pEP,  _outSlot, pEP->inSlot, pEP->activeTDs, pEP->onToDoList, pEP->toDoList, pEP->deferredTDs, pEP->deferredQueue, pEP->scheduledTDs, pEP->onProducerQ, _consumerCount, _producerCount, pEP->onReversedList, pEP->onDoneQueue, pEP->doneQueue);
	}
	
    if ((_outSlot < kEHCIPeriodicListEntries) && (pEP->inSlot < kEHCIPeriodicListEntries))
    {
		bool			stopAdvancing = false;
		UInt32			stopSlot;
		
        // now scavenge the remaining transactions on the periodic list
        slot = _outSlot;
		stopSlot = (pEP->inSlot+1) & (kEHCIPeriodicListEntries-1);
        while (slot != stopSlot)
        {
            IOUSBControllerListElement 		*thing;
            IOUSBControllerListElement		*nextThing;
            IOUSBControllerListElement		*prevThing;
			UInt32							nextSlot;
            
			nextSlot = (slot+1) & (kEHCIPeriodicListEntries-1);
            thing = _logicalPeriodicList[slot];
            prevThing = NULL;
			if (thing == NULL && (nextSlot != pEP->inSlot))
				_outSlot = nextSlot;
            while(thing != NULL)
            {
                nextThing = (IOUSBControllerListElement*)thing->_logicalNext;
                pTD = OSDynamicCast(IOUSBControllerIsochListElement, thing);
                if(pTD)
                {
                    if (pTD->_pEndpoint == pEP)
                    {
                        // first unlink it
                        if (prevThing)
                        {
                            prevThing->_logicalNext = thing->_logicalNext;
                            prevThing->SetPhysicalLink(thing->GetPhysicalLink());
							thing = prevThing;															// to cause prevThing to remain unchanged at the bottom of the loop
                        }
                        else
                        {
                            _logicalPeriodicList[slot] = nextThing;
                            _periodicList[slot] = HostToUSBLong(thing->GetPhysicalLink());
							thing = NULL;																// to cause prevThing to remain unchanged (NULL) at the bottom of the loop
							if (nextThing == NULL)
							{
								if (!stopAdvancing)
								{
									USBLog(7, "AppleUSBEHCI[%p]::AbortIsochEP(%p) - advancing _outslot from 0x%x to 0x%lx", this, pEP, _outSlot, nextSlot);
									_outSlot = nextSlot;
								}
								else
								{
									USBLog(6, "AppleUSBEHCI[%p]::AbortIsochEP(%p) - would have advanced _outslot from 0x%x to 0x%lx", this, pEP, _outSlot, nextSlot);
								}
							}
                        }
                        
						// Note:  we will do an IOSleep() after we unlink everything so we don't need to wait here for the processor to be
						// done with the TD
                        
                        err = pTD->UpdateFrameList(timeStamp);											// TODO - accumulate the return values or force abort err
						OSDecrementAtomic( &(pEP->scheduledTDs));
						if ( pEP->scheduledTDs < 0 )
						{
							USBLog(1, "AppleUSBEHCI[%p]::AbortIsochEP (%p) - scheduleTDs is negative! (%ld)", this, pEP, pEP->scheduledTDs);
						}
                        PutTDonDoneQueue(pEP, pTD, true	);
                    }
					else if (pTD->_pEndpoint == NULL)
					{
						USBLog(1, "AppleUSBEHCI[%p]::AbortIsochEP (%p) - NULL endpoint in pTD %p", this, pEP, pTD);
					}
					else
					{
						stopAdvancing = true;
						USBLog(7, "AppleUSBEHCI[%p]::AbortIsochEP (%p) - a different EP in play (%p) - stop advancing", this, pEP, pTD->_pEndpoint);
					}
                }
                else 
                {
                    // only care about Isoch in this list - if we get here we are at the Interrupt list, which means no more Isoch
                    break;
                }
                prevThing = thing;
                thing = nextThing;
            }
            slot = nextSlot;
        }
    }
    
    // now transfer any transactions from the todo list to the done queue
    pTD = GetTDfromToDoList(pEP);
    while (pTD)
    {
		err = pTD->UpdateFrameList(timeStamp);
		PutTDonDoneQueue(pEP, pTD, true);
		pTD = GetTDfromToDoList(pEP);
    }

	if (pEP->scheduledTDs == 0)
	{
		// since we have no Isoch xactions on the endpoint, we can reset the counter
		pEP->firstAvailableFrame = 0;
		pEP->inSlot = kEHCIPeriodicListEntries + 1;    
	}

    // we can go back to processing now
	_inAbortIsochEP = false;
    
    // Workaround for rdar://4383196 where on DART machines, we could get an orphaned TD to DMA AFTER we had completed the IOMD.  
    IOSleep(1);
    
    // EnablePeriodicSchedule();
    
    pEP->accumulatedStatus = kIOReturnAborted;
    ReturnIsochDoneQueue(pEP);
    pEP->accumulatedStatus = kIOReturnSuccess;
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(1, "+AppleUSBEHCI[%p]::AbortIsochEP[%p] - done - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%ld) onToDoList (%ld) todo (%p) deferredTDs (%ld) deferred(%p) scheduledTDs (%ld) onProducerQ (%ld) consumer (%ld) producer (%ld) onReversedList (%ld) onDoneQueue (%ld)  doneQueue (%p)", this, pEP,  _outSlot, pEP->inSlot, pEP->activeTDs, pEP->onToDoList, pEP->toDoList, pEP->deferredTDs, pEP->deferredQueue, pEP->scheduledTDs, pEP->onProducerQ, _consumerCount, _producerCount, pEP->onReversedList, pEP->onDoneQueue, pEP->doneQueue);
	}
	else
	{
		USBLog(6, "-AppleUSBEHCI[%p]::AbortIsochEP[%p] - done - all clean - _outSlot (0x%x), pEP->inSlot (0x%x)", this, pEP, _outSlot, pEP->inSlot);
	}
    
	pEP->aborting = false;
    
    return kIOReturnSuccess;
}



void 
AppleUSBEHCI::returnTransactions(AppleEHCIQueueHead *pED, EHCIGeneralTransferDescriptor *untilThisOne, IOReturn error, bool clearToggle)
{
    EHCIGeneralTransferDescriptorPtr	doneQueue = NULL, doneTail= NULL;
    bool				removedSome = false;
	
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions, pED(%p) until (%p), clearToggle: %d", this, pED, untilThisOne, clearToggle);
    pED->print(7);
	
    if (!(USBToHostLong(pED->GetSharedLogical()->qTDFlags) & kEHCITDStatus_Halted))
    {
		USBError(1, "AppleUSBEHCI[%p]::returnTransactions, pED (%p) NOT HALTED (qTDFlags = 0x%x)", this, pED, USBToHostLong(pED->GetSharedLogical()->qTDFlags));
		// HaltAsyncEndpoint(pED);
    }
    
    if ((pED->_qTD != pED->_TailTD) && (pED->_qTD != untilThisOne))		// There are transactions on this queue
    {
        USBLog(5, "AppleUSBEHCI[%p] returnTransactions: removing TDs", this);
		removedSome = true;
        if(untilThisOne == NULL)
        {
            untilThisOne = pED->_TailTD;	// Return all the transactions on this queue
        }
		doneQueue = pED->_qTD;
		doneTail = doneQueue;
		pED->_qTD = pED->_qTD->pLogicalNext;
		while (pED->_qTD != untilThisOne)
		{
			doneTail->pLogicalNext = pED->_qTD;
			doneTail = pED->_qTD;
			pED->_qTD = pED->_qTD->pLogicalNext;
		}
		doneTail->pLogicalNext = NULL;
		pED->GetSharedLogical()->AltqTDPtr = HostToUSBLong(kEHCITermFlag);	// Invalid address	
        pED->GetSharedLogical()->NextqTDPtr = HostToUSBLong(untilThisOne->pPhysical);
        pED->_qTD = untilThisOne;
    }
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions, pED->qTD flags were %x", this, USBToHostLong(pED->_qTD->pShared->flags));
	pED->_qTD->pShared->flags &= ~HostToUSBLong(kEHCITDStatus_Halted); // clear the halted bit in the first TD
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions, pED->qTD flags now %x", this, USBToHostLong(pED->_qTD->pShared->flags));
	
    // Kill the overlay transaction and leave the EP enabled (NOT halted)
    
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions, pED->qTD (L:%p P:0x%lx) pED->TailTD (L:%p P:0x%lx)", this, pED->_qTD, pED->_qTD->pPhysical, pED->_TailTD, pED->_TailTD->pPhysical);
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions: clearing ED bit, qTDFlags = %x", this, USBToHostLong(pED->GetSharedLogical()->qTDFlags));

    if (clearToggle)
		pED->GetSharedLogical()->qTDFlags = 0;					// Ensure that next TD is fetched (not the ALT) and reset the data toggle
    else
		pED->GetSharedLogical()->qTDFlags &= HostToUSBLong(kEHCITDFlags_DT);	// Ensure that next TD is fetched (not the ALT) but keep the data toggle
   
	if (doneQueue)
    {
		USBLog(5, "AppleUSBEHCI[%p]::returnTransactions: calling back the done queue (after ED is made active)", this);    
		EHCIUIMDoDoneQueueProcessing(doneQueue, error, NULL, NULL);
    }
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions: after bit clear, qTDFlags = %x", this, USBToHostLong(pED->GetSharedLogical()->qTDFlags));    
}



void
AppleUSBEHCI::HaltAsyncEndpoint(AppleEHCIQueueHead *pED, AppleEHCIQueueHead *pEDBack)
{
    // check to see if the endpoint is halted, and if not, delink it from the ASYNC
    // queue, halt it, and put it back
    if (!(USBToHostLong(pED->GetSharedLogical()->qTDFlags) & kEHCITDStatus_Halted))
    {
		// we must halt the endpoint before we can touch it
		// but we want the TD on the list when we go to return the transactions
		// 5-24-05 rdar://4120055
		// make sure we clear the active bit when we set the halted bit - see Table 3-16 in the EHCI spec
		USBLog(6, "AppleUSBEHCI[%p]::HaltAsyncEndpoint - unlinking, halting, and relinking (%p)", this, pED);
		unlinkAsyncEndpoint(pED, pEDBack);
		pED->GetSharedLogical()->qTDFlags |= HostToUSBLong(kEHCITDStatus_Halted);
		pED->GetSharedLogical()->qTDFlags &= ~(HostToUSBLong(kEHCITDStatus_Active));
		linkAsyncEndpoint(pED, _AsyncHead);
    }
}


void
AppleUSBEHCI::HaltInterruptEndpoint(AppleEHCIQueueHead *pED)
{
    // check to see if the endpoint is halted, and if not, delink it from the ASYNC
    // queue, halt it, and put it back
    if (!(USBToHostLong(pED->GetSharedLogical()->qTDFlags) & kEHCITDStatus_Halted))
    {
		// we must halt the endpoint before we can touch it
		// but we want the TD on the list when we go to return the transactions
		// 5-24-05 rdar://4120055
		// make sure we clear the active bit when we set the halted bit - see Table 3-16 in the EHCI spec
		USBLog(6, "AppleUSBEHCI[%p]::HaltInterruptEndpoint - unlinking, halting, and relinking (%p)", this, pED);
		unlinkIntEndpoint(pED);
		pED->GetSharedLogical()->qTDFlags |= HostToUSBLong(kEHCITDStatus_Halted);
		pED->GetSharedLogical()->qTDFlags &= ~(HostToUSBLong(kEHCITDStatus_Active));
		linkInterruptEndpoint(pED);
    }
}


IOReturn 
AppleUSBEHCI::HandleEndpointAbort(short			functionAddress,
								  short			endpointNumber,
								  short			direction,
								  bool			clearToggle)
{
    AppleEHCIQueueHead				*pED;
    AppleEHCIQueueHead				*pEDQueueBack;
    AppleEHCIIsochEndpoint			*piEP;
	static	int						reentrantFlag;
	
	if (reentrantFlag)
	{
		USBLog(1, "AppleUSBEHCI[%p]::HandleEndpointAbort ReEntered!!  FERG", this);
	}
	reentrantFlag = TRUE;	
    
    USBLog(5, "AppleUSBEHCI[%p]::HandleEndpointAbort: Addr: %d, Endpoint: %d,%d, clearToggle: %d", this, functionAddress, endpointNumber, direction, clearToggle);
	
    if (functionAddress == _rootHubFuncAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "AppleUSBEHCI[%p]::HandleEndpointAbort: bad params - endpNumber: %d", this, endpointNumber );
			reentrantFlag = FALSE;	
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDDelete (endpointNumber, direction) in 9
        //
        USBLog(5, "AppleUSBEHCI[%p]::HandleEndpointAbort: Attempting operation on root hub", this);
		reentrantFlag = FALSE;	
        return SimulateEDAbort( endpointNumber, direction);
    }
	
    piEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL));
    if (piEP)
    {
		reentrantFlag = FALSE;	
		return AbortIsochEP(piEP);
    }
    
    pED = FindControlBulkEndpoint (functionAddress, endpointNumber, &pEDQueueBack, direction);
    if(pED != NULL)
    {
		HaltAsyncEndpoint(pED, pEDQueueBack);
		returnTransactions(pED, NULL, kIOUSBTransactionReturned, clearToggle);				// this will unhalt the EP
    }
    else
    {
		pED = FindInterruptEndpoint(functionAddress, endpointNumber, direction, NULL);
		
		if(pED == NULL)
		{
			USBLog(1, "AppleUSBEHCI::HandleEndpointAbort, endpoint not found");
			reentrantFlag = FALSE;	
			return kIOUSBEndpointNotFound;
		}
		HaltInterruptEndpoint(pED);
		returnTransactions(pED, NULL, kIOUSBTransactionReturned, clearToggle);
    }
	
	// this will only be for control, bulk, and interrupt endpoints on the bus, since root hub endpoints
	// and Isoch endpoints will have returned before now
	if ( (pED->GetSharedLogical()->qTDFlags & HostToUSBLong(kEHCITDFlags_DT)) && !clearToggle )
	{
		USBLog(6, "AppleUSBEHCI[%p]::HandleEndpointAbort  Preserving a data toggle of 1 in response to an Abort()!", this);
	}
	
	if (clearToggle)
		pED->GetSharedLogical()->qTDFlags &= HostToUSBLong(~((UInt32)kEHCITDFlags_DT));	// clear the data toggle

	if(USBToHostLong(pED->GetSharedLogical()->qTDFlags) & kEHCITDStatus_Halted)
	{
		USBLog(1, "AppleUSBEHCI::HandleEndpointAbort, QH still halted following returnTransactions!!");
	}

	reentrantFlag = FALSE;	
    return kIOReturnSuccess;
	
}



IOReturn 
AppleUSBEHCI::UIMAbortEndpoint(short functionAddress, short endpointNumber, short direction)
{
    // this is probably not correct, but still waiting clarification on the EHCI spec section 4.8.2
    return HandleEndpointAbort(functionAddress, endpointNumber, direction, false);
}


IOReturn 
AppleUSBEHCI::UIMClearEndpointStall(short functionAddress, short endpointNumber, short direction)
{
    
    USBLog(7, "AppleUSBEHCI[%p]::UIMClearEndpointStall - endpoint %d:%d", this, functionAddress, endpointNumber);
    return  HandleEndpointAbort(functionAddress, endpointNumber, direction, true);
}



AppleEHCIQueueHead *
AppleUSBEHCI::FindInterruptEndpoint(short functionNumber, short endpointNumber, short direction, IOUSBControllerListElement * *pLEBack)
{
    UInt32							unique;
    AppleEHCIQueueHead *			pEDQueue;
    IOUSBControllerListElement *	pListElementBack;
    IOUSBControllerListElement *	pListElem;
    int								i;
	
    unique = (UInt32) ((((UInt32) endpointNumber) << kEHCIEDFlags_ENPhase) | ((UInt32) functionNumber));
    pListElementBack = NULL;
	
    for(i= 0; i < _greatestPeriod; i++)
    {
		pListElem = _logicalPeriodicList[i];
		while ( pListElem != NULL)
		{
			pEDQueue = OSDynamicCast(AppleEHCIQueueHead, pListElem);
			if (pEDQueue)
			{
				if( ( (USBToHostLong(pEDQueue->GetSharedLogical()->flags) & kEHCIUniqueNumNoDirMask) == unique) && ( pEDQueue->_direction == (UInt8)direction) ) 
				{
					if (pLEBack)
						*pLEBack = pListElementBack;
					return  pEDQueue;
				}
			}
			pListElementBack = pListElem;
			pListElem = pListElem->_logicalNext;
		} 
    }
    return  NULL;
}



IOReturn
AppleUSBEHCI::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
    IOReturn					status = kIOReturnSuccess;
    AppleEHCIQueueHead *		pEDQueue;
    IOUSBCompletion				completion = command->GetUSLCompletion();
    IOMemoryDescriptor*			buffer = command->GetBuffer();
    short						direction = command->GetDirection(); // our local copy may change
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateInterruptTransfer - adr=%d:%d cbp=%p:%lx br=%s cback=[%lx:%lx:%lx])", this,  
		   command->GetAddress(), command->GetEndpoint(), command->GetBuffer(), 
		   command->GetBuffer()->getLength(), command->GetBufferRounding()?"YES":"NO", 
		   (UInt32)completion.action, (UInt32)completion.target, 
		   (UInt32)completion.parameter);

    if (_rootHubFuncAddress == command->GetAddress())
    {
		IODMACommand			*dmaCommand = command->GetDMACommand();
		IOMemoryDescriptor		*memDesc = dmaCommand ? (IOMemoryDescriptor*)dmaCommand->getMemoryDescriptor() : NULL;
		
		if (memDesc)
		{
			USBLog(3, "AppleUSBEHCI[%p]::UIMCreateInterruptTransfer - root hub interrupt transfer - clearing unneeded memDesc (%p) from dmaCommand (%p)", this, memDesc, dmaCommand);
			dmaCommand->clearMemoryDescriptor();
		}
		if (command->GetEndpoint() == 1)
		{
			status = RootHubQueueInterruptRead(buffer, command->GetReqCount(), completion);
		}
		else
		{
			Complete(completion, kIOUSBEndpointNotFound, command->GetReqCount());
			status = kIOUSBEndpointNotFound;
		}
        return status;
    }    
	
    pEDQueue = FindInterruptEndpoint(command->GetAddress(), command->GetEndpoint(), direction, NULL);
    if (!pEDQueue)
    {
		USBLog(1, "AppleUSBEHCI[%p]::UIMCreateInterruptTransfer - Endpoint not found", this);
		return kIOUSBEndpointNotFound;
    }
	
    status = allocateTDs(pEDQueue, command, buffer, command->GetBuffer()->getLength(), direction, false );
    if(status != kIOReturnSuccess)
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateInterruptTransfer allocateTDs failed %x", this, status);
    }
    else
	{
		EnablePeriodicSchedule(false);
	}
    return status;
}



IOReturn
AppleUSBEHCI::UIMCreateInterruptTransfer(
										 short				functionAddress,
										 short				endpointNumber,
										 IOUSBCompletion			completion,
										 IOMemoryDescriptor *		CBP,
										 bool				bufferRounding,
										 UInt32				bufferSize,
										 short				direction)
{
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "AppleUSBEHCI[%p]UIMCreateInterruptTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
}




void 
AppleUSBEHCI::unlinkIntEndpoint(AppleEHCIQueueHead * pED)
{
    int								i;
    IOUSBControllerListElement *	pListElem;
    int								maxPacketSize;
    Boolean							foundED = false;
    
    USBLog(7, "+AppleUSBEHCI[%p]::unlinkIntEndpoint(%p)", this, pED);
    
    maxPacketSize   =  (USBToHostLong(pED->GetSharedLogical()->flags)  & kEHCIEDFlags_MPS) >> kEHCIEDFlags_MPSPhase;
    
    for(i= pED->_offset; i < kEHCIPeriodicListEntries; i += pED->_pollM1+1)
    {
		pListElem = OSDynamicCast(IOUSBControllerListElement, _logicalPeriodicList[i]);
		if(pED == pListElem)
		{
			_logicalPeriodicList[i] = pED->_logicalNext;
			_periodicList[i] = HostToUSBLong(pED->GetPhysicalLink());
			foundED = true;
			USBLog(7, "AppleUSBEHCI[%p]::unlinkIntEndpoint- found ED at top of list %d, new logical=%p, new physical=0x%x", this, i, _logicalPeriodicList[i], USBToHostLong(_periodicList[i]));
		}
		else
		{
			while(pListElem != NULL)
			{
				if(pListElem->_logicalNext == pED)
				{
					pListElem->_logicalNext = pED->_logicalNext;
					pListElem->SetPhysicalLink(pED->GetPhysicalLink());
					foundED = true;
					USBLog(6, "AppleUSBEHCI[%p]::unlinkIntEndpoint- found ED in list %d, new logical=%p, new physical=%p", this, i, pED->_logicalNext, (void*)pED->GetPhysicalLink());
					break;
				}
				pListElem = OSDynamicCast(IOUSBControllerListElement, pListElem->_logicalNext);
			}
			if(pListElem == NULL)
			{
				USBLog(2, "AppleUSBEHCI[%p]::unlinkIntEndpoint endpoint not found in list %d", this, i);
			}
			
		}
		if (i < kEHCIMaxPoll)
			_periodicBandwidth[i] += maxPacketSize;
    }
	
    IOSleep(1);				// make sure to clear the period list traversal cache
    pED->_logicalNext = NULL;
    
    if (foundED)
		_periodicEDsInSchedule--;
	
    USBLog(7, "-AppleUSBEHCI[%p]::unlinkIntEndpoint(%p)", this, pED);
}



void 
AppleUSBEHCI::unlinkAsyncEndpoint(AppleEHCIQueueHead * pED, AppleEHCIQueueHead * pEDQueueBack)
{
    UInt32 CMD, STS, count;
	
    if( (pEDQueueBack == NULL) && (pED->_logicalNext == NULL) )
    {
        USBLog(7, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: removing sole endpoint %lx", this, (long)pED);
		// this is the only endpoint in the queue. we will leave list processing disabled
		DisableAsyncSchedule(true);
		printAsyncQueue(7, "unlinkAsyncEndpoint");
		pED->GetSharedLogical()->flags &= ~HostToUSBLong(kEHCIEDFlags_H);
		_AsyncHead = NULL;
		_pEHCIRegisters->AsyncListAddr = 0;
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: removing endpoint from queue %lx",this, (long)pED);
		
		// have to take this out of the queue
		
		if(_AsyncHead == pED)
		{
			// this is the case where we are taking the head of the queue, but it is not the
			// only element left in the queue
			if (pEDQueueBack)
			{
				USBError(1, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: ERROR - pEDQueueBack should be NULL at this point",this );
			}
			// we need to find the last ED in the logical list, so that we can link in the "wrap around" physical pointer
			pEDQueueBack = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext);
			while (pEDQueueBack->_logicalNext)
				pEDQueueBack = OSDynamicCast(AppleEHCIQueueHead, pEDQueueBack->_logicalNext);
			printAsyncQueue(7, "unlinkAsyncEndpoint");
			_AsyncHead = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext);
			_pEHCIRegisters->AsyncListAddr = HostToUSBLong(pED->_logicalNext->_sharedPhysical);
			pEDQueueBack->SetPhysicalLink(pED->_logicalNext->GetPhysicalAddrWithType());
			pED = OSDynamicCast(AppleEHCIQueueHead , pED->_logicalNext); 
			pED->GetSharedLogical()->flags |= HostToUSBLong(kEHCIEDFlags_H);
			printAsyncQueue(7, "unlinkAsyncEndpoint");
		}
		else if(pEDQueueBack != NULL)
		{
			printAsyncQueue(7, "unlinkAsyncEndpoint");
			pEDQueueBack->SetPhysicalLink(pED->GetPhysicalLink());
			pEDQueueBack->_logicalNext = pED->_logicalNext;
			printAsyncQueue(7, "unlinkAsyncEndpoint");
		}
		else
		{
			USBLog(7, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: ED not head, but pEDQueueBack not NULL",this);
		}
		
		// If the async schedule is  enabled,  ring the doorbell, otherwise, just finish
		if ( USBToHostLong(_pEHCIRegisters->USBSTS) & kEHCISTSAsyncScheduleStatus ) 
		{
			// ED is unlinked, now tell controller
			CMD = USBToHostLong(_pEHCIRegisters->USBCMD);
			_pEHCIRegisters->USBCMD = HostToUSBLong(CMD | kEHCICMDAsyncDoorbell);
			
			// Wait for controller to acknowledge
			
			STS = USBToHostLong(_pEHCIRegisters->USBSTS);
			count = 0;
			
			while((STS & kEHCIAAEIntBit) == 0)
			{
				IOSleep(1);
				STS = USBToHostLong(_pEHCIRegisters->USBSTS);
				count++;
				if ( count > 10000)
				{
					// Bail out after 10 seconds
					break;
				}
			};
			
			USBLog(5, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: delayed for %ld",this, count);
			
			// Clear request
			_pEHCIRegisters->USBSTS = HostToUSBLong(kEHCIAAEIntBit);
		}
		else
		{
			USBLog(5, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint  Async schedule was disabled", this);
		}
    }
}	



IOReturn
AppleUSBEHCI::DeleteIsochEP(AppleEHCIIsochEndpoint *pEP)
{
    IOUSBControllerIsochEndpoint* 		curEP, *prevEP;
    UInt32								currentMaxPacketSize;
	
    USBLog(7, "AppleUSBEHCI[%p]::DeleteIsochEP (%p)", this, pEP);
    if (pEP->activeTDs)
    {
		USBLog(6, "AppleUSBEHCI[%p]::DeleteIsochEP- there are still %ld active TDs - aborting", this, pEP->activeTDs);
		AbortIsochEP(pEP);
		if (pEP->activeTDs)
		{
			USBError(1, "AppleUSBEHCI[%p]::DeleteIsochEP- after abort there are STILL %ld active TDs", this, pEP->activeTDs);
		}
    }
    prevEP = NULL;
    curEP = _isochEPList;
    while (curEP)
    {
		if (curEP == pEP)
		{
			if (prevEP)
				prevEP->nextEP = curEP->nextEP;
			else
				_isochEPList = curEP->nextEP;
			break;
		}
		prevEP = curEP;
		curEP = curEP->nextEP;
    }
    
	// Save the current max packet size, as DeallocateIsochBandwidth will set the ep->mps to 0
	currentMaxPacketSize = pEP->maxPacketSize;
	
    if (pEP->highSpeedHub != 0)
    {
        AppleUSBEHCIHubInfo		*hiPtr;
        
        hiPtr = AppleUSBEHCIHubInfo::GetHubInfo(&_hsHubs, pEP->highSpeedHub, pEP->highSpeedPort);
        if (hiPtr)
        {
			hiPtr->DeallocateIsochBandwidth(pEP);
        }
    }
	
    // need to return the bandwidth used
	_isochBandwidthAvail += currentMaxPacketSize;
	USBLog(4, "AppleUSBEHCI[%p]::DeleteIsochEP returned bandwidth %ld, new available: %ld", this, currentMaxPacketSize, _isochBandwidthAvail);	
	
	DeallocateIsochEP(pEP);
	
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::UIMDeleteEndpoint(short functionAddress, short endpointNumber, short direction)
{
    AppleEHCIQueueHead					*pED;
    AppleEHCIQueueHead					*pEDQueueBack;
    AppleEHCIIsochEndpoint				*piEP;
	
    USBLog(7, "AppleUSBEHCI[%p] AppleUSBEHCI::UIMDeleteEndpoint: Addr: %d, Endpoint: %d,%d", this, functionAddress,endpointNumber,direction);
    
    if (functionAddress == _rootHubFuncAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: bad params - endpNumber: %d", this, endpointNumber );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDDelete (endpointNumber, direction) in 9
        //
        USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: Attempting operation on root hub", this);
        return SimulateEDDelete( endpointNumber, direction);
    }
	
    piEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL));
    if (piEP)
		return DeleteIsochEP(piEP);
    
    pED = FindControlBulkEndpoint (functionAddress, endpointNumber, &pEDQueueBack, direction);
    
    if(pED == NULL)
    {
		UInt32					splitFlags;
		USBDeviceAddress		highSpeedHub;
		UInt8					highSpeedPort;
        AppleUSBEHCIHubInfo		*hiPtr;
		
		pED = FindInterruptEndpoint(functionAddress, endpointNumber, direction, NULL);
		if(pED == NULL)
		{
			USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint, endpoint not found", this);
			return kIOUSBEndpointNotFound;
		}
		USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: deleting Int endpoint", this);
		unlinkIntEndpoint(pED);

		splitFlags = USBToHostLong(pED->GetSharedLogical()->splitFlags);
		highSpeedHub = (splitFlags & kEHCIEDSplitFlags_HubAddr) >> kEHCIEDSplitFlags_HubAddrPhase;
		highSpeedPort = (splitFlags & kEHCIEDSplitFlags_Port) >> kEHCIEDSplitFlags_PortPhase;
		if (highSpeedHub)
		{
			hiPtr = AppleUSBEHCIHubInfo::GetHubInfo(&_hsHubs, highSpeedHub, highSpeedPort);
			if (hiPtr)
				hiPtr->DeallocateInterruptBandwidth(pED);
		}
    }
    else
    {
		USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: unlinking async endpoint", this);
		unlinkAsyncEndpoint(pED, pEDQueueBack);
    }
    
    if(pED->_qTD != pED->_TailTD)		// There are transactions on this queue
    {
        USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: removing TDs", this);
        EHCIUIMDoDoneQueueProcessing(pED->_qTD, kIOUSBTransactionReturned, NULL, pED->_TailTD);
        pED->_qTD = pED->_TailTD;
        pED->GetSharedLogical()->NextqTDPtr = HostToUSBLong(pED->_qTD->pPhysical);
    }
	
	if ( pED->_qTD != NULL )
	{
		// I need to delete the dummy TD
		USBLog(6, "AppleUSBEHCI[%p]::UIMDeleteEndpoint - deallocating the dummy TD", this);
		DeallocateTD(pED->_qTD);
		pED->_qTD = NULL;
    }
	
    USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: Deallocating %p", this, pED);
    DeallocateED(pED);
    
    return kIOReturnSuccess;
}

#define GET_NEXT_BUFFPTR()															\
	do {																			\
		offset = transferOffset;													\
		numSegments = 1;															\
		status = dmaCommand->gen64IOVMSegments(&offset, &segments, &numSegments);	\
		dmaAddrHighBits = (UInt32)(segments.fIOVMAddr >> 32);						\
		if (status || (numSegments != 1) || (dmaAddrHighBits && !_is64bit))			\
		  {																			\
			USBError(1, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - could not generate segments err (%p) numSegments (%d) fLength (%d)", this, (void*)status, (int)numSegments, (int)segments.fLength);	\
			status = status ? status : kIOReturnInternalError;						\
			dmaStartAddr = 0;														\
			segLen = 0;																\
			return kIOReturnInternalError;											\
		  }																			\
		else																		\
		  {																			\
			dmaStartAddr = segments.fIOVMAddr;										\
			segLen = segments.fLength;												\
		  }																			\
	} while (0)

#define ADJUST_SEGMENT_LENGTH(OFFSET)												\
	do {																			\
		if(segLen > bufferSize)														\
	  	{																			\
			segLen = bufferSize;													\
	  	}																			\
    	if(segLen > (kEHCIPageSize-OFFSET))											\
      	{																			\
			segLen = kEHCIPageSize-OFFSET;											\
	  	}																			\
	} while (0)

IOReturn
AppleUSBEHCI::CreateHSIsochTransfer(AppleEHCIIsochEndpoint * pEP, IOUSBIsocCommand * command)
{
    UInt32								bufferSize;
    AppleEHCIIsochTransferDescriptor 	*pNewITD = NULL;
    IOByteCount							transferOffset;
    UInt32								buffPtrSlot, transfer, frameNumberIncrease;
    unsigned							i,j;
    UInt32								*buffP, *transactionPtr = 0, *buffHighP;
    UInt32								pageOffset=0;
    UInt32								page;
    UInt32								frames;
    UInt32								trLen;
    IOPhysicalAddress					dmaStartAddr;
	UInt32								dmaAddrHighBits;
    IOByteCount							segLen;
	bool								lowLatency = command->GetLowLatency();
	UInt32								updateFrequency = command->GetUpdateFrequency();
	IOUSBIsocFrame *					pFrames = command->GetFrameList();
	IOUSBLowLatencyIsocFrame *			pLLFrames = (IOUSBLowLatencyIsocFrame *)pFrames;
	UInt32								transferCount = command->GetNumFrames();
	UInt64								frameNumberStart = command->GetStartFrame();
	IODMACommand *						dmaCommand = command->GetDMACommand();
	UInt64								offset;
	IODMACommand::Segment64				segments;
	UInt32								numSegments;
	UInt32								transfersPerTD;
	UInt32								baseTransferIndex;
	UInt32								epInterval;
	IOReturn							status;
	
	epInterval = pEP->interval;
    transferOffset = 0;
	
    // Each frame in the frameList (either "pFrames" or "pLLFrames",
	// depending on whether you're dealing with low-latency), corresponds
	// to a TRANSFER OPPORTUNITY. This is what this outer loop counts through.

	// Our job here is to convert this list of TRANSFERS into a list
	// of TDs. Depending on the value of pEP->interval, there could be
	// anywhere from 1 transfer per TD (in the case of pEP->interval
	// equal to 8 or more), up to 8 transfers per TD (if pEP->interval
	// is equal to 1). Other cases are 2 or 4 transfers per TD (for
	// interval values of 4 and 2, respectively)
	
	// Each transfer will happen on a particular microframe. The TD has entries
	// for 8 transactions (transfers), so Transaction0 will go out in microframe 0, 
	// Transaction7 will go out in microframe 7.

	// So we need a variable to express the ratio of transfers per TD
	// for this request: "transfersPerTD". Note how, once you've
	// got an pEP->interval > 8, it doesn't matter if the interval is 16,
	// or 32, or 1024 -- effectively, you've got 1 active transfer in
	// whatever USB frame that transfer happens to "land on".  In that case, we need to 
	// just update the USB Frame # of the TD (the frameNumberStart) as well as the endpoints
	// frameNumberStart to advance by bInterval / 8 frames.  We also need to then set the epInterval to 8.

	transfersPerTD = (epInterval >= 8 ? 1 : (8/epInterval));
	frameNumberIncrease = (epInterval <= 8 ? 1 : epInterval/8);
	
	USBLog(7,"AppleUSBEHCI[%p]::CreateHSIsochTransfer  transfersPerTD will be %ld, frameNumberIncrease = %ld", this, transfersPerTD, frameNumberIncrease);
	
	if ( frameNumberIncrease > 1 )
	{
		// Now that we have a frameNumberIncrease, set the epInterval to 8 so that our calculations are the same as 
		// if we had an interval of 8
		//
		epInterval = 8;		
	}
	
    // Format all the TDs, attach them to the pseudo endpoint. 
    // let the frame interrupt routine put them in the periodic list
	
	// At this point we would calculate the number of frames to put before each IOC bit.  The updateFrequency is expresed in terms
	// of ms.  On EHCI, we always need to set things up so that we get interrupted every ms.  (In OHCI, we
	// could put 8ms worth of transfers in 1 TD, so we had to look and see if we needed to get interrupted more often
	// than that, so that's where the update frequency comes from).  Thus, in EHCI we just need to make sure
	// that each TD has a transaction with the IOC bit set.
	
	// We must enforce the invariant, "there must always be enough transfers left on the framelist
	// to completely fill the current TD". 99% of the time, this is trivial, since the vast majority
	// of possible values of "pEP->interval" are 8 or greater - which means you only need 1 transfer
	// to "fill" the TD. 
	// It's the cases of pEP->interval equal to 1, 2, or 4 we need to worry about; if "transferCount"
	// is not a multiple of 8, 4, or 2, respectively, the "last" TD won't have its full complement of
	// active transfers.

	if (transferCount % transfersPerTD != 0)
	{
	  USBLog(3,"AppleUSBEHCI[%p]::CreateHSIsochTransfer Isoch -- incomplete final TD in transfer, command->GetNumFrames() is %d, pEP->Interval is %d and so transfersPerTD is %d", 
			 this, (int ) command->GetNumFrames(), (int ) epInterval, (int ) transfersPerTD);
	  return kIOReturnBadArgument;
	}

  	// We iterate over the framelist not "transfer by transfer", but
	// rather "TD by TD". At any given point in this process, the variable
	// "baseTransferIndex" contains the index into the framelist of the first
	// transfer that'll go into the TD we're currently working on. Each
	// time through the loop, we increment this base index by the number
	// of active transfers in each TD ("transfersPerTD").

    for (baseTransferIndex = 0;  baseTransferIndex < transferCount; baseTransferIndex += transfersPerTD)
    {
        //  Get the size of buffer for this TD
        //
	    bufferSize = 0;
		for (transfer = 0; transfer < transfersPerTD; transfer++)
		{
		  UInt32 microFramePayload = (lowLatency ? pLLFrames[baseTransferIndex + transfer].frReqCount : pFrames[baseTransferIndex + transfer].frReqCount);

		  if (microFramePayload > kUSBMaxHSIsocFrameReqCount)
		  {
			USBLog(3,"AppleUSBEHCI[%p]::CreateHSIsochTransfer Isoch frame too big %d", this, (int )microFramePayload);
			return kIOReturnBadArgument;
		  }
				
		  bufferSize += microFramePayload;

		  if(lowLatency)
		  {
			// Make sure our frStatus field has a known value.  This is used by the client to know whether the transfer has been completed or not
			//
			pLLFrames[baseTransferIndex + transfer].frStatus = command->GetIsRosettaClient() ? (IOReturn) OSSwapInt32(kUSBLowLatencyIsochTransferKey) : (IOReturn) kUSBLowLatencyIsochTransferKey;
		  }
		}

        // go ahead and make sure we can grab at least ONE TD, before we lock the buffer	
        //
        pNewITD = AllocateITD();
        USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - new iTD %p", this, pNewITD);
        if (pNewITD == NULL)
        {
            USBLog(1,"AppleUSBEHCI[%p]::CreateHSIsochTransfer Could not allocate a new iTD", this);
            return kIOReturnNoMemory;
        }
		
		// Every TD has some general info
		//
		pNewITD->_lowLatency = lowLatency;
		pNewITD->_framesInTD = 0;
		
		// Now, that we won't bail out because of an error, update our parameter in the endpoint that is used to figure out what 
		// frame # we have scheduled
		//
		pEP->firstAvailableFrame += frameNumberIncrease;
		
        // Set up all the physical page pointers
        //
		buffP = &pNewITD->GetSharedLogical()->bufferPage0;
		buffHighP = &pNewITD->GetSharedLogical()->extBufferPage0;

		for (buffPtrSlot=0; ((buffPtrSlot <= 6) && (bufferSize != 0)); buffPtrSlot++)
		{
			GET_NEXT_BUFFPTR();		// get next values of "segLen" and "dmaStartAddr"

			USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - Addr (0x%lx) Length (%ld) BufferSize (%ld)", this, dmaStartAddr, segLen, bufferSize);
			*(buffP++) = HostToUSBLong( dmaStartAddr & kEHCIPageMask);
			*(buffHighP++) = HostToUSBLong( dmaAddrHighBits );

			if (buffPtrSlot == 0)
			{ 
				// Only need to take the pageOffset into account for the 1st buffer pointer
				pageOffset = dmaStartAddr & kEHCIPageOffsetMask;
				ADJUST_SEGMENT_LENGTH(pageOffset); 
			} 
			else 
			{
				ADJUST_SEGMENT_LENGTH(0); 
			}

			USBLog(7, "AppleUSBEHCI[%p]::CreateHSIochTransfer - getPhysicalSegment returned start of 0x%lx; length:%ld ; Buff Ptr0:%lx", this, dmaStartAddr, segLen, *(buffP-1));

			transferOffset += segLen;
			bufferSize -= segLen;

		}

        // set up all the 'Transaction' section of the TD
        //
        page = 0;
        transactionPtr = &pNewITD->GetSharedLogical()->Transaction0;
		
		for (transfer = 0; transfer < transfersPerTD; transfer++)
		{
			
			pNewITD->_framesInTD++;
			
			trLen = (lowLatency ? pLLFrames[baseTransferIndex + transfer].frReqCount : pFrames[baseTransferIndex + transfer].frReqCount);
			
			USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - baseTransferIndex: %ld, microFrame: %d, frameIndex: %ld, interval %ld", this, baseTransferIndex, (int ) transfer, baseTransferIndex + transfer, epInterval);
			USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - forming transaction length (%ld), pageOffset (0x%lx), page (%ld)", this, trLen, pageOffset, page);
			
			*transactionPtr = HostToUSBLong(kEHCI_ITDStatus_Active |  (trLen<< kEHCI_ITDTr_LenPhase) | 
										  (pageOffset << kEHCI_ITDTr_OffsetPhase) | (page << kEHCI_ITDTr_PagePhase) );
			
			// Advance to the next transacton element, which depends on the interval
			//
			transactionPtr += epInterval;
			
			// Adjust the page and pageOffset for the next transaction
			pageOffset += trLen;
			if (pageOffset > kEHCIPageSize)
			{
				pageOffset -= kEHCIPageSize;
				page++;
			}
		}

		// Now, set the IOC bit for the last transaction in this TD. Note that we index by minus epInterval
		// to reach the transaction that had a TD
		//
		transactionPtr[-epInterval] |= HostToUSBLong(kEHCI_ITDTr_IOC);

		// Finish updating the other fields in the TD
		//
		pNewITD->_pFrames = pFrames;											// Points to the start of the frameList for this request (Used in the callback)
		pNewITD->_frameNumber = frameNumberStart;
		pNewITD->_frameIndex = baseTransferIndex;
		pNewITD->_completion.action = NULL;										// This gets filled in later for the last one.
		pNewITD->_pEndpoint = pEP;
		pNewITD->_requestFromRosettaClient = command->GetIsRosettaClient();
		
		pNewITD->GetSharedLogical()->bufferPage0 |= HostToUSBLong((pEP->functionAddress << kEHCI_ITDBuf_FnAddrPhase) | (pEP->endpointNumber << kEHCI_ITDBuf_EPPhase) );
        pNewITD->GetSharedLogical()->bufferPage1 |= HostToUSBLong( (pEP->oneMPS << kEHCI_ITDBuf_MPSPhase) | ((pEP->direction == kUSBIn) ? (UInt32) kEHCI_ITDBuf_IO : 0) );
        pNewITD->GetSharedLogical()->bufferPage2 |= HostToUSBLong( (pEP->mult << kEHCI_ITDBuf_MultPhase) );
        
		// Debugging aid
		pNewITD->print(7);
		
		// Finally, put the TD on the ToDo list
		//
		PutTDonToDoList(pEP, pNewITD);
		
		// Increase our frameNumberStart each time around the loop;
		//
		frameNumberStart += frameNumberIncrease;
    }
	
	// Add the completion action to the last TD
	//
	USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - in TD (%p) setting _completion action (%p)", this, pNewITD, pNewITD->_completion.action);
	pNewITD->_completion = command->GetUSLCompletion();

	// Add the request to the schedule
	//
	AddIsocFramesToSchedule(pEP);
	EnablePeriodicSchedule(false);

	return kIOReturnSuccess;
}

IOReturn
AppleUSBEHCI::CreateSplitIsochTransfer(AppleEHCIIsochEndpoint * pEP, IOUSBIsocCommand * command)
{
    AppleEHCISplitIsochTransferDescriptor	*pNewSITD=NULL, *pDummySITD=NULL;
    IOByteCount								transferOffset = 0;
    UInt32									bufferSize;
    UInt8									startSplitFlags;
    UInt8									completeSplitFlags = 0;
    UInt8									transactionPosition;
    UInt8									transactionCount;
    UInt8									ioc = 0;
    UInt32									i;
    UInt32									pageOffset;
    IOPhysicalAddress						dmaStartAddr;
	UInt32									dmaAddrHighBits;
    IOByteCount								segLen;
	UInt32									myStatFlags;
	IOPhysicalAddress						prevPhysLink = kEHCITermFlag;
	bool									lowLatency = command->GetLowLatency();
	UInt32									updateFrequency = command->GetUpdateFrequency();
	IOUSBIsocFrame *						pFrames = command->GetFrameList();
	IOUSBLowLatencyIsocFrame *				pLLFrames = (IOUSBLowLatencyIsocFrame *)pFrames;
	UInt32									frameCount = command->GetNumFrames();
	UInt64									frameNumberStart = command->GetStartFrame();
	IODMACommand *							dmaCommand = command->GetDMACommand();
	UInt64									offset;
	IODMACommand::Segment64					segments;
	UInt32									numSegments;
	IOReturn								status;
	
    //
    //  Get the total size of buffer
    //
    bufferSize = 0;
    for ( i = 0; i < frameCount; i++)
    {
        if (!lowLatency)
        {
            if (pFrames[i].frReqCount > pEP->maxPacketSize)
            {
                USBLog(1,"AppleUSBEHCI[%p]::CreateSplitIsochTransfer - Isoch frame (%ld) too big (%d) MPS (%ld)", this, i + 1, pFrames[i].frReqCount, pEP->maxPacketSize);
                return kIOReturnBadArgument;
            }
            bufferSize += pFrames[i].frReqCount;
        } else
        {
            if (pLLFrames[i].frReqCount > kUSBMaxFSIsocEndpointReqCount)
            {
                USBLog(1,"AppleUSBEHCI[%p]::CreateSplitIsochTransfer(LL) - Isoch frame (%ld) too big (%d) MPS (%ld)", this, i + 1, pLLFrames[i].frReqCount, pEP->maxPacketSize);
                return kIOReturnBadArgument;
            }
            bufferSize += pLLFrames[i].frReqCount;
            
			// Make sure our frStatus field has a known value (debugging aid)
            //
            pLLFrames[i].frStatus = command->GetIsRosettaClient() ? (IOReturn) OSSwapInt32(kUSBLowLatencyIsochTransferKey) : (IOReturn) kUSBLowLatencyIsochTransferKey;
        }
    }
	
    // Format all the TDs, attach them to the pseudo endpoint.
    // let the frame interrupt routine put them in the periodic list
	
    // Do this one frame at a time
    for ( i = 0; i < frameCount; i++)
    {
		UInt16		reqCount, reqLeft;
		
        pNewSITD = AllocateSITD();
		if (lowLatency)
			reqCount = pLLFrames[i].frReqCount;
		else
			reqCount = pFrames[i].frReqCount;
	    
        USBLog(7, "AppleUSBEHCI[%p]::CreateSplitIsochTransfer - new iTD %p size (%d)", this, pNewSITD, reqCount);
        if (!pNewSITD)
        {
            USBLog(1,"AppleUSBEHCI[%p]::CreateSplitIsochTransfer Could not allocate a new iTD", this);
            return kIOReturnNoMemory;
        }
		
		pEP->firstAvailableFrame++;
        pNewSITD->_lowLatency = lowLatency;
		
		// set up the physical page pointers
		
		offset = transferOffset;
		numSegments = 1;
		status = dmaCommand->gen64IOVMSegments(&offset, &segments, &numSegments);
		dmaAddrHighBits = (UInt32)(segments.fIOVMAddr >> 32);
		if (status || (numSegments != 1) || (dmaAddrHighBits && !_is64bit))
		{
			USBError(1, "AppleUSBEHCI[%p]::CreateSplitIsochTransfer - could not generate segments err (%p) numSegments (%d) fLength (%d)", this, (void*)status, (int)numSegments, (int)segments.fLength);
			status = status ? status : kIOReturnInternalError;
			dmaStartAddr = 0;
			segLen = 0;
		}
		else
		{
			dmaStartAddr = segments.fIOVMAddr;
			segLen = segments.fLength;
		}			
		
		pageOffset = dmaStartAddr & kEHCIPageOffsetMask;
		if(segLen > reqCount)
		{
			segLen = reqCount;
		}
		if(segLen > (kEHCIPageSize-pageOffset))
		{
			segLen = kEHCIPageSize-pageOffset;
		}
		pNewSITD->GetSharedLogical()->buffPtr0 = HostToUSBLong(dmaStartAddr);
		pNewSITD->GetSharedLogical()->extBuffPtr0 = HostToUSBLong(dmaAddrHighBits);
		USBLog(7, "AppleUSBEHCI[%p]::CreateSplitIocTransfer - gen64IOVMSegments returned start of %p:%p; length %ld", this, (void*)dmaAddrHighBits, (void*)dmaStartAddr, segLen);
		
		transferOffset += segLen;
		bufferSize -= segLen;
        reqLeft = reqCount - segLen;
		
		if(reqLeft==0)
		{
			pNewSITD->GetSharedLogical()->buffPtr1 = 0;
            pNewSITD->GetSharedLogical()->extBuffPtr1 = 0;
		}
		else
		{
			offset = transferOffset;
			numSegments = 1;
			status = dmaCommand->gen64IOVMSegments(&offset, &segments, &numSegments);
			dmaAddrHighBits = (UInt32)(segments.fIOVMAddr >> 32);
			if (status || (numSegments != 1) || (dmaAddrHighBits && !_is64bit))
			{
				USBError(1, "AppleUSBEHCI[%p]::CreateSplitIsochTransfer - could not generate segments err (%p) numSegments (%d) fLength (%d)", this, (void*)status, (int)numSegments, (int)segments.fLength);
				status = status ? status : kIOReturnInternalError;
				dmaStartAddr = 0;
				segLen = 0;
			}
			else
			{
				dmaStartAddr = segments.fIOVMAddr;
				segLen = segments.fLength;
			}			
			
			pNewSITD->GetSharedLogical()->buffPtr1 = HostToUSBLong(dmaStartAddr & kEHCIPageMask);
            pNewSITD->GetSharedLogical()->extBuffPtr1 = HostToUSBLong(dmaAddrHighBits);
			if(segLen > reqLeft)
			{
				segLen = reqLeft;
			}
			if(segLen > kEHCIPageSize)
			{
				segLen = kEHCIPageSize;
			}
			USBLog(7, "AppleUSBEHCI[%p]::CreateSplitIocTransfer - gen64IOVMSegments returned start of %p:%p; length %ld", this, (void*)dmaAddrHighBits, (void*)dmaStartAddr, segLen);
			transferOffset += segLen;
			bufferSize -= segLen;
		}
		
        pNewSITD->_pFrames = pFrames;
        pNewSITD->_frameNumber = frameNumberStart + i;
		pNewSITD->_frameIndex = i;
        
		// NEW 11-15-04 SS and CS calculated by AllocateIsochBandwidth
		if (pEP->direction == kUSBOut)
		{
			completeSplitFlags = 0;				// dont use complete split for OUT transactions
			if (reqCount > kUSBEHCIMaxSSOUTsection)
			{
				transactionCount = (reqCount + (kUSBEHCIMaxSSOUTsection-1)) / kUSBEHCIMaxSSOUTsection;					// number of 180 byte transfers
				transactionPosition = 1;							// beginning of a multi-part transfer
				startSplitFlags = pEP->startSplitFlags;				// bitmask of bits to send SSplit on (created in HubInfo.cpp)
			}
			else
			{
				startSplitFlags = pEP->startSplitFlags;				// bitmask of bits to send SSplit on (created in HubInfo.cpp)
				transactionPosition = 0;							// total transfer is in this microframe
				transactionCount = 1;								// only need one transfer
			}
		}
		else
		{
			// IN transactions
			startSplitFlags = pEP->startSplitFlags;						// issue the SSplit on microframe 0
			transactionPosition = 0;									// only used for OUT
			transactionCount = 0;										// only used for OUT
			completeSplitFlags = pEP->completeSplitFlags;				// allow completes on 2,3,4,5,6,7
			USBLog(7, "AppleUSBEHCI[%p]::CreateSplitIsochTransfer IN - SS (%x) CS (%x)", this, startSplitFlags, completeSplitFlags);
		}
        
        // calculate IOC and completion if necessary
		if (i == (frameCount-1))
		{
			ioc = 1;													// always ioc on the last TD and the dummy TD rdar://4300892

			// only put the completion in the last frame if we are not using the backPtr
			if (!pEP->useBackPtr)
				pNewSITD->_completion = command->GetUSLCompletion();
		}
		else if (lowLatency)
		{
			if (!updateFrequency)
				ioc = (((i+1) % 8) == 0) ? 1 : 0;
			else
				ioc = (((i+1) % updateFrequency) == 0) ? 1 : 0;
		}
		else
			ioc = 0;
		
        pNewSITD->GetSharedLogical()->nextSITD = HostToUSBLong(kEHCITermFlag); 
        pNewSITD->GetSharedLogical()->routeFlags = HostToUSBLong(((pEP->direction == kUSBOut) ? 0 : (1 << kEHCIsiTDRouteDirectionPhase))
																 |	(pEP->highSpeedPort <<  kEHCIsiTDRoutePortNumberPhase)
																 |	(pEP->highSpeedHub <<  kEHCIsiTDRouteHubAddrPhase)
																 |	(pEP->endpointNumber <<  kEHCIsiTDRouteEndpointPhase)
																 |	(pEP->functionAddress << kEHCIsiTDRouteDeviceAddrPhase)
																 );
		
        pNewSITD->GetSharedLogical()->timeFlags = HostToUSBLong((completeSplitFlags << kEHCIsiTDTimeCMaskPhase) 
																| 	(startSplitFlags << kEHCIsiTDTimeSMaskPhase)
																);
		
		myStatFlags = (ioc << kEHCIsiTDStatIOCPhase) | (reqCount << kEHCIsiTDStatLengthPhase) |	kEHCIsiTDStatStatusActive;
		if ((i > 0) && pEP->useBackPtr)
		{
			myStatFlags |= kEHCUsiTDStatStatusSplitXState;			// every TD after the first starts in DoCompleteSplit state
			pNewSITD->GetSharedLogical()->backPtr = HostToUSBLong(prevPhysLink);
		}
		else
		{
			pNewSITD->GetSharedLogical()->backPtr = HostToUSBLong(kEHCITermFlag);
		}
		
		prevPhysLink = pNewSITD->_sharedPhysical;
		pNewSITD->GetSharedLogical()->statFlags = HostToUSBLong(myStatFlags);
		
        pNewSITD->GetSharedLogical()->buffPtr1 |= HostToUSBLong(	// Buff pointer already set up
			(transactionPosition << kEHCIsiTDBuffPtr1TPPhase)
			|	(transactionCount));
		
		pNewSITD->_pEndpoint = pEP;
		pNewSITD->_requestFromRosettaClient = command->GetIsRosettaClient();

		PutTDonToDoList(pEP, pNewSITD);
    }
	// if we are wrapping around, we need to add one more link to wrap things up
	if (pEP->useBackPtr)
	{
        pDummySITD = AllocateSITD();
		// most of the fields get copied from the last SITD
        pDummySITD->GetSharedLogical()->nextSITD = HostToUSBLong(kEHCITermFlag);
		pDummySITD->GetSharedLogical()->routeFlags = pNewSITD->GetSharedLogical()->routeFlags;
		pDummySITD->GetSharedLogical()->timeFlags = HostToUSBLong((completeSplitFlags & 0x03) << kEHCIsiTDTimeCMaskPhase);		// no SS on the dummy TD and CS only on microframes 0 and 1
		pDummySITD->GetSharedLogical()->statFlags = HostToUSBLong(kEHCIsiTDStatIOC | kEHCIsiTDStatStatusActive | kEHCUsiTDStatStatusSplitXState);
		pDummySITD->GetSharedLogical()->buffPtr0 = pNewSITD->GetSharedLogical()->buffPtr0;
		pDummySITD->GetSharedLogical()->buffPtr1 = pNewSITD->GetSharedLogical()->buffPtr1;
		pDummySITD->GetSharedLogical()->extBuffPtr0 = pNewSITD->GetSharedLogical()->extBuffPtr0;
		pDummySITD->GetSharedLogical()->extBuffPtr1 = pNewSITD->GetSharedLogical()->extBuffPtr1;
		pDummySITD->GetSharedLogical()->backPtr = HostToUSBLong(prevPhysLink);
		pDummySITD->_completion = command->GetUSLCompletion();
		pDummySITD->_pEndpoint = pEP;
        pDummySITD->_pFrames = pFrames;
        pDummySITD->_frameNumber = pNewSITD->_frameNumber+1;
		pDummySITD->_isDummySITD = true;
		PutTDonToDoList(pEP, pDummySITD);
	}
	AddIsocFramesToSchedule(pEP);
	EnablePeriodicSchedule(false);

	return kIOReturnSuccess;
}


// this is the non Low Latency case
IOReturn
AppleUSBEHCI::UIMCreateIsochTransfer(	short				functionAddress,
										short				endpointNumber,
										IOUSBIsocCompletion completion,
										UInt8				direction,
										UInt64				frameStart,
										IOMemoryDescriptor 	*pBuffer,
										UInt32				frameCount,
										IOUSBIsocFrame 		*pFrames)
{
	USBError(1, "AppleUSBEHCI::UIMCreateIsochTransfer - old method");
	return kIOReturnIPCError;
	
	/*****
		AppleEHCIIsochEndpoint *			pEP;
	bool								requestFromRosettaClient = false;
	
	USBLog(5, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - adr=%d:%d cbp=%p:%lx (cback=[%lx:%lx:%lx]), frameStart: %qd, count: %ld, pFrames: %p", this,  
		   functionAddress, endpointNumber, pBuffer, 
		   pBuffer->getLength(), 
		   (UInt32)completion.action, (UInt32)completion.target, 
		   (UInt32)completion.parameter, frameStart, frameCount, pFrames);

    if ( (frameCount == 0) || (frameCount > 1000) )
    {
        USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer bad frameCount: %ld", this, frameCount);
        return kIOReturnBadArgument;
    }
	
	// Determine if our request came from a rosetta client
	if ( direction & 0x80 )
	{
		requestFromRosettaClient = true;
		direction &= ~0x80;
	}
	
    pEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL));
    if(pEP == NULL)
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - Endpoint not found", this);
        return kIOUSBEndpointNotFound;        
    }
    
    if ( pEP->aborting )
    {
        USBLog(3, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - sent request while aborting endpoint. Returning kIOReturnNotPermitted", this);
        return kIOReturnNotPermitted;        
    }
    
    if (pEP->highSpeedHub)
		return CreateSplitIsochTransfer(pEP, completion, frameStart, pBuffer, frameCount, pFrames, 0, false, requestFromRosettaClient);
    else
		return CreateHSIsochTransfer(pEP, completion, frameStart, pBuffer, frameCount, (IOUSBLowLatencyIsocFrame*)pFrames, 0, false, requestFromRosettaClient);
	*****/
}



// this is the Low Latency case
IOReturn
AppleUSBEHCI::UIMCreateIsochTransfer(	short						functionAddress,
										short						endpointNumber,
										IOUSBIsocCompletion			completion,
										UInt8						direction,
										UInt64						frameNumberStart,
										IOMemoryDescriptor			*pBuffer,
										UInt32						frameCount,
										IOUSBLowLatencyIsocFrame	*pFrames,
										UInt32						updateFrequency)
{
	USBError(1, "AppleUSBEHCI::UIMCreateIsochTransfer(LL) - old method");
	return kIOReturnIPCError;
	
	/*****
    AppleEHCIIsochEndpoint  *			pEP;
	bool								requestFromRosettaClient = false;
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - adr=%d:%d cbp=%p:%lx (cback=[%lx:%lx:%lx])", this, functionAddress, endpointNumber, pBuffer, pBuffer->getLength(), (UInt32)completion.action, (UInt32)completion.target, (UInt32)completion.parameter);
    if ( (frameCount == 0) || (frameCount > 1000) )
    {
        USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer bad frameCount: %ld", this, frameCount);
        return kIOReturnBadArgument;
    }

	// Determine if our request came from a rosetta client
	if ( direction & 0x80 )
	{
		requestFromRosettaClient = true;
		direction &= ~0x80;
	}
	
    pEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL));
	
    if(pEP == NULL)
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - Endpoint not found", this);
        return kIOUSBEndpointNotFound;        
    }
    
    if ( pEP->aborting )
    {
        USBLog(3, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - sent request while aborting endpoint. Returning kIOReturnNotPermitted", this);
        return kIOReturnNotPermitted;        
    }
    
    if (pEP->highSpeedHub)
		return CreateSplitIsochTransfer(pEP, completion, frameNumberStart, pBuffer, frameCount, (IOUSBIsocFrame*)pFrames, updateFrequency, true, requestFromRosettaClient);
    else
		return CreateHSIsochTransfer(pEP, completion, frameNumberStart, pBuffer, frameCount, pFrames, updateFrequency, true, requestFromRosettaClient);
	*****/
}


IOReturn 
AppleUSBEHCI::UIMCreateIsochTransfer(IOUSBIsocCommand *command)
{
	IOReturn								err;
    AppleEHCIIsochEndpoint  *				pEP;
    UInt64									maxOffset;
    UInt64									curFrameNumber = GetFrameNumber();
    UInt64									frameDiff;
    UInt32									diff32;

	USBLog(7, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - adr=%d:%d IOMD=%p, frameStart: %qd, count: %ld, pFrames: %p", this,  
		   command->GetAddress(), command->GetEndpoint(), 
		   command->GetBuffer(), 
		   command->GetStartFrame(), command->GetNumFrames(), command->GetFrameList());
	
    if ( (command->GetNumFrames() == 0) || (command->GetNumFrames() > 1000) )
    {
        USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer bad frameCount: %ld", this, command->GetNumFrames());
        return kIOReturnBadArgument;
    }
	
    pEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(command->GetAddress(), command->GetEndpoint(), command->GetDirection(), NULL));
	
    if(pEP == NULL)
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - Endpoint not found", this);
        return kIOUSBEndpointNotFound;        
    }
    
    if ( pEP->aborting )
    {
        USBLog(3, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - sent request while aborting endpoint. Returning kIOReturnNotPermitted", this);
        return kIOReturnNotPermitted;        
    }

    if (command->GetStartFrame() < pEP->firstAvailableFrame)
    {
		USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer: no overlapping frames -   EP (%p) frameNumberStart: %Ld, pEP->firstAvailableFrame: %Ld.  Returning 0x%x", this, pEP, command->GetStartFrame(), pEP->firstAvailableFrame, kIOReturnIsoTooOld);
		return kIOReturnIsoTooOld;
    }

    maxOffset = _frameListSize;
    pEP->firstAvailableFrame = command->GetStartFrame();
	
    if (command->GetStartFrame() <= curFrameNumber)
    {
        if (command->GetStartFrame() < (curFrameNumber - maxOffset))
        {
            USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer request frame WAY too old.  frameNumberStart: %ld, curFrameNumber: %ld.  Returning 0x%x", this, (UInt32)command->GetStartFrame(), (UInt32) curFrameNumber, kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
        USBLog(5,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %ld, curFrameNumber: %ld.  USBIsocFrame Ptr: %p, First ITD: %p", this, (UInt32)command->GetStartFrame(), (UInt32)curFrameNumber, command->GetFrameList(), pEP->toDoEnd);
    } 
	else 
    {					// frameNumberStart > curFrameNumber
        if (command->GetStartFrame() > (curFrameNumber + maxOffset))
        {
            USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer request frame too far ahead!  frameNumberStart: %ld, curFrameNumber: %ld", this, (UInt32)command->GetStartFrame(), (UInt32) curFrameNumber);
            return kIOReturnIsoTooNew;
        }
        frameDiff = command->GetStartFrame() - curFrameNumber;
        diff32 = (UInt32)frameDiff;
        if (diff32 < _istKeepAwayFrames)
        {
            USBLog(5,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer WARNING! - frameNumberStart less than 2 ms (is %ld)!  frameNumberStart: %ld, curFrameNumber: %ld", this, (UInt32) diff32, (UInt32)command->GetStartFrame(), (UInt32) curFrameNumber);
        }
    }
	
	if (!command->GetDMACommand() || !command->GetDMACommand()->getMemoryDescriptor())
	{
		USBError(1, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - no DMA Command or missing memory descriptor", this);
		return kIOReturnBadArgument;
	}
	
	USBLog(7, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - pEP (%p) command (%p) HSHub (%s)", this, pEP, command, pEP->highSpeedHub ? "true" : "false");
	
    if (pEP->highSpeedHub)
		return CreateSplitIsochTransfer(pEP, command);
    else
		return CreateHSIsochTransfer(pEP, command);
}



void 
AppleUSBEHCI::AddIsocFramesToSchedule(AppleEHCIIsochEndpoint	*pEP)
{
    UInt64										currFrame, startFrame, finFrame;
    IOUSBControllerIsochListElement				*pTD = NULL;
	AppleEHCISplitIsochTransferDescriptor		*pSITD = NULL;
    UInt16										nextSlot, firstOutSlot;
    AbsoluteTime								timeStamp;
	bool										fetchNewTDFromToDoList = true;
    if(pEP->toDoList == NULL)
    {
		USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - no frames to add fn:%d EP:%d", this, pEP->functionAddress, pEP->endpointNumber);
		return;
    }
    if(pEP->aborting)
    {
		USBLog(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - EP (%p) is aborting - not adding", this, pEP);
		return;
    }

	USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - scheduledTDs = %ld, deferredTDs = %ld", this, pEP->scheduledTDs, pEP->deferredTDs);

	// 4211382 - This routine is already non-reentrant, since it runs on the WL.
	// However, we also need to disable preemption while we are in here, since we have to get everything
	// done within a couple of milliseconds, and if we are preempted, we may come back long after that
	// point. So take a SimpleLock to prevent preemption
	if (!IOSimpleLockTryLock(_isochScheduleLock))
	{
		// This would indicate reentrancy, which should never ever happen
		USBError(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - could not obtain scheduling lock", this);
		return;
	}
	
	//*******************************************************************************************************
	// ************* WARNING WARNING WARNING ****************************************************************
	// Preemption is now off, which means that we cannot make any calls which may block
	// So don't call USBLog for example
	//*******************************************************************************************************
	
    // Don't get GetFrameNumber() unless we're going to use it
    //
    currFrame = GetFrameNumber();
	startFrame = currFrame;
    
    // USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - fn:%d EP:%d inSlot (0x%x), currFrame: 0x%Lx", this, pEP->functionAddress, pEP->endpointNumber, pEP->inSlot, currFrame);
    clock_get_uptime(&timeStamp);
    while(pEP->toDoList->_frameNumber <= (currFrame+_istKeepAwayFrames))		// Add keepaway, and use <= so you never put in a new frame 
																				// at less than 2 ahead of now. (EHCI spec, 7.2.1)
    {
		IOReturn	ret;
		UInt64		newCurrFrame;
		
		// this transaction is old before it began, move to done queue
		pTD = GetTDfromToDoList(pEP);
		//USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - ignoring TD(%p) because it is too old (%Lx) vs (%Lx) ", this, pTD, pTD->_frameNumber, currFrame);
		ret = pTD->UpdateFrameList(timeStamp);		// TODO - accumulate the return values
		if (pEP->scheduledTDs > 0)
			PutTDonDeferredQueue(pEP, pTD);
		else
		{
			//USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - putting TD(%p) on Done Queue instead of Deferred Queue ", this, pTD);
			PutTDonDoneQueue(pEP, pTD, true);
		}
	    
        //USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - pTD = %p", this, pTD);
		if (pEP->toDoList == NULL)
		{	
			// Run out of transactions to move.  Call this on a separate thread so that we return to the caller right away
            // 
			// ReturnIsocDoneQueue(pEP);
			IOSimpleLockUnlock(_isochScheduleLock);
			// OK to call USBLog, now that preemption is reenabled
			USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - calling thread_call_enter1(_returnIsochDoneQueueThread) scheduledTDs = %ld, deferredTDs = %ld", this, pEP->scheduledTDs, pEP->deferredTDs);
			bool alreadyQueued = thread_call_enter1(_returnIsochDoneQueueThread, (thread_call_param_t) pEP);
			if ( alreadyQueued )
			{
				USBLog(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - thread_call_enter1(_returnIsochDoneQueueThread) was NOT scheduled.  That's not good", this);
			}
			return;
		}
		newCurrFrame = GetFrameNumber();
		if (newCurrFrame > currFrame)
		{
			//USBLog(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - Current frame moved (0x%Lx->0x%Lx) resetting", this, currFrame, newCurrFrame);
			currFrame = newCurrFrame;
		}		
    }
    
	firstOutSlot = currFrame & (kEHCIPeriodicListEntries-1);									// this will be used if the _outSlot is not yet initialized
	
    currFrame = pEP->toDoList->_frameNumber;													// start looking at the first available number

    // This needs to be fixed up when we have variable length lists.
    pEP->inSlot = currFrame & (kEHCIPeriodicListEntries-1);

    do
    {		
		nextSlot = (pEP->inSlot + 1) & (kEHCIPeriodicListEntries-1);
		if (pEP->inSlot == _outSlot)
		{
			// USBLog(2, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - caught up pEP->inSlot (0x%x) _outSlot (0x%x)", this, pEP->inSlot, _outSlot);
			break;
		}
		if( nextSlot == _outSlot) 								// weve caught up with our tail
		{
			// USBLog(2, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - caught up nextSlot (0x%x) _outSlot (0x%x)", this, nextSlot, _outSlot);
			break;
		}
		
		if ( fetchNewTDFromToDoList )
		{
			pTD = GetTDfromToDoList(pEP);
			pSITD = OSDynamicCast(AppleEHCISplitIsochTransferDescriptor, pTD);
			// USBLog(5, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - checking TD(%p) FN(0x%Lx) against currFrame (0x%Lx)", this, pTD, pTD->_frameNumber, currFrame);
		}
		
		if(currFrame == pTD->_frameNumber)
		{
			IOUSBControllerListElement					*linkAfter = NULL;
			IOUSBControllerIsochListElement				*prevIsochLE;				// could be split or HS
			AppleEHCISplitIsochTransferDescriptor		*prevSITD;
			
			if(_outSlot > kEHCIPeriodicListEntries)
			{
				_outSlot = firstOutSlot;
			}
			// Place TD in list
			
			//USBLog(5, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - linking TD (%p) with frame (0x%Lx) into slot (0x%x) - curr next log (%p) phys (%p)", this, pTD, pTD->_frameNumber, pEP->inSlot, _logicalPeriodicList[pEP->inSlot], (void *)USBToHostLong(_periodicList[pEP->inSlot]));
			//pTD->print(7);
			prevIsochLE = OSDynamicCast(IOUSBControllerIsochListElement, _logicalPeriodicList[pEP->inSlot]);
			while (prevIsochLE)
			{
				prevSITD = OSDynamicCast(AppleEHCISplitIsochTransferDescriptor, prevIsochLE);
				if (prevSITD)
				{
					if (pEP->direction == kUSBIn)
					{
						
						// IN SITDs have to be linked AFTER
						//	a) any DUMMY SITDs on the same TT
						//	b) any OUT SITDs on the same TT
						if (((AppleEHCIIsochEndpoint*)(prevSITD->_pEndpoint))->hiPtr == pEP->hiPtr)
						{
							if (prevSITD->_isDummySITD)
							{
								//USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - found previous DUMMY SITD (%p) on same TT - linking after", this, prevSITD);
								linkAfter = prevSITD;
							}
							else if (prevSITD->_pEndpoint->direction == kUSBOut)
							{
								//USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - found previous OUT SITD (%p) on same TT - linking after", this, prevSITD);
								linkAfter = prevSITD;
							}
						}
					}
					else
					{
						// OUT transaction. check frame ordering
						UInt8	oldStart = FirstScheduledSSMicroFrame((AppleEHCIIsochEndpoint*)(prevSITD->_pEndpoint));
						UInt8	myStart = FirstScheduledSSMicroFrame((AppleEHCIIsochEndpoint*)(pTD->_pEndpoint));
						UInt8	oldEnd = LastScheduledSSMicroFrame((AppleEHCIIsochEndpoint*)(prevSITD->_pEndpoint));
						UInt8	myEnd = LastScheduledSSMicroFrame((AppleEHCIIsochEndpoint*)(pTD->_pEndpoint));
						
						if ((oldStart < myStart) || ((oldStart == myStart) && (oldEnd < myEnd)))
						{
							//USBLog(6, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - me[%d:%d] linking AFTER previous [%d:%d]", this, myStart, myEnd, oldStart, oldEnd);
							linkAfter = prevSITD;
						}
						else
						{
							//USBLog(6, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - me[%d:%d] ignoring previous [%d:%d]", this, myStart, myEnd, oldStart, oldEnd);
						}
					}
				}
				prevIsochLE = OSDynamicCast(IOUSBControllerIsochListElement, prevIsochLE->_logicalNext);
			}
			if (linkAfter)
			{
				pTD->SetPhysicalLink(linkAfter->GetPhysicalLink());
				pTD->_logicalNext = linkAfter->_logicalNext;
				linkAfter->_logicalNext = pTD;
				linkAfter->SetPhysicalLink(pTD->GetPhysicalAddrWithType());
			}
			else
			{
				pTD->SetPhysicalLink(USBToHostLong(_periodicList[pEP->inSlot]));
				pTD->_logicalNext = _logicalPeriodicList[pEP->inSlot];
				_logicalPeriodicList[pEP->inSlot] = pTD;
				_periodicList[pEP->inSlot] = HostToUSBLong(pTD->GetPhysicalAddrWithType());
			}
			
			OSIncrementAtomic(&(pEP->scheduledTDs));
			fetchNewTDFromToDoList = true;
			// USBLog(5, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - _periodicList[%x]:%x", this, pEP->inSlot, USBToHostLong(_periodicList[pEP->inSlot]));
		}
		else
		{
			// USBLog(3, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - expected frame (%qd) and see frame (%qd) -  will increment our slot/frame number without fetching a new TD", this, currFrame, pTD->_frameNumber);
			fetchNewTDFromToDoList = false;
		}
		
		if (!pSITD || !(pSITD->_isDummySITD))
		{
			currFrame++;
			pEP->inSlot = nextSlot;
		}
		//USBLog(5, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - pEP->inSlot is now 0x%x", this, pEP->inSlot);	
    } while(pEP->toDoList != NULL);
	
	finFrame = GetFrameNumber();
	// Unlock, reenable preemption, so we can log
	IOSimpleLockUnlock(_isochScheduleLock);
	if ((finFrame - startFrame) > 1)
		USBError(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - end -  startFrame(0x%Lx) finFrame(0x%Lx)", this, startFrame, finFrame);
    USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - finished,  currFrame: %Lx", this, GetFrameNumber() );
}



IOReturn 		
AppleUSBEHCI::UIMHubMaintenance(USBDeviceAddress highSpeedHub, UInt32 highSpeedPort, UInt32 command, UInt32 flags)
{
    AppleUSBEHCIHubInfo		*hiPtr;
	
    switch (command)
    {
		case kUSBHSHubCommandAddHub:
			USBLog(7, "AppleUSBEHCI[%p]::UIMHubMaintenance - adding hub %d with flags 0x%lx", this, highSpeedHub, flags);
			hiPtr = AppleUSBEHCIHubInfo::GetHubInfo(&_hsHubs, highSpeedHub, 0);
			if (hiPtr)
			{
				USBLog(7, "AppleUSBEHCI[%p]::UIMHubMaintenance - adding hub which already exists (%d)", this, highSpeedHub);
				AppleUSBEHCIHubInfo::DeleteHubInfoZero(&_hsHubs, highSpeedHub);
			}
			hiPtr = AppleUSBEHCIHubInfo::NewHubInfoZero(&_hsHubs, highSpeedHub, flags);
			if (!hiPtr)
				return kIOReturnNoMemory;
			USBLog(7, "AppleUSBEHCI[%p]::UIMHubMaintenance - done creating new hub (%p) for address (%d)", this, hiPtr, highSpeedHub);
			break;
			
		case kUSBHSHubCommandRemoveHub:
			USBLog(7, "AppleUSBEHCI[%p]::UIMHubMaintenance - deleting hub %d", this, highSpeedHub);
			AppleUSBEHCIHubInfo::DeleteHubInfoZero(&_hsHubs, highSpeedHub);			// remove master device and all sub devices
			break;
			
		default:
			return kIOReturnBadArgument;
    }
    return kIOReturnSuccess;
}
#define	kEHCIUIMScratchFirstActiveFrame	0

void 
AppleUSBEHCI::ReturnOneTransaction(
								   EHCIGeneralTransferDescriptor 	*transaction,
								   AppleEHCIQueueHead   		*pED,
								   AppleEHCIQueueHead   		*pEDBack,
								   IOReturn				err)
{
    // make sure it is halted, since we should leave it linked
    HaltAsyncEndpoint(pED, pEDBack);
	
    // USBLog(6, "ReturnOneTransaction Enter with transaction %p",transaction);
	
    while(transaction!= NULL)
    {
		if(transaction->lastTDofTransaction)
		{
			transaction = transaction->pLogicalNext;
			break;
		}
		transaction = transaction->pLogicalNext;
		// USBLog(7, "ReturnOneTransaction next transaction %p",transaction);
    }
    // USBLog(6, "ReturnOneTransaction going with transaction %p",transaction);
    if(transaction == NULL)
    {
		// This works, sort of, NULL for an end transction means remove them all.
		// But there will be no callback
		USBLog(1, "AppleUSBEHCI[%p]::ReturnOneTransaction - returning all TDs on the queue", this);
    }
    else
    {
		
    }
    returnTransactions(pED, transaction, err, true);
}

UInt32 AppleUSBEHCI::findBufferRemaining(AppleEHCIQueueHead *pED)
{
	UInt32 flags, bufferSizeRemaining;
	
	flags = USBToHostLong(pED->GetSharedLogical()->qTDFlags);
	
	bufferSizeRemaining = (flags & kEHCITDFlags_Bytes) >> kEHCITDFlags_BytesPhase;
	
	return(bufferSizeRemaining);
}

void
AppleUSBEHCI::CheckEDListForTimeouts(AppleEHCIQueueHead *head)
{
    AppleEHCIQueueHead				*pED = head;
    AppleEHCIQueueHead				*pEDBack = NULL, *pEDBack1 = NULL;
    IOPhysicalAddress				pTDPhys;
    EHCIGeneralTransferDescriptor 	*pTD;
	
    UInt32 				noDataTimeout;
    UInt32				completionTimeout;
    UInt32				rem;
    UInt32				curFrame = GetFrameNumber32();
	
    for (; pED != 0; pED = (AppleEHCIQueueHead *)pED->_logicalNext)
    {
		USBLog(7, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - checking ED [%p]", this, pED);
		pED->print(7);
		// Need to keep a note of the previous ED for back links. Usually I'd
		// put a pEDBack = pED at the end of the loop, but there are lots of 
		// continues in this loop so it was getting skipped (and unlinking the
		// entire async schedule). These lines get the ED from the previous 
		// interation in pEDBack.
		pEDBack = pEDBack1;
		pEDBack1 = pED;
		
		
		// OHCI gets phys pointer and logicals that, that seems a little complicated, so
		// I'll get the logical pointer and compare it to the phys. If they're different,
		// this transaction has only just got to the head and the previous one(s) haven't
		// been scavenged yet. Assume its not a good candidate for a timeout.
		
		// get the top TD
		pTDPhys = USBToHostLong(pED->GetSharedLogical()->CurrqTDPtr) & kEHCIEDTDPtrMask;
		pTD = pED->_qTD;
		if (!pTD)
		{
			USBLog(6, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - no TD", this);
			continue;
		}
		
		if (!pTD->command)
		{
			USBLog(7, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - found a TD without a command - moving on", this);
			continue;
		}
		
		if (pTD == pED->_TailTD)
		{
			USBLog(1, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - ED (%p) - TD is TAIL but there is a command - pTD (%p)", this, pED, pTD);
			pED->print(5);
		}
		
		if((pTDPhys != pTD->pPhysical) && !(pED->GetSharedLogical()->qTDFlags & USBToHostLong(kEHCITDStatus_Halted | kEHCITDStatus_Active)  ))
		{
			USBLog(6, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - pED (%p) - mismatched logical and physical - TD (L:%p - P:%p) will be scavenged later", this, pED, pTD, (void*)pTD->pPhysical);
			pED->print(7);
			printTD(pTD, 7);
			if (pTD->pLogicalNext)
				printTD(pTD->pLogicalNext, 7);
			continue;
		}
		
		noDataTimeout = pTD->command->GetNoDataTimeout();
		completionTimeout = pTD->command->GetCompletionTimeout();
		
		if (completionTimeout)
		{
			UInt32	firstActiveFrame = pTD->command->GetUIMScratch(kEHCIUIMScratchFirstActiveFrame);
			if (!firstActiveFrame)
			{
				pTD->command->SetUIMScratch(kEHCIUIMScratchFirstActiveFrame, curFrame);
				continue;
			}
			if ((curFrame - firstActiveFrame) >= completionTimeout)
			{
				UInt32	myFlags = USBToHostLong(pED->GetSharedLogical()->flags);

				USBLog(2, "AppleUSBEHCI[%p]::CheckEDListForTimeout - Found a TD [%p] on QH [%p] past the completion deadline, timing out! (0x%lx - 0x%lx)", this, pTD, pED, curFrame, firstActiveFrame);
				USBError(1, "AppleUSBEHCI[%p]::Found a transaction past the completion deadline on bus 0x%lx, timing out! (Addr: %ld, EP: %ld)", this, _busNumber, ((myFlags & kEHCIEDFlags_FA) >> kEHCIEDFlags_FAPhase), ((myFlags & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase) );
				pED->print(2);
				ReturnOneTransaction(pTD, pED, pEDBack, kIOUSBTransactionTimeout);
				continue;
			}
		}
		
		if (!noDataTimeout)
			continue;
		
		if (!pTD->lastFrame || (pTD->lastFrame > curFrame))
		{
			// this pTD is not a candidate yet, remember the frame number and go on
			pTD->lastFrame = curFrame;
			pTD->lastRemaining = findBufferRemaining(pED /*pTD get value from overlay area*/);
			continue;
		}
		rem = findBufferRemaining(pED /*pTD get value from overlay area*/);
		
		if (pTD->lastRemaining != rem)
		{
			// there has been some activity on this TD. update and move on
			pTD->lastRemaining = rem;
			continue;
		}
		if ((curFrame - pTD->lastFrame) >= noDataTimeout)
		{
			UInt32	myFlags = USBToHostLong(pED->GetSharedLogical()->flags);
			
			USBLog(2, "AppleUSBEHCI[%p]CheckEDListForTimeout:  Found a transaction (%p) which hasn't moved in 5 seconds, timing out! (0x%lx - 0x%lx)", this, pTD, curFrame, pTD->lastFrame);
			USBError(1, "AppleUSBEHCI[%p]::Found a transaction which hasn't moved in 5 seconds on bus 0x%lx, timing out! (Addr: %ld, EP: %ld)", this, _busNumber, ((myFlags & kEHCIEDFlags_FA) >> kEHCIEDFlags_FAPhase), ((myFlags & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase) );
			//printED(pED);
			//printTD(pTD);
			//printAsyncQueue();
			ReturnOneTransaction(pTD, pED, pEDBack, kIOUSBTransactionTimeout);
			//printED(pED);
			//printTD(pTD);
			//printAsyncQueue();
			
			continue;
		}
    }
}


//=============================================================================================
//
//  UIMCheckForTimeouts
//
//  This routine is called every kUSBWatchdogTimeoutMS by the controller.  It is useful for
//  periodic checks in the UIM
//
//=============================================================================================
//
void
AppleUSBEHCI::UIMCheckForTimeouts(void)
{
    AbsoluteTime	currentTime;
    AbsoluteTime	lastRootHubChangeTime;
    UInt64			elapsedTime = 0;
    bool			allPortsDisconnected = false;
	UInt32			usbcmd;
	UInt32			usbsts;
	
    // If we are not active anymore or if we're in ehciBusStateOff, then don't check for timeouts 
    //
    if ( isInactive() || (_onCardBus && _pcCardEjected) || !_controllerAvailable || (_myBusState != kUSBBusStateRunning) || _wakingFromHibernation)
	{
        return;
	}

	usbcmd = USBToHostLong(_pEHCIRegisters->USBCMD);
	usbsts = USBToHostLong(_pEHCIRegisters->USBSTS);
	
	// check the state of the AsyncEnable bits in the CMD/STS registers
	if (usbcmd & kEHCICMDAsyncEnable)
	{
		if (!(usbsts & kEHCISTSAsyncScheduleStatus))
		{
			_asynchScheduleUnsynchCount++;
			USBLog(2, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Async USBCMD and USBSTS not synched ON (#%d)", this, _asynchScheduleUnsynchCount);
			if (_asynchScheduleUnsynchCount >= 10)
			{
				USBError(1, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Async USBCMD and USBSTS not synched ON (#%d)", this, _asynchScheduleUnsynchCount);
			}
		}
		else
			_asynchScheduleUnsynchCount = 0;
	}
	else
	{
		if (usbsts & kEHCISTSAsyncScheduleStatus)
		{
			_asynchScheduleUnsynchCount++;
			USBLog(2, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Async USBCMD and USBSTS not synched OFF (#%d)", this, _asynchScheduleUnsynchCount);
			if (_asynchScheduleUnsynchCount >= 10)
			{
				USBError(1, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Async USBCMD and USBSTS not synched OFF (#%d)", this, _asynchScheduleUnsynchCount);
			}
		}
		else
			_asynchScheduleUnsynchCount = 0;
	}
	
	if (usbcmd & kEHCICMDPeriodicEnable)
	{
		if (!(usbsts & kEHCISTSPeriodicScheduleStatus))
		{
			_periodicScheduleUnsynchCount++;
			USBLog(2, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Periodic USBCMD and USBSTS not synched ON (#%d)", this, _periodicScheduleUnsynchCount);
			if (_periodicScheduleUnsynchCount >= 10)
			{
				USBError(1, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Periodic USBCMD and USBSTS not synched ON (#%d)", this, _periodicScheduleUnsynchCount);
			}
		}
		else
			_periodicScheduleUnsynchCount = 0;
	}
	else
	{
		if (usbsts & kEHCISTSPeriodicScheduleStatus)
		{
			_periodicScheduleUnsynchCount++;
			USBLog(2, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Periodic USBCMD and USBSTS not synched ON (#%d)", this, _periodicScheduleUnsynchCount);
			if (_periodicScheduleUnsynchCount >= 10)
			{
				USBError(1, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Periodic USBCMD and USBSTS not synched ON (#%d)", this, _periodicScheduleUnsynchCount);
			}
		}
		else
			_periodicScheduleUnsynchCount = 0;
	}
	
    // Check to see if our control or bulk lists have a TD that has timed out
    //
    CheckEDListForTimeouts(_AsyncHead);
	
}



// Don't call IORegistry methods as this call might be called holding a spinlock
UInt8
AppleUSBEHCI::FirstScheduledSSMicroFrame(AppleEHCIIsochEndpoint	*pEP)
{
	UInt8		ssFlags = pEP->startSplitFlags;
	UInt8		ret = 0;
	
	if (!ssFlags)
		return 0;
	
	while ((ssFlags & 1) == 0)
	{
		ret++;
		ssFlags >>= 1;
	}
	USBLog(6, "AppleUSBEHCI[%p]::FirstScheduledSSMicroFrame(%p): returning %d", this, pEP, ret);
	return ret;
}



// Don't call IORegistry methods as this call might be called holding a spinlock
UInt8 
AppleUSBEHCI::LastScheduledSSMicroFrame(AppleEHCIIsochEndpoint	*pEP)
{
	UInt8		ssFlags = pEP->startSplitFlags;
	UInt8		ret = 7;
	
	if (!ssFlags)
		return 0;
	
	while ((ssFlags & 0x80) == 0)
	{
		ret--;
		ssFlags <<= 1;
	}
	
	USBLog(6, "AppleUSBEHCI[%p]::LastScheduledSSMicroFrame(%p): returning %d", this, pEP, ret);
	return ret;
}


// this call is not gated, so we need to gate it ourselves
IOReturn
AppleUSBEHCI::GetFrameNumberWithTime(UInt64* frameNumber, AbsoluteTime *theTime)
{
	if (!_commandGate)
		return kIOReturnUnsupported;
		
	return _commandGate->runAction(GatedGetFrameNumberWithTime, frameNumber, theTime);
}



// here is the gated version
IOReturn
AppleUSBEHCI::GatedGetFrameNumberWithTime(OSObject *owner, void* arg0, void* arg1, void* arg2, void* arg3)
{
	AppleUSBEHCI		*me = (AppleUSBEHCI*)owner;
	UInt64				*frameNumber = (UInt64*)arg0;
	AbsoluteTime		*theTime = (AbsoluteTime*)arg1;
	
	*frameNumber = me->_anchorFrame;
	*theTime = me->_anchorTime;
	return kIOReturnSuccess;
}



// ========================================================================
#pragma mark Disabled Endpoints
// ========================================================================


IOReturn
AppleUSBEHCI::UIMEnableAddressEndpoints(USBDeviceAddress address, bool enable)
{
	UInt32							slot;
	IOReturn						err;
	AppleEHCIQueueHead				*pQH = NULL;
	AppleEHCIQueueHead				*pPrevQH = NULL;
    IOUSBControllerListElement *	pLEBack;
    IOUSBControllerListElement *	pLE;
    int								i;
	
	
	
	USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints(%d, %s)", this, (int)address, enable ? "true" : "false");
	printAsyncQueue(7, "+UIMEnableAddressEndpoints");
	showRegisters(7, "+UIMEnableAddressEndpoints");
	
	if (enable)
	{
		pQH = _disabledQHList;
		USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints- looking for QHs for address (%d) in the disabled queue", this, address);
		while (pQH)
		{
			if (pQH->_functionNumber == address)
			{
				USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints- found matching QH[%p] on disabled list - reenabling", this, pQH);
				// first adjust the list
				if (pPrevQH)
					pPrevQH->_logicalNext = pQH->_logicalNext;
				else
					_disabledQHList = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
				pQH->_logicalNext = NULL;
				// now stick pQH back in the queue
				switch (pQH->_queueType)
				{
					case kEHCITypeControl:
					case kEHCITypeBulk:
						if (_AsyncHead)
						{
							linkAsyncEndpoint(pQH, _AsyncHead);
						}
						else
						{
							linkAsyncEndpoint(pQH, NULL);
							_AsyncHead = pQH;
							_pEHCIRegisters->AsyncListAddr = HostToUSBLong( pQH->_sharedPhysical );
						}
						break;
						
					case kEHCITypeInterrupt:
						linkInterruptEndpoint(pQH);
						break;
						
					default:
						USBError(1, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints- found QH[%p] with unknown type(%d)", this, pQH, pQH->_queueType);
						break;
				}
				// advance the pointer
				if (pPrevQH)
					pQH = OSDynamicCast(AppleEHCIQueueHead, pPrevQH->_logicalNext);
				else
					pQH = _disabledQHList;
			}
			else
			{
				// advance the pointer when we didn't find what we were looking for
				pPrevQH = pQH;
				pQH = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
			}
		}
	}
	else
	{
		// the disable case
		USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints- looking for endpoints for device address(%d) to disable", this, address);

		// look throught the Control/Bulk list
		pQH = _AsyncHead;
		while (pQH)
		{
			if (pQH->_functionNumber == address)
			{
				USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints- found matching QH[%p] with _queueType (%d) on AsyncList - disabling", this, pQH, pQH->_queueType);
				unlinkAsyncEndpoint(pQH, pPrevQH);
				pQH->_logicalNext = _disabledQHList;
				_disabledQHList = pQH;
				pQH = pPrevQH;
			}
			pPrevQH = pQH;
			pQH = pQH ? OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext) : _AsyncHead;
		}
		
		pLEBack = NULL;
		
		for(i= 0; i < _greatestPeriod; i++)
		{
			pLE = _logicalPeriodicList[i];
			while ( pLE != NULL)
			{
				pQH = OSDynamicCast(AppleEHCIQueueHead, pLE);
				if (pQH)
				{
					if (pQH->_functionNumber == address)
					{
						USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints- found matching QH[%p] with _queueType (%d) on periodic list - disabling", this, pQH, pQH->_queueType);
						unlinkIntEndpoint(pQH);
						pQH->_logicalNext = _disabledQHList;
						_disabledQHList = pQH;
						pLE = pLEBack;
					}
				}
				pLEBack = pLE;
				pLE = pLE ? pLE->_logicalNext : _logicalPeriodicList[i];
			} 
		}
	}
	
	pQH = _disabledQHList;
	if (!pQH)
	{
		USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints - list is now empty", this);
	}
	else
	{
		USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints - new list:", this);
		while (pQH)
		{
			USBLog(5, "\t[%p]", pQH);
			pQH = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
		}
	}
	printAsyncQueue(7, "-UIMEnableAddressEndpoints");
	showRegisters(7, "-UIMEnableAddressEndpoints");
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::UIMEnableAllEndpoints(bool enable)
{
	UInt32							slot;
	IOReturn						err;
	AppleEHCIQueueHead				*pQH = NULL;
    IOUSBControllerListElement *	pLEBack;
    IOUSBControllerListElement *	pLE;
    int								i;
	
	
	USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints(%s)", this, enable ? "true" : "false");
	showRegisters(7, "+UIMEnableAllEndpoints");
	printAsyncQueue(7, "+UIMEnableAllEndpoints");
	if (enable)
	{
		pQH = _disabledQHList;
		USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints- looking for QHs in the disabled queue", this);
		while (pQH)
		{
			USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints- found QH[%p] on disabled list - reenabling", this, pQH);
			// first adjust the list
			_disabledQHList = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
			pQH->_logicalNext = NULL;
			// now stick pQH back in the queue
			switch (pQH->_queueType)
			{
				case kEHCITypeControl:
				case kEHCITypeBulk:
					if (_AsyncHead)
					{
						linkAsyncEndpoint(pQH, _AsyncHead);
					}
					else
					{
						linkAsyncEndpoint(pQH, NULL);
						_AsyncHead = pQH;
						_pEHCIRegisters->AsyncListAddr = HostToUSBLong( pQH->_sharedPhysical );
					}
					break;
					
				case kEHCITypeInterrupt:
					linkInterruptEndpoint(pQH);
					break;
					
				default:
					USBError(1, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints- found QH[%p] with unknown type(%d)", this, pQH, pQH->_queueType);
					break;
			}
			// advance the pointer
			pQH = _disabledQHList;
		}
	}
	else
	{
		// the disable case
		USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints- disabling endpoints", this);

		// look throught the Control/Bulk list
		pQH = _AsyncHead;
		while (pQH)
		{
			USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints- found matching QH[%p] with _queueType (%d) on AsyncList - disabling", this, pQH, pQH->_queueType);
			unlinkAsyncEndpoint(pQH, NULL);
			pQH->_logicalNext = _disabledQHList;
			_disabledQHList = pQH;
			pQH = _AsyncHead;
		}
		
		pLEBack = NULL;
		
		for(i= 0; i < _greatestPeriod; i++)
		{
			pLE = _logicalPeriodicList[i];
			while ( pLE != NULL)
			{
				pQH = OSDynamicCast(AppleEHCIQueueHead, pLE);
				if (pQH)
				{
					USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints- found matching QH[%p] with _queueType (%d) on periodic list - disabling", this, pQH, pQH->_queueType);
					unlinkIntEndpoint(pQH);
					pQH->_logicalNext = _disabledQHList;
					_disabledQHList = pQH;
				}
				pLE = _logicalPeriodicList[i];
			} 
		}
	}
	
	pQH = _disabledQHList;
	if (!pQH)
	{
		USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints - list is now empty", this);
	}
	else
	{
		USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints - new list:", this);
		while (pQH)
		{
			USBLog(5, "\t[%p]", pQH);
			pQH = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
		}
	}

	showRegisters(7, "-UIMEnableAllEndpoints");
	printAsyncQueue(7, "-UIMEnableAllEndpoints");
	return kIOReturnSuccess;
}
