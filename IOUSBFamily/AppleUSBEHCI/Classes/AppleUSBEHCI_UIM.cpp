/*
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



#include <libkern/OSByteOrder.h>

#include "AppleUSBEHCI.h"
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMemoryCursor.h>

#include <IOKit/usb/IOUSBLog.h>

#define super IOUSBControllerV2
		
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

    USBLog(7, "%s[%p]::UIMCreateControlEndpoint(%d, %d, %d, %d @(%d, %d))\n", getName(), this,
          functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort);

    if (_rootHubFuncAddress == functionAddress)
    {
        return(kIOReturnSuccess);
    }

    if( (speed == kUSBDeviceSpeedLow) && (maxPacketSize > 8) )
    {
	if(functionAddress != 0)
	{
	    USBError(1, "%s[%p]::UIMCreateControlEndpoint - incorrect max packet size (%d) for low speed", getName(), this, maxPacketSize);
	    return(kIOReturnBadArgument);
	}
	USBLog(3, "%s[%p]::UIMCreateControlEndpoint - changing low speed max packet from %d to 8 for dev 0", getName(), this, maxPacketSize);
	maxPacketSize = 8;
    }
 
    pED = _AsyncHead;

    pEHCIEndpointDescriptor = AddEmptyCBEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, kEHCIEDDirectionTD, pED);

    if(pED == NULL)
    {
	_AsyncHead = pEHCIEndpointDescriptor;
	_pEHCIRegisters->AsyncListAddr = HostToUSBLong( pEHCIEndpointDescriptor->_sharedPhysical );
    }

    if (pEHCIEndpointDescriptor == NULL)
        return(-1);  //FIXME
	
    return (kIOReturnSuccess);
}



AppleEHCIQueueHead * 
AppleUSBEHCI::MakeEmptyEndPoint(
        UInt8 						functionAddress,
        UInt8						endpointNumber,
        UInt16						maxPacketSize,
        UInt8						speed,
        USBDeviceAddress				highSpeedHub,
        int						highSpeedPort,
        UInt8						direction)
{
	UInt32						myFunctionAddress;
	UInt32 						myEndPointNumber;
	UInt32						myMaxPacketSize;
	UInt32						mySpeed = 0;
	AppleEHCIQueueHead *				pED;
	EHCIGeneralTransferDescriptorPtr		pTD;
    
    USBLog(7, "%s[%p]::MakeEmptyEndPoint - Addr: %d, EPT#: %d, MPS: %d, speed: %d, hubAddr: %d, hubPort: %d, dir: %d", getName(), this, 
		functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);

    if( (highSpeedHub == 0) && (speed != kUSBDeviceSpeedHigh) )
    {
	USBLog(1, "%s[%p]::MakeEmptyEndPoint - new endpoint NOT fixing up speed", getName(), this);
	// speed = kUSBDeviceSpeedHigh;
    }

    pED = FindControlBulkEndpoint(functionAddress, endpointNumber, NULL, direction);
    if(pED != NULL)
    {
	USBLog(1, "%s[%p]::MakeEmptyEndPoint - old endpoint found, abusing %p", getName(), this, pED);
        pED->GetSharedLogical()->flags = 0xffffffff;
    }

    pED = AllocateQH();
    
    USBLog(7, "%s[%p]::MakeEmptyEndPoint - new endpoint %p", getName(), this, pED);
   
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
    else
    if(speed == kUSBDeviceSpeedLow)
    {
        mySpeed = 1  << kEHCIEDFlags_SPhase;
    }
    else
    if(speed == kUSBDeviceSpeedHigh)
    {
        mySpeed = 2  << kEHCIEDFlags_SPhase;
    }
    
    USBLog(7, "%s[%p]::MakeEmptyEndPoint - mySpeed %d", getName(), this, mySpeed);

    myMaxPacketSize = ((UInt32) maxPacketSize) << kEHCIEDFlags_MPSPhase;

    pED->direction = direction;

    USBLog(7, "%s[%p]::MakeEmptyEndPoint - MPS = %d, setting flags to %p", getName(), this, maxPacketSize, myFunctionAddress | myEndPointNumber | myMaxPacketSize | mySpeed);
    
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
// ??    pED->GetSharedLogical()->qTDFlags = 0;					// this sets the active and halted bits to zero.
#warning Async problems adding first qTD

    // Put in a blank TD now, this will be fetched everytime the endpoint is accessed 
    // so we can use it to drag in a real transaction.
		    
    pTD = AllocateTD(); 	// ***** What if this fails?????? see also OHCI
    pTD->pShared->flags = 0;	// make sure active is 0
    pTD->pShared->nextTD = HostToUSBLong(kEHCITermFlag);
    pTD->pShared->altTD = HostToUSBLong(kEHCITermFlag);
    pTD->command = NULL;
    
    USBLog(7, "%s[%p]::MakeEmptyEndPoint - pointing NextqTDPtr to %lx", getName(), this, pTD->pPhysical);
    pED->GetSharedLogical()->NextqTDPtr = HostToUSBLong( (UInt32)pTD->pPhysical & ~0x1F);

    pED->qTD = pTD;
    pED->TailTD =  pED->qTD;

    pED->responseToStall = 0;

    return pED;
}



AppleEHCIQueueHead * 
AppleUSBEHCI::MakeEmptyIntEndPoint(
    UInt8 			functionAddress,
    UInt8			endpointNumber,
    UInt16			maxPacketSize,
    UInt8			speed,
    USBDeviceAddress            highSpeedHub,
    int                         highSpeedPort,
    UInt8			direction)
{
    AppleEHCIQueueHead * 	intED;
    UInt32			mySMask;
    
    intED = MakeEmptyEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
	
    if (intED == NULL)
	return(NULL);

    mySMask = (0x08 << kEHCIEDSplitFlags_SMaskPhase);		// 3272813 - start on microframe 3
    if(speed != kUSBDeviceSpeedHigh)
    {
	mySMask |= (0xE0 << kEHCIEDSplitFlags_CMaskPhase);	// Complete  on frames 5, 6, and 7
    }

    intED->GetSharedLogical()->splitFlags |= HostToUSBLong(mySMask);
    return intED;
}



void 
AppleUSBEHCI::linkAsyncEndpoint(AppleEHCIQueueHead *CBED, AppleEHCIQueueHead *pEDHead)
{
    IOPhysicalAddress			newHorizPtr;

    if(pEDHead == NULL)	// No previous EDs, make this head of queue.
    {
	CBED->GetSharedLogical()->flags |= HostToUSBLong(1 << kEHCIEDFlags_HPhase);
    }

    // Point first endpoint to itself
    newHorizPtr = HostToUSBLong(CBED->GetPhysicalAddrWithType());		// Its a queue head

    USBLog(7, "%s[%p]::linkAsynEndpoint pEDHead %p", getName(), this, pEDHead);

    if(pEDHead == NULL)
    {
	CBED->GetSharedLogical()->nextQH = newHorizPtr;
	CBED->_logicalNext = NULL;
    }
    else
    {
	// New endpoints are inserted just after queuehead (not at beginning or end).
	
	// Point new endpoint at same endpoint old queuehead was pointing
	CBED->GetSharedLogical()->nextQH =  pEDHead->GetSharedLogical()->nextQH;
	CBED->_logicalNext = pEDHead->_logicalNext;
	
	// Point queue head to new endpoint
	pEDHead->_logicalNext = CBED;
	pEDHead->GetSharedLogical()->nextQH = newHorizPtr;
    }
}



AppleEHCIQueueHead * 
AppleUSBEHCI::AddEmptyCBEndPoint(
    UInt8 			functionAddress,
    UInt8			endpointNumber,
    UInt16			maxPacketSize,
    UInt8			speed,
    USBDeviceAddress            highSpeedHub,
    int                  	highSpeedPort,
    UInt8			direction,
    AppleEHCIQueueHead *	pED)
{
    AppleEHCIQueueHead *	CBED;
    UInt32 			cFlag;
    UInt32			myDataToggleCntrl;

    USBLog(7, "%s[%p]::AddEmptyCBEndPoint speed %d @(%d, %d)", getName(), this, speed, highSpeedHub, highSpeedPort);
   
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
    USBLog(1, "%s(%p)UIMCreateControlTransfer- calling the wrong method!", getName(), this);
    return kIOReturnIPCError;
}



void
AppleUSBEHCI::printAsyncQueue()
{
    AppleEHCIQueueHead *pED = _AsyncHead;
    
    if (pED)
    {
	USBLog(7, "%s[%p]::printAsyncQueue", getName(), this);
	while (pED)
	{
		//pED->print();
	    pED = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext);
	}
    }
    else
    {
	USBLog(7, "%s[%p]::printAsyncQueue: NULL Async Queue", getName(), this);
    }
}



void AppleUSBEHCI::printTD(EHCIGeneralTransferDescriptorPtr		pTD)
{
#if 1
	IOLog("print TD turned off\n");
#else
    if(pTD == 0)
    {
        IOLog("Attempt to print null TD\n");
        return;
    }
	USBLog(5, "%s[%p]::printED: ------pTD at %lx", getName(), this, (UInt32)pTD);
	USBLog(5, "%s[%p]::printED: nextTD:  %lx", getName(), this, USBToHostLong(pTD->pShared->nextTD));
	USBLog(5, "%s[%p]::printED: altTD:   %lx", getName(), this, USBToHostLong(pTD->pShared->altTD));
	USBLog(5, "%s[%p]::printED: flags:   %lx", getName(), this, USBToHostLong(pTD->pShared->flags));
	USBLog(5, "%s[%p]::printED: BuffPtr0: %lx", getName(), this, USBToHostLong(pTD->pShared->BuffPtr[0]));
	USBLog(5, "%s[%p]::printED: pEndpt:  %lx",  getName(), this, (UInt32)(pTD->pQH));
//	USBLog(5, "%s[%p]::printED: bufSiz:  %lx", getName(), this, (UInt32)(pTD->bufferSize));
	USBLog(5, "%s[%p]::printED: pPhys:   %lx", getName(), this, (UInt32)(pTD->pPhysical));
	USBLog(5, "%s[%p]::printED: pLogNxt: %lx", getName(), this, (UInt32)(pTD->pLogicalNext));
	USBLog(5, "%s[%p]::printED: logBf:   %lx", getName(), this, (UInt32)(pTD->logicalBuffer));
#endif

}


IOReturn 
AppleUSBEHCI::UIMCreateControlTransfer(
    short				functionAddress,
    short				endpointNumber,
    IOUSBCommand*			command,
    IOMemoryDescriptor*			CBP,
    bool				bufferRounding,
    UInt32				bufferSize,
    short				direction)
{
    AppleEHCIQueueHead *		pEDQueue;
    AppleEHCIQueueHead *		pEDDummy;
    IOReturn				status;
    IOUSBCompletion			completion = command->GetUSLCompletion();

    USBLog(7, "%s[%p]::UIMCreateControlTransfer adr=%d:%d cbp=%lx:%lx br=%s cback=[%p:%p] dir=%d)",
          getName(), this, functionAddress, endpointNumber, (UInt32)CBP, bufferSize,
          bufferRounding ? "YES":"NO", completion.target, completion.parameter, direction);
    
    // search for endpoint descriptor
    pEDQueue = FindControlBulkEndpoint(functionAddress, endpointNumber, &pEDDummy, kEHCIEDDirectionTD);
    USBLog(7, "%s[%p]::UIMCreateControlTransfer -- found endpoint at %p", getName(), this, pEDQueue);

    if (pEDQueue == NULL)
    {
        USBLog(3, "%s[%p]::UIMCreateControlTransfer- Could not find endpoint!", getName(), this);
        return kIOUSBEndpointNotFound;
    }


    status = allocateTDs(pEDQueue, command, CBP, bufferSize, direction, true);

    if(status == kIOReturnSuccess)
    {
	USBLog(7, "%s[%p]::UIMCreateControlTransfer allocateTDS done - CMD = %p, STS = %p", getName(), this, USBToHostLong(_pEHCIRegisters->USBCMD), USBToHostLong(_pEHCIRegisters->USBSTS));
	printAsyncQueue();
	EnableAsyncSchedule();
    }
    else
    {
        USBError(1, "%s[%p]::UIMCreateControlTransfer - allocateTDs returned error %x", getName(), this, (UInt32)status);
    }
    
    return status;
}



AppleEHCIQueueHead * 
AppleUSBEHCI::FindControlBulkEndpoint (
    short 			functionNumber, 
    short			endpointNumber, 
    AppleEHCIQueueHead *   	*pEDBack,
    short 			direction)
{
    UInt32			unique;
    AppleEHCIQueueHead *	pEDQueue;
    AppleEHCIQueueHead *	pEDQueueBack;
    short 			EDDirection;
	
    // search for endpoint descriptor
    unique = (UInt32) ((((UInt32) endpointNumber) << kEHCIEDFlags_ENPhase) | ((UInt32) functionNumber));
    
    
    pEDQueue = _AsyncHead;
    pEDQueueBack = NULL;
    if(pEDQueue == NULL)
    {
	USBLog(7, "%s[%p]::FindControlBulkEndpoint - pEDQueue is NULL", getName(), this);
	return NULL;
    }
    
    do {
	EDDirection = pEDQueue->direction;
	if( ( (USBToHostLong(pEDQueue->GetSharedLogical()->flags) & kUniqueNumNoDirMask) == unique) && ( ((EDDirection == kEHCIEDDirectionTD) || (EDDirection) == direction)) ) 
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
AppleUSBEHCI::allocateTDs(
    AppleEHCIQueueHead *		pEDQueue,
    IOUSBCommand 			*command,
    IOMemoryDescriptor 			*CBP,
    UInt32				bufferSize,
    UInt16				direction,
    Boolean				controlTransaction)
{

    EHCIGeneralTransferDescriptorPtr	pTD1, pTD, pTDnew, pTDLast;
    UInt32				myToggle = 0;
    UInt32				myDirection = 0;
    IOByteCount				transferOffset;
    UInt32				pageCount;
    UInt32				flags;
    IOReturn				status = kIOReturnSuccess;
    UInt32				maxPacket;
    UInt32  				bytesThisTD, segment;
    UInt32				curTDsegment;
    UInt32				totalPhysLength;
    IOPhysicalAddress			dmaStartAddr;
    UInt32				dmaStartOffset;
    UInt32				bytesToSchedule;
    bool				needNewTD = false;
    UInt32				maxTDLength;

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
	USBError(1, "%s[%p]::allocateTDs can't allocate 1st new TD", getName(), this);
	return kIOReturnNoMemory;
    }
    pTD = pTD1;	// We'll be working with pTD

    if (bufferSize != 0)
    {	    
        transferOffset = 0;
	curTDsegment = 0;
	bytesThisTD = 0;
        while (transferOffset < bufferSize)
        {
	    // first, calculate the maximum possible transfer of the given segment. note that this was already checked for
	    // being disjoint, so we don't have to worry about that.
	    dmaStartAddr = CBP->getPhysicalSegment(transferOffset, &totalPhysLength);
	    USBLog(7, "%s[%p]::allocateTDs - getPhysicalSegment returned length of %d (out of %d) and start of %p", getName(), this, totalPhysLength, bufferSize, dmaStartAddr);
	    dmaStartOffset = (dmaStartAddr & (kEHCIPageSize-1));
	    bytesToSchedule = 0;
	    
	    // only the first segment can start on a non page boundary
	    if ((curTDsegment == 0) || (dmaStartOffset == 0))
	    {
                needNewTD = false;
                
		if (totalPhysLength > bufferSize)
		{
		    USBLog(1, "%s[%p]::allocateTDs - segment physical length > buffer size - very strange", getName(), this);
		    totalPhysLength = bufferSize;
		}
		// each TD can transfer at most four full pages plus from the initial offset to the end of the first page
		maxTDLength = ((kEHCIPagesPerTD-curTDsegment) * kEHCIPageSize) - dmaStartOffset;
		if (totalPhysLength > maxTDLength)
		{
		    if ((curTDsegment == 0) && (dmaStartOffset != 0))
		    {
			// truncate this TD to exactly 4 pages, which will always be a multiple of MPS
			USBLog(7, "%s[%p]::allocateTDs - segment won't fit - using 4 pages for this TD", getName(), this);
			bytesToSchedule = (kEHCIPagesPerTD-1) * kEHCIPageSize;
		    }
		    else
		    {
			// truncate this TD to however many pages are left, which will always be a multiple of MPS
			USBLog(7, "%s[%p]::allocateTDs - segment is larger than 1 TD - using %d pages for this TD", getName(), this, kEHCIPagesPerTD - curTDsegment);
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
                    USBLog(6, "%s[%p]::allocateTDs - non-last transfer didn't end at end of page (%d, %d)", getName(), this, dmaStartOffset+bytesToSchedule);
                    needNewTD = true;
                }

                // now schedule all of the bytes I just discovered
		while (bytesToSchedule)
		{
		    pTD->pShared->BuffPtr[curTDsegment++] = HostToUSBLong(dmaStartAddr);
		    dmaStartAddr += (kEHCIPageSize - dmaStartOffset);
		    if (bytesToSchedule > (kEHCIPageSize - dmaStartOffset))
			bytesToSchedule -= (kEHCIPageSize - dmaStartOffset);
		    else
			bytesToSchedule = 0;
		    dmaStartOffset = 0;
		}

                if ( ((curTDsegment < kEHCIPagesPerTD) && (transferOffset < bufferSize)) && !needNewTD )
		{
		    USBLog(7, "%s[%p]::allocateTDs - didn't fill up this TD (segment %d) - going back for more", getName(), this, curTDsegment);
		    continue;
		}
	    }
	    
            flags = kEHCITDioc;				// Want to interrupt on completion
	    
	    USBLog(7, "%s[%p]::allocateTDs - i have %d bytes in %d segments", getName(), this, bytesThisTD, curTDsegment);
	    for (segment = 0; segment < curTDsegment; segment++)
	    {
		USBLog(7, "%s[%p]::allocateTDs - addr[%d]:%p", getName(), this, segment, USBToHostLong(pTD->pShared->BuffPtr[segment]));
	    }

            flags |= (bytesThisTD << kEHCITDFlags_BytesPhase);
            pTD->pShared->altTD = HostToUSBLong(pTD1->pPhysical);	// point alt to first TD, will be fixed up later
	    pTD->pType = (UInt32) kEHCIBulkTransferOutType;
	    flags |= myDirection | myToggle | kEHCITDStatus_Active | (3<<kEHCITDFlags_CerrPhase);
           
	    // this is for debugging
            // pTD->traceFlag = trace;
            pTD->traceFlag = false;
            pTD->pQH = pEDQueue;
          
           
            // only supply a callback when the entire buffer has been
            // transfered.
	    
	    USBLog(6, "%s[%p]::allocateTDs - putting command into TD (%p) on ED (%p)", getName(), this, pTD, pEDQueue);
	    pTD->command = command;				// Do like OHCI, link to command from each TD
            if (transferOffset >= bufferSize)
            {
		myToggle = 0;					// Only set toggle on first TD
		pTD->lastTDofTransaction = true;
		pTD->logicalBuffer = CBP;
		pTD->pShared->flags = HostToUSBLong(flags);
		//if(trace)printTD(pTD);
            }
	    else
            {
		pTD->lastTDofTransaction = false;
		myToggle = 0;	// Only set toggle on first TD
		pTDnew = AllocateTD();
		if (pTDnew == NULL)
		{
		    status = kIOReturnNoMemory;
		    USBError(1, "%s[%p]::allocateTDs can't allocate new TD", getName(), this);
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
		    USBLog(7, "%s[%p]::allocateTDs - got another TD - going to fill it up too (%d, %d)", getName(), this, transferOffset, bufferSize);
		}
            }
        }
    }
    else
    {
	// no buffer to transfer
	pTD->pShared->altTD = HostToUSBLong(pTD1->pPhysical);	// point alt to first TD, will be fixed up later
	USBLog(6, "%s[%p]::allocateTDs - (no buffer)- putting command into TD (%p) on ED (%p)", getName(), this, pTD, pEDQueue);
	pTD->command = command;
	pTD->lastTDofTransaction = true;
	pTD->logicalBuffer = CBP;
	// pTD->traceFlag = trace;
	pTD->traceFlag = false;
	pTD->pQH = pEDQueue;
	flags = kEHCITDioc | kEHCITDioc | myDirection | myToggle | kEHCITDStatus_Active | (3<<kEHCITDFlags_CerrPhase);
	// if(trace)printTD(pTD);
	pTD->pShared->flags = HostToUSBLong(flags);
    }

    pTDLast = pEDQueue->TailTD;
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
    pTDLast->pQH = pTD1->pQH;
    USBLog(7, "%s[%p]::allocateTDs - transfering command from TD (%p) to TD (%p)", getName(), this, pTD1, pTDLast);
    pTDLast->command = pTD1->command;
    pTDLast->lastTDofTransaction = pTD1->lastTDofTransaction;
    //pTDLast->bufferSize = pTD1->bufferSize;
    pTDLast->traceFlag = pTD1->traceFlag;
    pTDLast->pType = pTD1->pType;
    pTDLast->pLogicalNext = pTD1->pLogicalNext;
    pTDLast->logicalBuffer = pTD1->logicalBuffer;
    
    // Note not copying
    
    //  flags
    //  unused
    //  index      (pertains to TD)
    //  pPhysical  (pretains to TD)

    // Point end of new TDs to first TD, now new tail
    pTD->pShared->nextTD = HostToUSBLong(pTD1->pPhysical);
    pTD->pLogicalNext = pTD1;
    
    // squash pointers on new tail
    pTD1->pShared->nextTD = HostToUSBLong(kEHCITermFlag);
    pTD1->pShared->altTD = HostToUSBLong(kEHCITermFlag);
    pTD1->pLogicalNext = 0;
    USBLog(7, "%s[%p]::allocateTDs - zeroing out command in  TD (%p)", getName(), this, pTD1);
    pTD1->command = NULL;
    
    // This is (of course) copied from the 9 UIM. It has this cryptic note.
    // **** THIS NEEDS TO BE CHANGED, SEE NOTE
    // we have good status, so let's kick off the machine
    // Make new descriptor the tail
    pEDQueue->TailTD = pTD1;
    pTDLast->pShared->flags = flags;

    if (status)
    {
	USBLog(3, "%s[%p] CreateGeneralTransfer: returning status 0x%x", getName(), this, status);
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
	pEndpoint->responseToStall = 0;
	return	kIOReturnSuccess;
    }

    if( (flags & kEHCITDStatus_BuffErr) != 0)
    {	
	// Buffer over or under run error
	if( (flags & kEHCITDFlags_PID) == (1 << kEHCITDFlags_PIDPhase) )	
	{	
	    // An in token
	    return kOHCIITDConditionDataUnderrun;
	}
	else
	{	
	    // OUT/Status, data going out 
	    return kOHCIITDConditionDataUnderrun;
	}
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
		
		pEndpoint->responseToStall = 1 - pEndpoint->responseToStall;	// Switch 0 - 1
		if(pEndpoint->responseToStall == 1)
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

	return kOHCIITDConditionDeviceNotResponding;	// Can't tell this from any other transaction err
    }	
    
    if( (flags & kEHCITDStatus_PingState) != 0)			// Int/Isoc gave error to transaction translator
    {
	return kOHCIITDConditionDeviceNotResponding;
    }	

    USBLog(1, "mungeECHIStatus condition we're not expecting %x", flags);
    
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
    UInt32				flags, transferStatus;
    UInt32				bufferSizeRemaining = 0;
    EHCIGeneralTransferDescriptorPtr	nextTD;
    OSStatus				accumErr = kIOReturnSuccess;

    USBLog(7, "+%s[%p]::EHCIUIMDoDoneQueueProcessing", getName(), this);
    while (pHCDoneTD != NULL)
    {
        IOReturn	errStatus;
        if(pHCDoneTD == stopAt)
        {
            // Don't process this one or any further
            USBLog(5, "%s[%p]::EHCIUIMDoDoneQueueProcessing stop at %p",getName(), this, pHCDoneTD);
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
            // forceErr = transferStatus;					// Give this to the whole transaction
            if (transferStatus)
            {
                USBLog(4, "%s[%p]::EHCIUIMDoDoneQueueProcessing - TD (%p) - got transferStatus %p with flags (%p)", getName(), this, pHCDoneTD, transferStatus, flags);
            }
            errStatus = TranslateStatusToUSBError(transferStatus);
            accumErr = errStatus;
            if (errStatus)
            {
                USBLog(4, "%s[%p]::EHCIUIMDoDoneQueueProcessing - got errror %p", getName(), this, errStatus);
            }
        }

        if (pHCDoneTD->pType == kEHCIIsochronousType)
        {
            USBError (1, "%s[%p]::EHCIUIMDoDoneQueueProcessing we're not expecting this (pHCDoneTD->pType == kEHCIIsochronousType)", getName(), this);
            // cast to a isoc type
            //pITD = (OHCIIsochTransferDescriptorPtr) pHCDoneTD;
            //ProcessCompletedITD(pITD, errStatus);
            // deallocate td
            //DeallocateITD(pITD);
        }
        else
        {
            bufferSizeRemaining += (flags & kEHCITDFlags_Bytes) >> kEHCITDFlags_BytesPhase;

            if (pHCDoneTD->lastTDofTransaction)
            {
                if ( pHCDoneTD->command == NULL )
                {
                    // IOPanic("pHCDoneTD->command is NULL in EHCIUIMDoneQueueProcessing");
                    USBError (1, "%s[%p]::EHCIUIMDoDoneQueueProcessing pHCDoneTD->command is NULL (%p)", getName(), this, pHCDoneTD);
                }
                else
                {
                    IOUSBCompletion completion = pHCDoneTD->command->GetUSLCompletion();
                    if(!safeAction || (safeAction == completion.action))
                    {
                        // remove flag before completing
                        pHCDoneTD->lastTDofTransaction = 0;

                        Complete(completion, errStatus, bufferSizeRemaining);
                        bufferSizeRemaining = 0;	// So next transaction starts afresh.
                        accumErr = kIOReturnSuccess;
                    }
                    else
                    {	// what about buffer remaining here????
                        if(_pendingHead)
                            _pendingTail->pLogicalNext = pHCDoneTD;
                        else
                            _pendingHead = pHCDoneTD;
                        _pendingTail = pHCDoneTD;
                    }
                }
            }
            pHCDoneTD->logicalBuffer = NULL;
	    USBLog(7, "%s[%p]::EHCIUIMDoDoneQueueProcessing - deallocating TD (%p)", getName(), this, pHCDoneTD);
            DeallocateTD(pHCDoneTD);
        }
        pHCDoneTD = nextTD;	// New qHead
    }

    USBLog(7, "-%s[%p]::EHCIUIMDoDoneQueueProcessing", getName(), this);
    return(kIOReturnSuccess);
}


IOReturn
AppleUSBEHCI::scavengeIsocTransactions(IOUSBCompletionAction safeAction)
{
    AppleEHCIIsochListElement 		*pDoneEl;
    UInt32				cachedProducer;
    UInt32				cachedConsumer;
    AppleEHCIIsochEndpointPtr 		pEP;
    AppleEHCIIsochListElement		*prevEl;
    AppleEHCIIsochListElement		*nextEl;
    IOInterruptState			intState;

    // Get the values of the Done Queue Head and the producer count.  We use a lock and disable interrupts
    // so that the filter routine does not preempt us and updates the values while we're trying to read them.
    //
    intState = IOSimpleLockLockDisableInterrupt( _wdhLock );
    
    pDoneEl = (AppleEHCIIsochListElement*)_savedDoneQueueHead;
    cachedProducer = _producerCount;
    
    IOSimpleLockUnlockEnableInterrupt( _wdhLock, intState );
    
    cachedConsumer = _consumerCount;

    if (pDoneEl && (cachedConsumer != cachedProducer))
    {
	// there is real work to do - first reverse the list
	prevEl = NULL;
	USBLog(7, "%s[%p]::scavengeIsocTransactions - before reversal, cachedConsumer = 0x%x", getName(), this, cachedConsumer);
	while (true)
	{
	    pDoneEl->_logicalNext = prevEl;
	    prevEl = pDoneEl;
	    cachedConsumer++;
	    
	    if ( cachedProducer == cachedConsumer)
		break;
	
	    pDoneEl = pDoneEl->doneQueueLink;
	}

	// update the consumer count
	_consumerCount = cachedConsumer;

	USBLog(7, "%s[%p]::scavengeIsocTransactions - after reversal, cachedConsumer = 0x%x", getName(), this, cachedConsumer);
	// now cachedDoneQueueHead points to the head of the done queue in the right order
	while (pDoneEl)
	{
	    nextEl = OSDynamicCast(AppleEHCIIsochListElement, pDoneEl->_logicalNext);
	    pDoneEl->_logicalNext = NULL;
	    USBLog(7, "%s[%p]::scavengeIsocTransactions - about to scavenge TD %p", getName(), this, pDoneEl);
	    scavengeAnIsocTD(pDoneEl, safeAction);
	    pDoneEl = nextEl;
	}
    }
    
    pEP = _isochEPList;
    while (pEP)
    {
	AddIsocFramesToSchedule(pEP);
	pEP = pEP->nextEP;
    }
    return kIOReturnSuccess;
		
}

IOReturn
AppleUSBEHCI::scavengeAnIsocTD(AppleEHCIIsochListElement *pTD, IOUSBCompletionAction safeAction)
{
    AppleEHCIIsochEndpointPtr 		pEP;
    IOReturn				ret;

    pEP = pTD->myEndpoint;
	
    if(pEP == NULL)
    {
	USBError(1, "%s[%p]::scavengeAnIsocEndPoint - could not find endpoint associated with iTD (%p)", getName(), this, pTD->myEndpoint);
    }
    else
    {	
	if (!pTD->lowLatency)
	    ret = pTD->UpdateFrameList();		// TODO - accumulate the return values
	PutTDonDoneQueue(pEP, pTD);
    }
    
    if(pTD->completion.action != NULL)
    {
	ReturnIsocDoneQueue(pEP);
    }
	
    return(kIOReturnSuccess);
}



void
AppleUSBEHCI::PutTDonToDoList(AppleEHCIIsochEndpointPtr pED, AppleEHCIIsochListElement *pTD)
{
    USBLog(7, "%s[%p]::PutTDonToDoList - pED (%p) pTD (%p) frameNumber(%Lx)", getName(), this, pED, pTD, pTD->frameNumber);
    // Link TD into todo list
    if(pED->toDoList == NULL)
    {
	// as the head of a new list
	pED->toDoList = pTD;
    }
    else
    {
	// at the tail of the old list
	pED->toDoEnd->_logicalNext = pTD;
    }
    // no matter what we are the new tail
    pED->toDoEnd = pTD;
    pED->activeTDs++;
}



AppleEHCIIsochListElement *
AppleUSBEHCI::GetTDfromToDoList(AppleEHCIIsochEndpointPtr pED)
{
    AppleEHCIIsochListElement	*pTD;
    
    pTD = pED->toDoList;
    if (pTD)
    {
	if (pTD == pED->toDoEnd)
	    pED->toDoList = pED->toDoEnd = NULL;
	else
	    pED->toDoList = OSDynamicCast(AppleEHCIIsochListElement, pTD->_logicalNext);
    }
    return pTD;
}



void
AppleUSBEHCI::PutTDonDoneQueue(AppleEHCIIsochEndpointPtr pED, AppleEHCIIsochListElement *pTD)
{
    if(pED->doneQueue == NULL)
    {
	// as the head of a new list
	pED->doneQueue = pTD;
    }
    else
    {
	// at the tail of the old list
	pED->doneEnd->_logicalNext = pTD;
    }
    // and not matter what we are now the new tail
    pED->doneEnd = pTD;
	
}



AppleEHCIIsochListElement *
AppleUSBEHCI::GetTDfromDoneQueue(AppleEHCIIsochEndpointPtr pED)
{
    AppleEHCIIsochListElement	*pTD;
    
    pTD = pED->doneQueue;
    if (pTD)
    {
	if (pTD == pED->doneEnd)
	    pED->doneQueue = pED->doneEnd = NULL;
	else
	    pED->doneQueue = OSDynamicCast(AppleEHCIIsochListElement, pTD->_logicalNext);
	pED->activeTDs--;
    }
    return pTD;
}



IOReturn
AppleUSBEHCI::scavengeAnEndpointQueue(AppleEHCIListElement *pEDQueue, IOUSBCompletionAction safeAction)
{
    EHCIGeneralTransferDescriptorPtr	doneQueue = NULL, doneTail= NULL, qHead, qTD, qEnd;
    UInt32 				flags, count = 0;
    Boolean 				TDisHalted, shortTransfer;
    AppleEHCIQueueHead			*pQH;
    
    while( (pEDQueue != NULL) && (count++ < 150000) )
    {
	pQH = OSDynamicCast(AppleEHCIQueueHead, pEDQueue);
	if(pQH)
	{
	    qTD = qHead = pQH->qTD;
	    qEnd = pQH->TailTD;
	    TDisHalted = false;
	    shortTransfer = false;
	    while( (qTD != qEnd)  && (count++ < 150000) )
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
		if(qTD->lastTDofTransaction)
		{
		    // We have the complete command
	
		    USBLog(7, "%s[%p]::scavengeAnEndpointQueue - TD (%p) on ED (%p)", getName(), this, qTD, pQH); 
		    if(doneQueue == NULL)
		    {
			doneQueue = qHead;
		    }
		    else
		    {
			doneTail->pLogicalNext = qHead;
		    }
		    doneTail = qTD;
		    qTD = qTD->pLogicalNext;
		    qHead = qTD;
		    doneTail->pLogicalNext = NULL;
		    pQH->qTD = qTD;
		    
		    if(TDisHalted)
		    {
			flags = USBToHostLong(qTD->pShared->flags);
			pQH->GetSharedLogical()->NextqTDPtr = HostToUSBLong( qTD->pPhysical);
			if((flags & kEHCITDStatus_Active) != 0)	// Next command is still active
			{
			    break;
			}
		    }

                    // Reset our loop variables
                    //
                    TDisHalted = false;
                    shortTransfer = false;
                } 
		else
		{
		    USBLog(7, "%s[%p]::scavengeAnEndpointQueue - looking past TD (%p) on ED (%p)", getName(), this, qTD, pQH); 
		    qTD = qTD->pLogicalNext;
		}
	    }
	}
	pEDQueue = pEDQueue->_logicalNext;
    }
    if(doneQueue != NULL)
    {
	EHCIUIMDoDoneQueueProcessing(doneQueue, kIOReturnSuccess, safeAction, NULL);
    }
    if(count > 1000)
    {
	USBLog(1, "%s[%p]::scavengeAnEndpointQueue looks like bad ed queue %lx", getName(), this, count);
    }
    
    return kIOReturnSuccess;
}



void
AppleUSBEHCI::scavengeCompletedTransactions(IOUSBCompletionAction safeAction)
{
    AppleEHCIListElement 	**pEDQueue;
    IOReturn 			err, err1;
    int 			i;

    safeAction = 0;

    err = scavengeIsocTransactions(safeAction);
    if(err != kIOReturnSuccess)
    {
	USBLog(1, "%s[%p]::scavengeCompletedTransactions err isoch list %x", getName(), this, err);
    }

	if ( _AsyncHead != NULL )
	{
		err = scavengeAnEndpointQueue(_AsyncHead, safeAction);
		if(err != kIOReturnSuccess)
		{
		USBLog(1, "%s[%p]::scavengeCompletedTransactions err async queue %x", getName(), this, err);
		}
    }

    if ( _logicalPeriodicList != NULL )
    {
        pEDQueue = _logicalPeriodicList;
        for(i= 0; i<kEHCIPeriodicListEntries; i++)
        {
            if(*pEDQueue != NULL)
            {
                err1 = scavengeAnEndpointQueue(*pEDQueue, safeAction);
                if(err1 != kIOReturnSuccess)
                {
                    err = err1;
                    USBLog(1, "%s[%p]::scavengeCompletedTransactions err periodic queue %lx", getName(), this, err);
                }
            }
            pEDQueue++;
        }
    }
}


IOReturn 
AppleUSBEHCI::UIMCreateControlTransfer(
    short				functionAddress,
    short				endpointNumber,
    IOUSBCommand*			command,
    void*				CBP,
    bool				bufferRounding,
    UInt32				bufferSize,
    short				direction)
{
    IOMemoryDescriptor *		desc = NULL;
    IODirection				descDirection;
    IOReturn				status;
    IOUSBCompletion			completion = command->GetUSLCompletion();

    USBLog(7, "%s[%p]::UIMCreateControlTransfer - CrntlTx: adr=%d:%d cbp=%lx:%lx br=%s cback=[%lx:%lx] dir=%d)", getName(), this, 
          functionAddress, endpointNumber, (UInt32)CBP, bufferSize,
          bufferRounding?"YES":"NO", (UInt32)completion.target, (UInt32)completion.parameter, direction);
       
    if (direction == kUSBOut)
        descDirection = kIODirectionOut;
    else if (direction == kUSBIn)
        descDirection = kIODirectionIn;
    else
        descDirection = kIODirectionOut;
    if(bufferSize != 0) 
    {
        desc = IOMemoryDescriptor::withAddress(CBP, bufferSize, descDirection);
        if(!desc)
        {
	    USBError(1, "%s[%p]::UIMCreateControlTransfer - IOMemoryDescriptor::withAddress failed", getName(), this);
	    return kIOReturnNoMemory;
        }
    }

    USBLog(7, "%s[%p]::UIMCreateControlTransfer - calling UIMCreateControlTransfer0 with mem desc", getName(), this);

    status = UIMCreateControlTransfer(functionAddress, endpointNumber, command, desc, bufferRounding, bufferSize, direction);

    if(desc)
        desc->release();

    return status;
}





IOReturn 
AppleUSBEHCI::UIMCreateControlTransfer(
    short				functionAddress,
    short				endpointNumber,
    IOUSBCompletion			completion,
    IOMemoryDescriptor*			CBP,
    bool				bufferRounding,
    UInt32				bufferSize,
    short				direction)
{
    USBLog(1, "%s[%p] UIMCreateControlTransfer- calling the wrong method!", getName(), this);
    return kIOReturnIPCError;
}



IOReturn
AppleUSBEHCI::UIMCreateBulkEndpoint(
    UInt8				functionAddress,
    UInt8				endpointNumber,
    UInt8				direction,
    UInt8				speed,
    UInt8				maxPacketSize)
{
    USBLog(1, "%s[%p] UIMCreateBulkEndpoint- calling the wrong method!", getName(), this);
    return kIOReturnInternalError;
}



IOReturn
AppleUSBEHCI::UIMCreateBulkEndpoint(
    UInt8			functionAddress,
    UInt8			endpointNumber,
    UInt8			direction,
    UInt8			speed,
    UInt16			maxPacketSize,
    USBDeviceAddress    	highSpeedHub,
    int                 	highSpeedPort)
{
    AppleEHCIQueueHead 		*pEHCIEndpointDescriptor;
    AppleEHCIQueueHead		*pED;
    
    USBLog(7, "%s[%p]::UIMCreateBulkEndpoint(adr=%d:%d, max=%d, dir=%d)", getName(), this, functionAddress, endpointNumber, maxPacketSize, direction);

    if( (direction != kUSBOut) && (direction != kUSBIn) )
    {
	USBLog(1, "%s[%p]::UIMCreateBulkEndpoint - wrong direction %d", getName(), this, direction);
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
	USBLog(1, "%s[%p]::UIMCreateBulkEndpoint - ERROR converting HS maxPacketSize from %d to 512", getName(), this, maxPacketSize);
        maxPacketSize = 512;
    }

    pEHCIEndpointDescriptor = AddEmptyCBEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction, pED);
    if (pEHCIEndpointDescriptor == NULL)
	    return kIOReturnNoResources;

    if(pED == NULL)
    {
	_AsyncHead = pEHCIEndpointDescriptor;
	_pEHCIRegisters->AsyncListAddr = USBToHostLong( pEHCIEndpointDescriptor->_sharedPhysical );
    }

    return kIOReturnSuccess;

}



IOReturn 
AppleUSBEHCI::UIMCreateBulkTransfer(IOUSBCommand* command)
{
    AppleEHCIQueueHead 			*pEDQueue;
    AppleEHCIQueueHead 			*pEDDummy;
    IOReturn				status;
    IOMemoryDescriptor*			buffer = command->GetBuffer();
    short				direction = command->GetDirection();

    USBLog(7, "%s[%p]::UIMCreateBulkTransfer - adr=%d:%d cbp=%lx:%x cback=[%lx:%lx:%lx] dir=%d)", getName(), this,
	command->GetAddress(), command->GetEndpoint(), (UInt32)buffer, (int)buffer->getLength(), 
	(UInt32)command->GetUSLCompletion().action, (UInt32)command->GetUSLCompletion().target, (UInt32)command->GetUSLCompletion().parameter, direction);
    
    pEDQueue = FindControlBulkEndpoint(command->GetAddress(), command->GetEndpoint(), &pEDDummy, direction);
    
    if (pEDQueue == NULL)
    {
        USBLog(3, "%s[%p]::UIMCreateControlTransfer- Could not find endpoint!", getName(), this);
        return kIOUSBEndpointNotFound;
    }


    status = allocateTDs(pEDQueue, command, buffer, buffer->getLength(), direction, false );
    if(status == kIOReturnSuccess)
	EnableAsyncSchedule();
    else
    {
        USBLog(1, "%s[%p]::UIMCreateControlTransfer- allocateTDs returned error %x", getName(), this, (UInt32)status);
    }
    
    return status;

}


IOReturn 
AppleUSBEHCI::UIMCreateBulkTransfer(
    short				functionAddress,
    short				endpointNumber,
    IOUSBCompletion			completion,
    IOMemoryDescriptor *		CBP,
    bool				bufferRounding,
    UInt32				bufferSize,
    short				direction)
{
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "%s(%p)UIMCreateBulkTransfer- calling the wrong method!", getName(), this);
    return kIOReturnIPCError;
}



UInt16
AppleUSBEHCI::validatePollingRate(short rawPollingRate,  short speed, int *offset, UInt16 *bytesAvailable)
{
    UInt16 	pollingRate;
    int 	perFrame, i;
    UInt16	availableBandwidth[kEHCIMaxPoll];

    pollingRate = rawPollingRate;

    if(speed == kUSBDeviceSpeedHigh)
    {
	USBLog(5,"%s[%p]::validatePollingRate HS pollingRate raw: %d", getName(), this, pollingRate);
	if(pollingRate <= 3)
	{
	    perFrame = 1<< (pollingRate-1);
	    pollingRate = 1;
	}
	else
	{
	    if(pollingRate > 16)
	    {
		    //return(paramErr);
		    pollingRate = 16;
	    }
	    perFrame = 1;		
	    pollingRate = 1 << (pollingRate - 3);
	}
	if(pollingRate > kEHCIMaxPoll)
	{
		pollingRate = kEHCIMaxPoll;
	}
	USBLog(5,"%s[%p]::validatePollingRate HS pollingRate cooked: %d", getName(), this, pollingRate);
    }
    else
    {
	// full/low speed device
	int	count = 0;
	perFrame = 1;
	// pollingRate is good
	USBLog(5,"%s[%p]::validatePollingRate LS/FS pollingRate raw: %d", getName(), this, pollingRate);

	// Find the next lowest power of 2 for the polling rate
	while( (pollingRate >> count) != 1)
	    count++;

	pollingRate = (1 <<  count);

	if(pollingRate > 255)
	{
	    USBLog(5,"%s[%p]::validatePollingRate -  Oops LS/FS pollingRate too high: %d, setting to 128", getName(), this, pollingRate);
	    pollingRate = 128;
	}
	else if( (pollingRate < 8) && (speed == kUSBDeviceSpeedLow) )
	{
	    USBLog(5,"%s[%p]::validatePollingRate Oops LS pollingRate too low: %d, setting to 8.", getName(), this, pollingRate);
	    pollingRate = 8;
	}
	else if(pollingRate == 0)
	{
	    USBLog(5,"%s[%p]::validatePollingRate Oops HS pollingRate too low: %d, setting to 1.", getName(), this, pollingRate);
	    pollingRate = 1;
	}
	USBLog(5, "%s[%p]::validatePollingRate LS/FS pollingRate cooked: %d", getName(), this, pollingRate);
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


static AppleEHCIListElement* 
FindIntEDqueue(AppleEHCIListElement *start, UInt8 pollingRate)
{
    AppleEHCIQueueHead 		*pQH;
    AppleEHCIIsochListElement	*pIsoch = OSDynamicCast(AppleEHCIIsochListElement, start);
    
    if (pIsoch)
    {
	// the list starts with some Isoch elements and we need to find the end of them
	while (OSDynamicCast(AppleEHCIIsochListElement, start->_logicalNext))
	{
	    pIsoch = OSDynamicCast(AppleEHCIIsochListElement, start->_logicalNext);
	    start = start->_logicalNext;
	}
	// now pIsoch and start both point to the final Isoch element in the list
	start = start->_logicalNext;
    }
    
    // so either we went through the above loop or pIsoch is NULL. if start is also NULL, then we are 
    // at a list which consists of 0 or more Isoch elements
    if (!start)
	return pIsoch;

    pQH = OSDynamicCast(AppleEHCIQueueHead, start);
    
    pollingRate--;
    if(pQH && (pQH->pollM1 < pollingRate))
    {
	return pIsoch;
    }
	    
    while(start->_logicalNext != NULL)
    {
	pQH = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
	if(pQH && (pQH->pollM1 < pollingRate))
	{
	    break;
	}
	start = start->_logicalNext;
    }

    return start;
}


void  AppleUSBEHCI::linkInterruptEndpoint(AppleEHCIQueueHead *pEHCIEndpointDescriptor)
{
short pollingRate;
UInt16 maxPacketSize;
int offset;
AppleEHCIListElement *	pLE;
UInt32 newHorizPtr;

    maxPacketSize = pEHCIEndpointDescriptor->maxPacketSize;
    offset = pEHCIEndpointDescriptor->offset;
    pollingRate = pEHCIEndpointDescriptor->pollM1+1;

    newHorizPtr = HostToUSBLong( pEHCIEndpointDescriptor->GetPhysicalAddrWithType());
    while( offset < kEHCIPeriodicListEntries)
    {
	pLE = FindIntEDqueue(_logicalPeriodicList[offset], pollingRate);
	if(pLE == NULL)
	{	// Insert as first in queue
	    if(pEHCIEndpointDescriptor->_logicalNext == NULL)
	    {
		pEHCIEndpointDescriptor->_logicalNext = _logicalPeriodicList[offset];
		pEHCIEndpointDescriptor->GetSharedLogical()->nextQH = _periodicList[offset];
	    }
	    else
	    {
		USBError(1, "%s[%p]::linkInterruptEndpoint(%p) - _logicalNext is not NULL! (%p)", getName(), this, pEHCIEndpointDescriptor, pEHCIEndpointDescriptor->_logicalNext);
	    }
	    
	    _logicalPeriodicList[offset] = pEHCIEndpointDescriptor;
	    _periodicList[offset] = newHorizPtr;
	    USBLog(7, "%s[%p]::linkInterruptEndpoint - inserted at top of list %d - next logical (%p) next physical (%p)", getName(), this, offset, pEHCIEndpointDescriptor->_logicalNext, USBToHostLong(pEHCIEndpointDescriptor->GetSharedLogical()->nextQH));
	}
	else if (pEHCIEndpointDescriptor != pLE)
	{	// Insert in middle/end of queue
		
	    if(pEHCIEndpointDescriptor->_logicalNext == NULL)		// JRH - again - how could this not be NULL?
	    {
		pEHCIEndpointDescriptor->_logicalNext = pLE->_logicalNext;
		pEHCIEndpointDescriptor->GetSharedLogical()->nextQH =  pLE->GetPhysicalLink();
	    }
	    else
	    {
		USBError(1, "%s[%p]::linkInterruptEndpoint(%p) - _logicalNext is not NULL! (%p)", getName(), this, pEHCIEndpointDescriptor, pEHCIEndpointDescriptor->_logicalNext);
	    }
	    // Point queue element to new endpoint
	    pLE->_logicalNext = pEHCIEndpointDescriptor;
	    pLE->SetPhysicalLink(newHorizPtr);
	    USBLog(7, "%s[%p]::linkInterruptEndpoint - inserted into list %d - next logical (%p) next physical (%p)", getName(), this, offset, pEHCIEndpointDescriptor->_logicalNext, USBToHostLong(pEHCIEndpointDescriptor->GetSharedLogical()->nextQH));
	}
	else
	{
	    // Else was already linked
	    USBLog(7, "%s[%p]::linkInterruptEndpoint - (%p) already linked into %d (%p)", getName(), this, pEHCIEndpointDescriptor, offset, pLE);
	}
	
	// this gets repeated, but we only subtract up to MaxPoll
	if (offset < kEHCIMaxPoll)
	    _periodicBandwidth[offset] -= maxPacketSize;
	offset += pollingRate;
    } 
    _periodicEDsInSchedule++;
}

IOReturn 
AppleUSBEHCI::UIMCreateInterruptEndpoint(
    short		functionAddress,
    short		endpointNumber,
    UInt8		direction,
    short		speed,
    UInt16		maxPacketSize,
    short		pollingRate)
{
    USBError(1, "%s[%p]::UIMCreateInterruptEndpoint - old version called with no split params");
    return kIOReturnInternalError;
}


IOReturn 
AppleUSBEHCI::UIMCreateInterruptEndpoint(
    short			functionAddress,
    short			endpointNumber,
    UInt8			direction,
    short			speed,
    UInt16			maxPacketSize,
    short			pollingRate,
    USBDeviceAddress    	highSpeedHub,
    int                 	highSpeedPort)
{
    int 			offset;
    UInt16			availableBandwidth;
//    UInt32 			newHorizPtr;
    AppleEHCIQueueHead *	pEHCIEndpointDescriptor;
    AppleEHCIListElement *	pLE;
    AppleUSBEHCIHubInfoPtr	hiPtr;

    if (_rootHubFuncAddress == functionAddress)
        return kIOReturnSuccess;

    USBLog(7, "%s[%p]::UIMCreateInterruptEndpoint (%d, %d, %s, %d, %d)", getName(), this,
          functionAddress, endpointNumber, (speed == kUSBDeviceSpeedLow) ? "lo" : "full", maxPacketSize,
          pollingRate);

    if( (speed == kUSBDeviceSpeedLow) && (maxPacketSize > 8) )
    {
	USBLog (1, "%s[%p]::UIMCreateInterruptEndpoint - incorrect max packet size (%d) for low speed", getName(), this, maxPacketSize);
	return kIOReturnBadArgument;
    }
    
    if(pollingRate == 0)
	return(kIOReturnBadArgument);

    // find my offset and check the HIGH SPEED bandwidth to the hub
    pollingRate = validatePollingRate(pollingRate, speed, &offset, &availableBandwidth);
        
    if (maxPacketSize > availableBandwidth)
    {
	USBLog (1, "%s[%p]::UIMCreateInterruptEndpoint - no bandwidth", getName(), this);
	return kIOReturnNoBandwidth;
    }
    
    if (highSpeedHub)
    {
	// get the "master" hub Info for this hub to check the flags
	hiPtr = GetHubInfo(highSpeedHub, 0);
	if (!hiPtr)
	{
	    // if there is no pointer, then i have to assume that this is a single TT hub
	    hiPtr = NewHubInfo(highSpeedHub, 0);
	    if (!hiPtr)
		return kIOReturnInternalError;
	    USBLog (1, "%s[%p]::UIMCreateInterruptEndpoint - setting new hub info(%p) bandwidth to 1024", getName(), this, hiPtr);
	    hiPtr->bandwidthAvailable = 1024;
	}
	if (hiPtr->flags & kUSBEHCIFlagsMuliTT)
	{
	    // this is a multiTT hub
	    hiPtr = GetHubInfo(highSpeedHub, highSpeedPort);
	    if (!hiPtr)
	    {
		// if there is no pointer, then i have to assume that this is a single TT hub
		hiPtr = NewHubInfo(highSpeedHub, highSpeedPort);
		if (!hiPtr)
		    return kIOReturnInternalError;
		USBLog (1, "%s[%p]::UIMCreateInterruptEndpoint - setting new hub info(%p) bandwidth to 1024", getName(), this, hiPtr);
		hiPtr->bandwidthAvailable = 1024;
	    }
	}
	
	if (maxPacketSize > hiPtr->bandwidthAvailable)
	{
	    USBLog (1, "%s[%p]::UIMCreateInterruptEndpoint - split transaction - no bandwidth on hub or port (MPS %d, bw %d)", getName(), this, maxPacketSize, hiPtr->bandwidthAvailable);
	    return kIOReturnNoBandwidth;
	}
    
	hiPtr->bandwidthAvailable -= maxPacketSize;
	USBLog(5, "%s[%p]::UIMCreateInterruptEndpoint - using offset %d with HS bandwidth of %d and hub/port bandwidth of %d", getName(), this, offset, availableBandwidth, hiPtr->bandwidthAvailable);
    }
    else
    {
	USBLog(5, "%s[%p]::UIMCreateInterruptEndpoint - using offset %d with HS bandwidth of %d", getName(), this, offset, availableBandwidth);
    }
    

    pEHCIEndpointDescriptor = MakeEmptyIntEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
             
    if (pEHCIEndpointDescriptor == NULL)
    {
        USBError(1, "%s[%p]::UIMCreateInterruptEndpoint - could not create empty endpoint", getName(), this);
	return kIOReturnNoResources;
    }

    pEHCIEndpointDescriptor->pollM1 = pollingRate-1;
    pEHCIEndpointDescriptor->offset = offset;
    pEHCIEndpointDescriptor->maxPacketSize = maxPacketSize;

    if(_greatestPeriod < pollingRate)
    {
	_greatestPeriod = pollingRate;
    }

    linkInterruptEndpoint(pEHCIEndpointDescriptor);

#if 0
    // Point first endpoint to itself

    UInt32 			newHorizPtr;
    newHorizPtr = HostToUSBLong( pEHCIEndpointDescriptor->GetPhysicalAddrWithType());

    USBLog(5, "+%s[%p]::UIMCreateInterruptEndpoint - about to put new int ED %p (physical %p) into periodic list", getName(), this, pEHCIEndpointDescriptor, USBToHostLong(newHorizPtr));
    while( offset < kEHCIPeriodicListEntries)
    {
	pLE = FindIntEDqueue(_logicalPeriodicList[offset], pollingRate);
	if(pLE == NULL)
	{	// Insert as first in queue
	    if(pEHCIEndpointDescriptor->_logicalNext == NULL)		// JRH - how could this not be NULL?
	    {
		pEHCIEndpointDescriptor->_logicalNext = _logicalPeriodicList[offset];
		pEHCIEndpointDescriptor->GetSharedLogical()->nextQH = _periodicList[offset];
	    }
	    
	    _logicalPeriodicList[offset] = pEHCIEndpointDescriptor;
	    _periodicList[offset] = newHorizPtr;
	    USBLog(7, "%s[%p]::UIMCreateInterruptEndpoint - inserted at top of list %d - next logical (%p) next physical (%p)", getName(), this, offset, pEHCIEndpointDescriptor->_logicalNext, USBToHostLong(pEHCIEndpointDescriptor->GetSharedLogical()->nextQH));
	}
	else if (pEHCIEndpointDescriptor != pLE)
	{	// Insert in middle/end of queue
		
	    if(pEHCIEndpointDescriptor->_logicalNext == NULL)		// JRH - again - how could this not be NULL?
	    {
		pEHCIEndpointDescriptor->_logicalNext = pLE->_logicalNext;
		pEHCIEndpointDescriptor->GetSharedLogical()->nextQH =  pLE->GetPhysicalLink();
	    }
	    // Point queue element to new endpoint
	    pLE->_logicalNext = pEHCIEndpointDescriptor;
	    pLE->SetPhysicalLink(newHorizPtr);
	    USBLog(7, "%s[%p]::UIMCreateInterruptEndpoint - inserted into list %d - next logical (%p) next physical (%p)", getName(), this, offset, pEHCIEndpointDescriptor->_logicalNext, USBToHostLong(pEHCIEndpointDescriptor->GetSharedLogical()->nextQH));
	}
	else
	{
	    // Else was already linked
	    USBLog(7, "%s[%p]::UIMCreateInterruptEndpoint - (%p) already linked into %d (%p)", getName(), this, pEHCIEndpointDescriptor, offset, pLE);
	}
	
	// this gets repeated, but we only subtract up to MaxPoll
	if (offset < kEHCIMaxPoll)
	    _periodicBandwidth[offset] -= maxPacketSize;
	offset += pollingRate;
    } 
    _periodicEDsInSchedule++;
#endif
    USBLog(5, "-%s[%p]::UIMCreateInterruptEndpoint", getName(), this);
    return kIOReturnSuccess;
}


IOReturn 
AppleUSBEHCI::UIMCreateIsochEndpoint(
    short		functionAddress,
    short		endpointNumber,
    UInt32		maxPacketSize,
    UInt8		direction)
{
    USBError(1, "%s[%p]::UIMCreateIsochEndpoint -- old version called with no split params", getName(), this);
    return kIOReturnBadArgument;
}



IOReturn 
AppleUSBEHCI::UIMCreateIsochEndpoint(
    short			functionAddress,
    short			endpointNumber,
    UInt32			maxPacketSize,
    UInt8			direction,
    USBDeviceAddress    	highSpeedHub,
    int               		highSpeedPort)
{
    AppleEHCIIsochEndpointPtr	pEP;
    UInt32			curMaxPacketSize;
    AppleUSBEHCIHubInfoPtr	hiPtr;
    UInt32			xtraRequest;
    UInt16 			*bandWidthAvailPtr;
    UInt16 			highSpeedBandWidth;
    
    // we do not create an isoch endpoint in the hardware itself. isoch transactions are handled by 
    // TDs linked firectly into the Periodic List. There are two types of isoch TDs - split (siTD)
    // and high speed (iTD). This method will allocate an internal data structure which will be used
    // to track bandwidth, etc.

    USBLog(7, "%s[%p]::UIMCreateIsochEndpoint(%d, %d, %d, %d, %d, %d)", getName(), this, functionAddress, endpointNumber, maxPacketSize, direction, highSpeedHub, highSpeedPort);
    
    if (highSpeedHub == 0)
    {
	USBLog (1, "%s[%p]::UIMCreateIsochEndpoint - high speed", getName(), this);
//	return kIOReturnSuccess;
    }

    if (highSpeedHub == 0)
    {
        // Use a UInt16 to keep track of isoc bandwidth, then you can use
        // a pointer to it or to hiPtr->bandwidthAvailable for full speed.
        highSpeedBandWidth = _isochBandwidthAvail;
        bandWidthAvailPtr = &highSpeedBandWidth;
    }
    else
    {
        // this is for full speed devices only - when HS is implemented, it should not use this code
        hiPtr = GetHubInfo(highSpeedHub, 0);
        if (!hiPtr)
        {
            // if there is no pointer, then i have to assume that this is a single TT hub
            hiPtr = NewHubInfo(highSpeedHub, 0);
            if (!hiPtr)
                return kIOReturnInternalError;
            USBLog (1, "%s[%p]::UIMCreateIsochEndpoint - setting new hub info(%p) bandwidth to 1024", getName(), this, hiPtr);
            hiPtr->bandwidthAvailable = 1024;
        }
        if (hiPtr->flags & kUSBEHCIFlagsMuliTT)
        {
            // this is a multiTT hub
            hiPtr = GetHubInfo(highSpeedHub, highSpeedPort);
            if (!hiPtr)
            {
                // if there is no pointer, then i have to create one for this port
                hiPtr = NewHubInfo(highSpeedHub, highSpeedPort);
                if (!hiPtr)
                    return kIOReturnInternalError;
                USBLog (1, "%s[%p]::UIMCreateIsochEndpoint - setting new hub info(%p) bandwidth to 1024", getName(), this, hiPtr);
                hiPtr->bandwidthAvailable = 1024;
            }
        }
        bandWidthAvailPtr = &hiPtr->bandwidthAvailable;
    }
    
    pEP = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);

    if (pEP) 
    {
        // this is the case where we have already created this endpoint, and now we are adjusting the maxPacketSize
        //
        USBLog(6,"%s[%p]::UIMCreateIsochEndpoint endpoint already exists, changing maxPacketSize to %d",getName(), this, maxPacketSize);

        curMaxPacketSize = pEP->maxPacketSize;
	
        if (maxPacketSize == curMaxPacketSize) 
	{
            USBLog(4, "%s[%p]::UIMCreateIsochEndpoint maxPacketSize (%d) the same, no change",getName(), this, maxPacketSize);
            return kIOReturnSuccess;
        }
        if (maxPacketSize > curMaxPacketSize) 
	{
            // client is trying to get more bandwidth
            xtraRequest = maxPacketSize - curMaxPacketSize;
            if (xtraRequest > *bandWidthAvailPtr)
            {
                USBLog(1,"%s[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %d, available: %d",getName(), this, xtraRequest, *bandWidthAvailPtr);
                return kIOReturnNoBandwidth;
            }
            *bandWidthAvailPtr -= xtraRequest;
            USBLog(6, "%s[%p]::UIMCreateIsochEndpoint grabbing additional bandwidth: %d, new available: %d",getName(), this, xtraRequest, *bandWidthAvailPtr);
        } else 
	{
            // client is trying to return some bandwidth
            xtraRequest = curMaxPacketSize - maxPacketSize;
            *bandWidthAvailPtr += xtraRequest;
            USBLog(6, "%s[%p]::UIMCreateIsochEndpoint returning some bandwidth: %d, new available: %d",getName(), this, xtraRequest, *bandWidthAvailPtr);
        }
        pEP->maxPacketSize = maxPacketSize;
        if (highSpeedHub == 0)
        {
            // Note new high speed bandwidth
            _isochBandwidthAvail = highSpeedBandWidth;
            
            pEP->mult = maxPacketSize/2048+1;
            pEP->oneMPS = maxPacketSize&(2048-1);
            USBLog(5,"%s[%p]::UIMCreateIsochEndpoint high speed size %d, mult %d: %d, new available: %d",getName(), this, pEP->mult, pEP->oneMPS);
        }
        
        return kIOReturnSuccess;
    }
    
    // we neeed to create a new EP structure
    if (maxPacketSize > *bandWidthAvailPtr) 
    {
        USBLog(1,"%s[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %d, available: %d",getName(), this, maxPacketSize, *bandWidthAvailPtr);
        return kIOReturnNoBandwidth;
    }

    pEP = CreateIsochronousEndpoint(functionAddress, endpointNumber, direction, highSpeedHub, highSpeedPort);
    if (pEP == NULL) 
        return kIOReturnNoMemory;

    pEP->toDoList = NULL;
    pEP->toDoEnd = NULL;
    pEP->doneQueue = NULL;
    pEP->doneEnd = NULL;
    pEP->inSlot = kEHCIPeriodicListEntries+1;


    *bandWidthAvailPtr -= maxPacketSize;
    pEP->maxPacketSize = maxPacketSize;
    
    USBLog(7, "%s[%p]::UIMCreateIsochEndpoint success. bandwidth used = %d, new available: %d",getName(), this, maxPacketSize, *bandWidthAvailPtr);
    if (highSpeedHub == 0)
    {
        // Note new Full speed bandwidth
        _isochBandwidthAvail = highSpeedBandWidth;
		pEP->mult = maxPacketSize/2048+1;
		pEP->oneMPS = maxPacketSize&(2048-1);
		USBLog(6,"%s[%p]::UIMCreateIsochEndpoint high speed 2 size %d, mult %d: %d, new available: %d",getName(), this, pEP->mult, pEP->oneMPS);
    }

    return kIOReturnSuccess;
}


IOReturn 
AppleUSBEHCI::AbortIsochEP(AppleEHCIIsochEndpointPtr pEP)
{
    UInt32			slot;
    IOReturn			err;
    AppleEHCIIsochListElement	*pTD;
    
    USBLog(7, "%s[%p]::AbortIsochEP (%p)", getName(), this, pEP);
            
    // we need to make sure that the interrupt routine is not processing the periodic list
    DisablePeriodicSchedule();
    
    // now make sure we finish any periodic processing we were already doing (for MP machines)
    while (_filterInterruptActive)
	;
	
    // now scavange any transactions which were already on the done queue
    err = scavengeIsocTransactions(NULL);
    if (err)
    {
	USBLog(1, "%s[%p]::AbortIsochEP - err (%p) from scavengeIsocTransactions", getName(), this, err);
    }
    
    if ((_outSlot < kEHCIPeriodicListEntries) && (pEP->inSlot < kEHCIPeriodicListEntries))
    {

        // now scavenge the remaining transactions on the periodic list
        slot = _outSlot;
        while (slot != pEP->inSlot)
        {
            AppleEHCIListElement 		*thing;
            AppleEHCIListElement		*nextThing;
            AppleEHCIListElement		*prevThing;
            
            thing = _logicalPeriodicList[slot];
            prevThing = NULL;
            while(thing != NULL)
            {
                nextThing = thing->_logicalNext;
                pTD = OSDynamicCast(AppleEHCIIsochListElement, thing);
                if(pTD)
                {
                    if (pTD->myEndpoint == pEP)
                    {
                        // first unlink it
                        if (prevThing)
                        {
                            prevThing->_logicalNext = thing->_logicalNext;
                            prevThing->SetPhysicalLink(thing->GetPhysicalLink());
                        }
                        else
                        {
                            _logicalPeriodicList[slot] = nextThing;
                            _periodicList[slot] = thing->GetPhysicalLink();
                        }
                        err = pTD->UpdateFrameList();		// TODO - accumulate the return values or force abort err
                        PutTDonDoneQueue(pEP, pTD);
                        break;					// only one TD per endpoint
                    }
                }
                else 
                {
                    // only care about Isoch in this list
                    break;
                }
                prevThing = thing;
                thing = nextThing;
            }
            slot = (slot + 1) & (kEHCIPeriodicListEntries-1);
        }
    }
    
    // now transfer any transactions from the todo list to the done queue
    pTD = GetTDfromToDoList(pEP);
    while (pTD)
    {
	err = pTD->UpdateFrameList();
	PutTDonDoneQueue(pEP, pTD);
	pTD = GetTDfromToDoList(pEP);
    }
    // we can go back to processing now
    EnablePeriodicSchedule();
    
    pEP->accumulatedStatus = kIOReturnAborted;
    ReturnIsocDoneQueue(pEP);
    
    return kIOReturnSuccess;
}


void 
AppleUSBEHCI::returnTransactions(AppleEHCIQueueHead *pED, EHCIGeneralTransferDescriptor *untilThisOne)
{
    EHCIGeneralTransferDescriptorPtr	doneQueue = NULL, doneTail= NULL;

    USBLog(5, "%s[%p]::returnTransactions, pED(%p) until (%p)", getName(), this, pED, untilThisOne);
    pED->print(5);

    if (!(USBToHostLong(pED->GetSharedLogical()->qTDFlags) & kEHCITDStatus_Halted))
    {
	USBError(1, "%s[%p]::returnTransactions, pED (%p) NOT HALTED (qTDFlags = %p)", getName(), this, pED, USBToHostLong(pED->GetSharedLogical()->qTDFlags));
	// HaltAsyncEndpoint(pED);
    }
    
    if ((pED->qTD != pED->TailTD) && (pED->qTD != untilThisOne))		// There are transactions on this queue
    {
        USBLog(5, "%s[%p] returnTransactions: removing TDs", getName(), this);
        if(untilThisOne == NULL)
        {
            untilThisOne = pED->TailTD;	// Return all the transactions on this queue
        }
	doneQueue = pED->qTD;
	doneTail = doneQueue;
	pED->qTD = pED->qTD->pLogicalNext;
	while (pED->qTD != untilThisOne)
	{
	    doneTail->pLogicalNext = pED->qTD;
	    doneTail = pED->qTD;
	    pED->qTD = pED->qTD->pLogicalNext;
	}
	doneTail->pLogicalNext = NULL;
        pED->GetSharedLogical()->NextqTDPtr = HostToUSBLong(untilThisOne->pPhysical);
        pED->qTD = untilThisOne;
    }

    // Kill the overlay transaction and leave the EP enabled (NOT halted)
    
    USBLog(5, "%s[%p]::returnTransactions, pED->qTD (L:%p P:%p) pED->TailTD (L:%p P:%p)", getName(), this, pED->qTD, pED->qTD->pPhysical, pED->TailTD, pED->TailTD->pPhysical);
    USBLog(5, "%s[%p]::returnTransactions: clearing ED bit, qTDFlags = %x", getName(), this, USBToHostLong(pED->GetSharedLogical()->qTDFlags));
    pED->GetSharedLogical()->qTDFlags = 0;						// Ensure that next TD is fetched (not the ALT)
    if (doneQueue)
    {
	USBLog(5, "%s[%p]::returnTransactions: calling back the done queue (after ED is made active)", getName(), this);    
	EHCIUIMDoDoneQueueProcessing(doneQueue, kIOUSBTransactionReturned, NULL, NULL);
    }
    USBLog(5, "%s[%p]::returnTransactions: after bit clear, qTDFlags = %x", getName(), this, USBToHostLong(pED->GetSharedLogical()->qTDFlags));    
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
	USBLog(6, "%s[%p]::HaltAsyncEndpoint - unlinking, halting, and relinking (%p)", getName(), this, pED);
	unlinkAsyncEndpoint(pED, pEDBack);
	pED->GetSharedLogical()->qTDFlags |= HostToUSBLong(kEHCITDStatus_Halted);
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
	USBLog(6, "%s[%p]::HaltInterruptEndpoint - unlinking, halting, and relinking (%p)", getName(), this, pED);
	unlinkIntEndpoint(pED);
	pED->GetSharedLogical()->qTDFlags |= HostToUSBLong(kEHCITDStatus_Halted);
	linkInterruptEndpoint(pED);
    }
}


IOReturn 
AppleUSBEHCI::HandleEndpointAbort(
    short			functionAddress,
    short			endpointNumber,
    short			direction)
{
    AppleEHCIQueueHead 		*pED;
    AppleEHCIQueueHead 		*pEDQueueBack;
    AppleEHCIIsochEndpointPtr	piEP;
    
    USBLog(5, "%s[%p] AppleUSBEHCI::HandleEndpointAbort: Addr: %d, Endpoint: %d,%d",getName(), this, functionAddress, endpointNumber, direction);

    if (functionAddress == _rootHubFuncAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "%s[%p::HandleEndpointAbort: bad params - endpNumber: %d", getName(), this, endpointNumber );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDDelete (endpointNumber, direction) in 9
        //
        USBLog(5, "%s[%p]::HandleEndpointAbort: Attempting operation on root hub", getName(), this);
        return SimulateEDAbort( endpointNumber, direction);
    }

    piEP = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);
    if (piEP)
    {
	return AbortIsochEP(piEP);
    }
    
    pED = FindControlBulkEndpoint (functionAddress, endpointNumber, &pEDQueueBack, direction);
    if(pED != NULL)
    {
	HaltAsyncEndpoint(pED, pEDQueueBack);
	returnTransactions(pED, NULL);				// this will unhalt the EP
    }
    else
    {
	pED = FindInterruptEndpoint(functionAddress, endpointNumber, direction, &pEDQueueBack);

	if(pED == NULL)
	{
	    USBLog(1, "AppleUSBEHCI::HandleEndpointAbort, endpoint not found");
	    return kIOUSBEndpointNotFound;
	}
#warning Aborting Int endpoints needs to be done properly.
	HaltInterruptEndpoint(pED);
	returnTransactions(pED, NULL);
    }

    return kIOReturnSuccess;

}



IOReturn 
AppleUSBEHCI::UIMAbortEndpoint(
    short			functionAddress,
    short			endpointNumber,
    short			direction)
{
    // this is probably not correct, but still waiting clarification on the EHCI spec section 4.8.2
    return HandleEndpointAbort(functionAddress, endpointNumber, direction);
}


IOReturn 
AppleUSBEHCI::UIMClearEndpointStall(
            short				functionAddress,
            short				endpointNumber,
            short				direction)
{
    
    USBLog(7, "%s[%p]::UIMClearEndpointStall - endpoint %d:%d", getName(), this, functionAddress, endpointNumber);
    return  HandleEndpointAbort(functionAddress, endpointNumber, direction);
}



AppleEHCIQueueHead *
AppleUSBEHCI::FindInterruptEndpoint(short functionNumber, short endpointNumber, short direction, AppleEHCIQueueHead * *pEDBack)
{
    UInt32			unique;
    AppleEHCIQueueHead *	pEDQueue;
    AppleEHCIQueueHead *	pEDQueueBack;
    int 			i;

    unique = (UInt32) ((((UInt32) endpointNumber) << kEHCIEDFlags_ENPhase) | ((UInt32) functionNumber));
    pEDQueueBack = NULL;

    for(i= 0; i < _greatestPeriod; i++)
    {
	pEDQueue = OSDynamicCast(AppleEHCIQueueHead, _logicalPeriodicList[i]);
	while ( pEDQueue != NULL)
	{
	    if( ( (USBToHostLong(pEDQueue->GetSharedLogical()->flags) & kUniqueNumNoDirMask) == unique) && ( pEDQueue->direction == (UInt8)direction) ) 
	    {
		if (pEDBack)
		    *pEDBack = pEDQueueBack;
		return  pEDQueue;
	    } else 
	    {
		pEDQueueBack = pEDQueue;
		pEDQueue = OSDynamicCast(AppleEHCIQueueHead, pEDQueue->_logicalNext);
	    }
	} 
    }
    return  NULL;
}



IOReturn
AppleUSBEHCI::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
    IOReturn				status = kIOReturnSuccess;
    AppleEHCIQueueHead *		pEDQueue;
    IOUSBCompletion			completion = command->GetUSLCompletion();
    IOMemoryDescriptor*			buffer = command->GetBuffer();
    short 				direction = command->GetDirection(); // our local copy may change

    USBLog(7, "%s[%p]::UIMCreateInterruptTransfer - adr=%d:%d cbp=%p:%lx br=%s cback=[%lx:%lx:%lx])", getName(), this,  
	    command->GetAddress(), command->GetEndpoint(), command->GetBuffer(), 
	    command->GetBuffer()->getLength(), command->GetBufferRounding()?"YES":"NO", 
	    (UInt32)completion.action, (UInt32)completion.target, 
	    (UInt32)completion.parameter);
    if (_rootHubFuncAddress == command->GetAddress())
    {
        SimulateRootHubInt(command->GetEndpoint(), buffer, buffer->getLength(), completion);
        return kIOReturnSuccess;
    }    

    pEDQueue = FindInterruptEndpoint(command->GetAddress(), command->GetEndpoint(), direction, NULL);
    if (!pEDQueue)
    {
	USBLog(1, "%s[%p]::UIMCreateInterruptTransfer - Endpoint not found", getName(), this);
	return kIOUSBEndpointNotFound;
    }

    status = allocateTDs(pEDQueue, command, buffer, command->GetBuffer()->getLength(), direction, false );
    if(status != kIOReturnSuccess)
    {
        USBLog(1, "%s[%p]::UIMCreateInterruptTransfer allocateTDs failed %x", getName(), this, status);
    }
    else
	EnablePeriodicSchedule();
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
    USBLog(1, "%s(%p)UIMCreateInterruptTransfer- calling the wrong method!", getName(), this);
    return kIOReturnIPCError;
}




void 
AppleUSBEHCI::unlinkIntEndpoint(AppleEHCIQueueHead * pED)
{
    int 			i;
    AppleEHCIQueueHead * 	pEDQueue;
    int				maxPacketSize;
    Boolean			foundED = false;
    
    USBLog(7, "+%s[%p]::unlinkIntEndpoint(%p)", getName(), this, pED);
    
    maxPacketSize   =  (USBToHostLong(pED->GetSharedLogical()->flags)  & kEHCIEDFlags_MPS) >> kEHCIEDFlags_MPSPhase;
    
    for(i= pED->offset; i < kEHCIPeriodicListEntries; i += pED->pollM1+1)
    {
	pEDQueue = OSDynamicCast(AppleEHCIQueueHead, _logicalPeriodicList[i]);
	if(pED == pEDQueue)
	{
	    _logicalPeriodicList[i] = pED->_logicalNext;
	    _periodicList[i] = pED->GetSharedLogical()->nextQH;
	    foundED = true;
	    USBLog(6, "%s[%p]::unlinkIntEndpoint- found ED at top of list %d, new logical=%p, new physical=%p", getName(), this, i, _logicalPeriodicList[i], USBToHostLong(_periodicList[i]));
	}
	else
	{
	    while(pEDQueue != NULL)
	    {
		if(pEDQueue->_logicalNext == pED)
		{
		    pEDQueue->_logicalNext = pED->_logicalNext;
		    pEDQueue->GetSharedLogical()->nextQH = pED->GetSharedLogical()->nextQH;
		    foundED = true;
		    USBLog(6, "%s[%p]::unlinkIntEndpoint- found ED in list %d, new logical=%p, new physical=%p", getName(), this, i, pEDQueue->_logicalNext, USBToHostLong(pEDQueue->GetSharedLogical()->nextQH));
		    break;
		}
		pEDQueue = OSDynamicCast(AppleEHCIQueueHead, pEDQueue->_logicalNext);
	    }
	    if(pEDQueue == NULL)
	    {
		USBLog(7, "%s[%p]::unlinkIntEndpoint endpoint not found in list %d", getName(), this, i);
	    }
	
	}
	if (i < kEHCIMaxPoll)
	    _periodicBandwidth[i] += maxPacketSize;
    }

    IOSleep(1);				// make sure to clear the period list traversal cache
    pED->_logicalNext = NULL;
    
    if (foundED)
	_periodicEDsInSchedule--;

    USBLog(7, "-%s[%p]::unlinkIntEndpoint(%p)", getName(), this, pED);
}



void 
AppleUSBEHCI::unlinkAsyncEndpoint(AppleEHCIQueueHead * pED, AppleEHCIQueueHead * pEDQueueBack)
{
    UInt32 CMD, STS, count;

    if( (pEDQueueBack == NULL) && (pED->_logicalNext == NULL) )
    {
        USBLog(7, "%s[%p]::unlinkAsyncEndpoint: removing sole endpoint %lx", getName(),this, (long)pED);
	// this is the only endpoint in the queue. we will leave list processing disabled
	DisableAsyncSchedule();
	printAsyncQueue();
	_AsyncHead = NULL;
	_pEHCIRegisters->AsyncListAddr = 0;
	printAsyncQueue();
    }
    else
    {
	USBLog(7, "%s[%p]::unlinkAsyncEndpoint: removing endpoint from queue %lx", getName(),this, (long)pED);

	// have to take this out of the queue
	
	if(_AsyncHead == pED)
	{
	    // this is the case where we are taking the head of the queue, but it is not the
	    // only element left in the queue
	    if (pEDQueueBack)
	    {
		USBError(1, "%s[%p]::unlinkAsyncEndpoint: ERROR - pEDQueueBack should be NULL at this point", getName(),this );
	    }
	    // we need to find the last ED in the logical list, so that we can link in the "wrap around" physical pointer
	    pEDQueueBack = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext);
	    while (pEDQueueBack->_logicalNext)
		pEDQueueBack = OSDynamicCast(AppleEHCIQueueHead, pEDQueueBack->_logicalNext);
//	    DisableAsyncSchedule();
	    printAsyncQueue();
	    _AsyncHead = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext);
	    _pEHCIRegisters->AsyncListAddr = HostToUSBLong(pED->_logicalNext->_sharedPhysical);
	    pEDQueueBack->GetSharedLogical()->nextQH = HostToUSBLong(pED->_logicalNext->GetPhysicalAddrWithType());
	    pED = OSDynamicCast(AppleEHCIQueueHead , pED->_logicalNext); 
	    pED->GetSharedLogical()->flags |= HostToUSBLong(kEHCIEDFlags_H);
	    printAsyncQueue();
//	    EnableAsyncSchedule();
	}
	else if(pEDQueueBack != NULL)
	{
//	    DisableAsyncSchedule();
	    printAsyncQueue();
	    pEDQueueBack->GetSharedLogical()->nextQH = pED->GetSharedLogical()->nextQH;
	    pEDQueueBack->_logicalNext = pED->_logicalNext;
	    printAsyncQueue();
//	    EnableAsyncSchedule();
	}
	else
	{
	    USBLog(7, "%s[%p]::unlinkAsyncEndpoint: ED not head, but pEDQueueBack not NULL", getName(),this);
	}

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
	USBLog(5, "%s[%p]::unlinkAsyncEndpoint: delayed for %d", getName(),this, count);
	
	// Clear request
	_pEHCIRegisters->USBSTS = HostToUSBLong(kEHCIAAEIntBit);
    }
}	



IOReturn
AppleUSBEHCI::DeleteIsochEP(AppleEHCIIsochEndpointPtr pEP)
{
    AppleEHCIIsochEndpointPtr 		curEP, prevEP;
    
    USBLog(7, "%s[%p]::DeleteIsochEP (%p)", getName(), this, pEP);
    if (pEP->activeTDs)
    {
	USBLog(6, "%s[%p]::DeleteIsochEP- there are still %d active TDs - aborting", getName(), this, pEP->activeTDs);
	AbortIsochEP(pEP);
	if (pEP->activeTDs)
	{
	    USBLog(3, "%s[%p]::DeleteIsochEP- after abort there are STILL %d active TDs", getName(), this, pEP->activeTDs);
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
    
    // need to return the bandwidth used
    if (pEP->highSpeedHub == 0)
    {
	_isochBandwidthAvail += pEP->maxPacketSize;
    }
    else
    {
        AppleUSBEHCIHubInfoPtr		hiPtr;
        
        hiPtr = GetHubInfo(pEP->highSpeedHub, pEP->highSpeedPort);
        if (!hiPtr)
        {
            // must be a single TT hub
            hiPtr = GetHubInfo(pEP->highSpeedHub, 0);
        }
        if (hiPtr)
        {
            USBLog(5, "%s[%p]::DeleteIsochEP - returning bandwidth (%d) - new bandwidth (%d)", getName(), this, pEP->maxPacketSize, hiPtr->bandwidthAvailable + pEP->maxPacketSize);
            hiPtr->bandwidthAvailable += pEP->maxPacketSize;
        }
    }
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::UIMDeleteEndpoint(
    short				functionAddress,
    short				endpointNumber,
    short				direction)
{
    AppleEHCIQueueHead  		*pED;
    AppleEHCIQueueHead  		*pEDQueueBack;
    AppleEHCIIsochEndpointPtr		piEP;

    USBLog(7, "%s[%p] AppleUSBEHCI::UIMDeleteEndpoint: Addr: %d, Endpoint: %d,%d",getName(), this, functionAddress,endpointNumber,direction);
    
    if (functionAddress == _rootHubFuncAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "%s[%p] UIMDeleteEndpoint: bad params - endpNumber: %d", getName(), this, endpointNumber );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDDelete (endpointNumber, direction) in 9
        //
        USBLog(5, "%s[%p] UIMDeleteEndpoint: Attempting operation on root hub", getName(), this);
        return SimulateEDDelete( endpointNumber, direction);
    }

    piEP = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);
    if (piEP)
	return DeleteIsochEP(piEP);
    
    pED = FindControlBulkEndpoint (functionAddress, endpointNumber, &pEDQueueBack, direction);
    
    if(pED == NULL)
    {
	UInt32				splitFlags;
	USBDeviceAddress		highSpeedHub;
	UInt8				highSpeedPort;
        AppleUSBEHCIHubInfoPtr		hiPtr;
	
	pED = FindInterruptEndpoint(functionAddress, endpointNumber, direction, &pEDQueueBack);
	if(pED == NULL)
	{
	    USBLog(2, "AppleUSBEHCI::UIMDeleteEndpoint, endpoint not found");
	    return kIOUSBEndpointNotFound;
	}
	USBLog(5, "%s[%p]::UIMDeleteEndpoint: deleting Int endpoint", getName(), this);
	unlinkIntEndpoint(pED);
	if (!_periodicEDsInSchedule)
	{
	    USBLog(3, "%s[%p]::UIMDeleteEndpoint: $$$$$$ all EDs gone from periodic schedule, disabling", getName(), this, _periodicEDsInSchedule);
	    DisablePeriodicSchedule();
	}
	splitFlags = USBToHostLong(pED->GetSharedLogical()->splitFlags);
	highSpeedHub = (splitFlags & kEHCIEDSplitFlags_HubAddr) >> kEHCIEDSplitFlags_HubAddrPhase;
	highSpeedPort = (splitFlags & kEHCIEDSplitFlags_Port) >> kEHCIEDSplitFlags_PortPhase;
        hiPtr = GetHubInfo(highSpeedHub, highSpeedPort);
        if (!hiPtr)
        {
            // must be a single TT hub
            hiPtr = GetHubInfo(highSpeedHub, 0);
        }
        if (hiPtr)
        {
            USBLog(5, "%s[%p]::UIMDeleteEndpoint - returning bandwidth (%d) - new bandwidth (%d)", getName(), this, pED->maxPacketSize, hiPtr->bandwidthAvailable + pED->maxPacketSize);
            hiPtr->bandwidthAvailable += pED->maxPacketSize;
        }
    }
    else
    {
	USBLog(5, "%s[%p] UIMDeleteEndpoint: deleting async endpoint", getName(), this);
	unlinkAsyncEndpoint(pED, pEDQueueBack);
    }
    
    if(pED->qTD != pED->TailTD)		// There are transactions on this queue
    {
        USBLog(5, "%s[%p] UIMDeleteEndpoint: removing TDs", getName(), this);
        EHCIUIMDoDoneQueueProcessing(pED->qTD, kIOUSBTransactionReturned, NULL, pED->TailTD);
        pED->qTD = pED->TailTD;
        pED->GetSharedLogical()->NextqTDPtr = HostToUSBLong(pED->qTD->pPhysical);
    }
    
    USBLog(5, "%s[%p] AppleUSBEHCI::UIMDeleteEndpoint: Deallocating %p",getName(), this, pED);
    DeallocateED(pED);
    
    return kIOReturnSuccess;
}


IOReturn
AppleUSBEHCI::CreateHSIsochTransfer(	AppleEHCIIsochEndpointPtr	pEP,
					IOUSBIsocCompletion		completion,
					UInt64 				frameNumberStart,
					IOMemoryDescriptor 		*pBuffer,
					UInt32 				frameCount,
					IOUSBIsocFrame	*pFrames)
{
	return CreateHSIsochTransfer(pEP, completion, frameNumberStart, pBuffer, frameCount, (IOUSBLowLatencyIsocFrame *)pFrames, 0, false);
}
IOReturn
AppleUSBEHCI::CreateHSIsochTransfer(	AppleEHCIIsochEndpointPtr	pEP,
					IOUSBIsocCompletion		completion,
					UInt64 				frameNumberStart,
					IOMemoryDescriptor 		*pBuffer,
					UInt32 				frameCount,
					IOUSBLowLatencyIsocFrame	*pFrames,
					UInt32 				updateFrequency)
{
	return CreateHSIsochTransfer(pEP, completion, frameNumberStart, pBuffer, frameCount, pFrames, updateFrequency, true);
}
IOReturn
AppleUSBEHCI::CreateHSIsochTransfer(	AppleEHCIIsochEndpointPtr	pEP,
					IOUSBIsocCompletion		completion,
					UInt64 				frameNumberStart,
					IOMemoryDescriptor 		*pBuffer,
					UInt32 				frameCount,
					IOUSBLowLatencyIsocFrame	*pFrames,
					UInt32 				updateFrequency,
					Boolean 			lowLatency)
{
    UInt64 				curFrameNumber = GetFrameNumber();
    UInt64 				maxOffset;
    UInt32 				bufferSize;
    AppleEHCIIsochTransferDescriptor 	*pNewITD = NULL;
    IOByteCount 			transferOffset;
    unsigned 				i,j;
    UInt32 				*buffP, *TransactionP = 0;
    UInt32 				pageOffset;
    UInt32 				page;
    UInt32 				frames;
    UInt32 				trLen;
    UInt64 				frameDiff;
    UInt32 				diff32;
    IOUSBIsocFrame			*pHLFrames;
    IOPhysicalAddress			dmaStartAddr;
    IOByteCount segLen;
    
    if( (frameCount % 8) != 0 )
	{
        USBLog(3,"%s[%p]::UIMCreateIsochTransfer frameCount not whole frame: %d",getName(), this, frameCount);
        return kIOReturnBadArgument;
	}

    maxOffset = _frameListSize;
    if (frameNumberStart <= curFrameNumber)
    {
        if (frameNumberStart < (curFrameNumber - maxOffset))
        {
            USBLog(3,"%s[%p]::UIMCreateIsochTransfer request frame WAY too old.  frameNumberStart: %ld, curFrameNumber: %ld.  Returning 0x%x",getName(), this, (UInt32) frameNumberStart, (UInt32) curFrameNumber, kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
        USBLog(5,"%s[%p]::UIMCreateIsochTransfer WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %ld, curFrameNumber: %ld.  USBIsocFrame Ptr: %p, First ITD: %p",getName(), this, (UInt32) frameNumberStart, (UInt32) curFrameNumber, pFrames, pEP->toDoEnd);
    } else 
    {					// frameNumberStart > curFrameNumber
        if (frameNumberStart > (curFrameNumber + maxOffset))
        {
            USBLog(3,"%s[%p]::UIMCreateIsochTransfer request frame too far ahead!  frameNumberStart: %ld, curFrameNumber: %ld",getName(), this, (UInt32) frameNumberStart, (UInt32) curFrameNumber);
            return kIOReturnIsoTooNew;
        }
        frameDiff = frameNumberStart - curFrameNumber;
        diff32 = (UInt32)frameDiff;
        if (diff32 < 2)
        {
            USBLog(5,"%s[%p]::UIMCreateIsochTransfer WARNING! - frameNumberStart less than 2 ms (is %ld)!  frameNumberStart: %ld, curFrameNumber: %ld",getName(), this, (UInt32) diff32, (UInt32) frameNumberStart, (UInt32) curFrameNumber);
        }
    }

    /* Format all the TDs, attach them to the pseudo endpoint. */
    /* let the frame interrupt routine put them in the periodic list */

	frames = 100;
    transferOffset = 0;
	pHLFrames = (IOUSBIsocFrame *)pFrames;
	
    /* Do this one frame, 8 micro frames, at a time */
    while(frameCount > 0)
    {
    
        //
        //  Get the size of buffer for this frame
        //
        bufferSize = 0;
        for ( i = 0; i< 8; i++)
        {
            if(frameCount < i)
            {
                break;
            }
    
			if(lowLatency)
			{
				if (pFrames[i].frReqCount > kUSBMaxHSIsocFrameReqCount)
				{
					USBLog(3,"%s[%p]::UIMCreateIsochTransfer Isoch frame too big %d",getName(), this, pFrames[i].frReqCount);
					return kIOReturnBadArgument;
				}
				bufferSize += pFrames[i].frReqCount; 
			}
			else
			{
				if (pHLFrames[i].frReqCount > kUSBMaxHSIsocFrameReqCount)
				{
					USBLog(3,"%s[%p]::UIMCreateIsochTransfer Isoch frame too big %d",getName(), this, pHLFrames[i].frReqCount);
					return kIOReturnBadArgument;
				}
				bufferSize += pHLFrames[i].frReqCount; 
			
			}
        }
    
        //
        // go ahead and make sure we can grab at least ONE TD, before we lock the buffer	
        //
        pNewITD = AllocateITD();
        USBLog(7, "%s[%p]::UIMCreateIsochTransfer - new iTD %p", getName(), this, pNewITD);
        if (pNewITD == NULL)
        {
            USBLog(1,"%s[%p]::UIMCreateIsochTransfer Could not allocate a new iTD",getName(), this);
            return kIOReturnNoMemory;
        }
		
		pNewITD->lowLatency = lowLatency;
    
    
    /* set up all the physical page pointers */

    
	buffP = &pNewITD->GetSharedLogical()->bufferPage0;
	dmaStartAddr = pBuffer->getPhysicalSegment(transferOffset, &segLen);
	pageOffset = dmaStartAddr & kEHCIPageOffsetMask;
	if(segLen > bufferSize)
	{
	    segLen = bufferSize;
	}
	if(segLen > (kEHCIPageSize-pageOffset))
	{
	    segLen = kEHCIPageSize-pageOffset;
	}
	
	*(buffP++) = HostToUSBLong( dmaStartAddr & kEHCIPageMask);
	USBLog(7, "%s[%p]::CreateHSIocTransfer - getPhysicalSegment returned start of %p; length:%ld ; Buff Ptr0:%lx", getName(), this, dmaStartAddr, segLen, *(buffP-1));
	transferOffset += segLen;
	bufferSize -= segLen;
	
	for(j=1; j<=6; j++)
	{
	    if(bufferSize==0)
	    {
		*(buffP++) = 0;
		continue;
	    }
	    dmaStartAddr = pBuffer->getPhysicalSegment(transferOffset, &segLen);
	    *(buffP++) = HostToUSBLong( dmaStartAddr & kEHCIPageMask);
	    pageOffset = dmaStartAddr & kEHCIPageOffsetMask;
	    if(pageOffset != 0)
	    {
		USBError(1, "%s[%p]::CreateHSIocTransfer - pageOffset not zero in %dth buff: %p", getName(), this, j, dmaStartAddr);
	    }
	    if(segLen > bufferSize)
	    {
		segLen = bufferSize;
	    }
	    if(segLen > kEHCIPageSize)
	    {
		segLen = kEHCIPageSize;
	    }
	    transferOffset += segLen;
	    bufferSize -= segLen;
	    USBLog(7, "%s[%p]::CreateHSIocTransfer - getPhysicalSegment returned start of %p;  length:%ld ; Buff Ptr%d:%lx", getName(), this, dmaStartAddr, segLen, j, *(buffP-1));
	}

    //    return kIOReturnNoMemory;
        page = 0;
        TransactionP = &pNewITD->GetSharedLogical()->Transaction0;
	if(lowLatency)
	{
	    pNewITD->myFrames = (IOUSBIsocFrame *)pFrames;	// Points to the 8 frames structure for the 8 frames in this micro frame
	}
	else
	{
	    pNewITD->myFrames = pHLFrames;	// Points to the 8 frames structure for the 8 frames in this micro frame
	}
        for ( i = 0; i< 8; i++)
        {
            if(frameCount < i)
            {
                break;
            }
			
			if(lowLatency)
			{
				trLen = (pFrames++)->frReqCount;
			}
			else
			{
				trLen = (pHLFrames++)->frReqCount;
			}
            /* Len already checked above */
            
            *(TransactionP++) = HostToUSBLong(kEHCI_ITDStatus_Active |  (trLen<< kEHCI_ITDTr_LenPhase) | 
                                        (pageOffset << kEHCI_ITDTr_OffsetPhase) | (page << kEHCI_ITDTr_PagePhase) );

			// This gets filled in later for the last one.
            pNewITD->completion.action = NULL;

            pageOffset += trLen;
            if(pageOffset > kEHCIPageSize)
            {
                pageOffset -= kEHCIPageSize;
                page++;
            }
        };
        if(frameCount > 8)
        {
            frameCount -= 8;
        }
        else
        {
            frameCount = 0;
        }

	/*We're no longer using IOC as end of transaction marker. There's a iTD->completion field for that */
	if(frames-- == 0)
	{	/* We've gone 100 frames, about time to set an IOC */
		TransactionP[-1] |= HostToUSBLong(kEHCI_ITDTr_IOC);
		frames = 100;
	}

	pNewITD->frameNumber = frameNumberStart++;
    
        pNewITD->GetSharedLogical()->bufferPage0 |= HostToUSBLong((pEP->functionAddress << kEHCI_ITDBuf_FnAddrPhase) | (pEP->endpointNumber << kEHCI_ITDBuf_EPPhase) );
        pNewITD->GetSharedLogical()->bufferPage1 |= HostToUSBLong( (pEP->oneMPS << kEHCI_ITDBuf_MPSPhase) | ((pEP->direction == kUSBIn) ? (UInt32) kEHCI_ITDBuf_IO : 0) );
        pNewITD->GetSharedLogical()->bufferPage2 |= HostToUSBLong( (pEP->mult << kEHCI_ITDBuf_MultPhase) );
        
	pNewITD->myEndpoint = pEP;
	
	PutTDonToDoList(pEP, pNewITD);
    }
	// Rember to put IOC on the last one at least.
	TransactionP[-1] |= HostToUSBLong(kEHCI_ITDTr_IOC);
	// An the completion action.
USBLog(3,"%s[%p]::UIMCreateIsochTransfer completion completion.action %p",getName(), this, completion.action);
	pNewITD->completion = completion;
       
    AddIsocFramesToSchedule(pEP);
    EnablePeriodicSchedule();
	
    return kIOReturnSuccess;
}


IOReturn
AppleUSBEHCI::CreateSplitIsochTransfer(	AppleEHCIIsochEndpointPtr	pEP,
					IOUSBIsocCompletion		completion,
					UInt64 				frameNumberStart,
					IOMemoryDescriptor 		*pBuffer,
					UInt32 				frameCount,
					IOUSBIsocFrame			*pFrames,
					UInt32 				updateFrequency,
					bool 				lowLatency)
{
    AppleEHCISplitIsochTransferDescriptor	*pNewSITD;
    UInt64 					maxOffset;
    UInt64 					curFrameNumber = GetFrameNumber();
    UInt64 					frameDiff;
    UInt32 					diff32;
    IOByteCount 				transferOffset;
    UInt32 					frames;
    IOUSBLowLatencyIsocFrame			*pLLFrames;
    UInt32 					bufferSize;
    UInt8					startSplitFlags;
    UInt8					completeSplitFlags;
    UInt8					transactionPosition;
    UInt8					transactionCount;
    UInt8					ioc = 0;
    UInt32					i;
    UInt32 				pageOffset;
    IOPhysicalAddress			dmaStartAddr;
    IOByteCount segLen;

    USBLog(6, "%s[%p]::CreateSplitIsochTransfer - frameCount %d - frameNumberStart 0x%Lx - curFrameNumber 0x%Lx", getName(), this, frameCount, frameNumberStart, curFrameNumber);
    USBLog(6, "%s[%p]::CreateSplitIsochTransfer - updateFrequency %d - lowLatency %d", getName(), this, updateFrequency, lowLatency);
    maxOffset = _frameListSize;
    if (frameNumberStart < pEP->firstAvailableFrame)
    {
	USBLog(3,"%s[%p]::CreateSplitIsochTransfer: no overlapping frames -  frameNumberStart: %Ld, pEP->firstAvailableFrame: %Ld.  Returning 0x%x",getName(), this, frameNumberStart, pEP->firstAvailableFrame, kIOReturnIsoTooOld);
	return kIOReturnIsoTooOld;
    }
    pEP->firstAvailableFrame = frameNumberStart;
    if (frameNumberStart <= curFrameNumber)
    {
        if (frameNumberStart < (curFrameNumber - maxOffset))
        {
            USBLog(3,"%s[%p]::CreateSplitIsochTransfer request frame WAY too old.  frameNumberStart: %Ld, curFrameNumber: %Ld.  Returning 0x%x",getName(), this, frameNumberStart, curFrameNumber, kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
        USBLog(5,"%s[%p]::UIMCreateIsochTransfer WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %Ld, curFrameNumber: %Ld.  USBIsocFrame Ptr: %p, First ITD: %p",getName(), this, frameNumberStart, curFrameNumber, pFrames, pEP->toDoEnd);
    } else 
    {					// frameNumberStart > curFrameNumber
        if (frameNumberStart > (curFrameNumber + maxOffset))
        {
            USBLog(3,"%s[%p]::CreateSplitIsochTransfer request frame too far ahead!  frameNumberStart: %Ld, curFrameNumber: %Ld",getName(), this, frameNumberStart, curFrameNumber);
            return kIOReturnIsoTooNew;
        }
        frameDiff = frameNumberStart - curFrameNumber;
        diff32 = (UInt32)frameDiff;
        if (diff32 < 2)
        {
            USBLog(5,"%s[%p]::UIMCreateIsochTransfer WARNING! - frameNumberStart less than 2 ms (is %d)!  frameNumberStart: %Ld, curFrameNumber: %Ld",getName(), this, diff32, frameNumberStart, curFrameNumber);
        }
    }

    pLLFrames = (IOUSBLowLatencyIsocFrame *)pFrames;
    //
    //  Get the total size of buffer
    //
    bufferSize = 0;
    for ( i = 0; i < frameCount; i++)
    {
        if (!lowLatency)
        {
            if (pFrames[i].frReqCount > kUSBMaxIsocFrameReqCount)
            {
                USBLog(3,"%s[%p]::CreateSplitIsochTransfer - Isoch frame (%d) too big %d",getName(), this, i + 1, pFrames[i].frReqCount);
                return kIOReturnBadArgument;
            }
            bufferSize += pFrames[i].frReqCount;
        } else
        {
            if (pLLFrames[i].frReqCount > kUSBMaxIsocFrameReqCount)
            {
                USBLog(3,"%s[%p]::CreateSplitIsochTransfer(LL) - Isoch frame (%d) too big %d",getName(), this, i + 1, pLLFrames[i].frReqCount);
                return kIOReturnBadArgument;
            }
            bufferSize += pLLFrames[i].frReqCount;
            // Make sure our frStatus field has a known value (debugging aid)
            //
            pLLFrames[i].frStatus = (IOReturn) kUSBLowLatencyIsochTransferKey;
        }
    }

    // Format all the TDs, attach them to the pseudo endpoint.
    // let the frame interrupt routine put them in the periodic list

    frames = 100;
    transferOffset = 0;
	
    // Do this one frame at a time
    for ( i = 0; i < frameCount; i++)
    {
	UInt16		reqCount, reqLeft;
	
        pNewSITD = AllocateSITD();
	if (lowLatency)
	    reqCount = pLLFrames[i].frReqCount;
	else
	    reqCount = pFrames[i].frReqCount;
	    
        USBLog(7, "%s[%p]::CreateSplitIsochTransfer - new iTD %p size (%d)", getName(), this, pNewSITD, reqCount);
        if (!pNewSITD)
        {
            USBLog(1,"%s[%p]::CreateSplitIsochTransfer Could not allocate a new iTD",getName(), this);
            return kIOReturnNoMemory;
        }
		
	pEP->firstAvailableFrame++;
        pNewSITD->lowLatency = lowLatency;
    
	// set up the physical page pointers

	dmaStartAddr = pBuffer->getPhysicalSegment(transferOffset, &segLen);
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
	USBLog(7, "%s[%p]::CreateSplitIocTransfer - getPhysicalSegment returned start of %p; length %ld", getName(), this, dmaStartAddr, segLen);

	transferOffset += segLen;
	bufferSize -= segLen;
        reqLeft = reqCount - segLen;

	if(reqLeft==0)
	{
	    pNewSITD->GetSharedLogical()->buffPtr1 = 0;
	}
	else
	{
	    dmaStartAddr = pBuffer->getPhysicalSegment(transferOffset, &segLen);
	    pNewSITD->GetSharedLogical()->buffPtr1 = HostToUSBLong(dmaStartAddr & kEHCIPageMask);
	    if(segLen > reqLeft)
	    {
		segLen = reqLeft;
	    }
	    if(segLen > kEHCIPageSize)
	    {
		segLen = kEHCIPageSize;
	    }
	    USBLog(7, "%s[%p]::CreateSplitIocTransfer - getPhysicalSegment returned start of %p; length %ld", getName(), this, dmaStartAddr, segLen);
	    transferOffset += segLen;
	    bufferSize -= segLen;
	}

        pNewSITD->myFrames = pFrames;
        pNewSITD->frameNumber = frameNumberStart + i;
	pNewSITD->frameIndex = i;
        
        // calculate start split and complete split flags
	if (pEP->direction == kUSBOut)
	{
	    completeSplitFlags = 0;				// dont use complete split for OUT transactions
	    if (reqCount > 188)
	    {
		transactionCount = (reqCount + 187) / 188;	// number of 188 byte transfers
		transactionPosition = 1;			// beginning of a multi-part transfer
		startSplitFlags = (2 << transactionCount) - 1;	// bitmask of bits to send SSplit on
	    }
	    else
	    {
                startSplitFlags = 2;				// Start them on second microframe (#3273910)
		transactionPosition = 0;			// total transfer is in this microframe
		transactionCount = 1;				// only need one transfer
	    }
	}
	else
	{
	    // IN transactions
	    startSplitFlags = 1;				// issue the SSplit on microframe 0
	    transactionPosition = 0;				// only used for OUT
	    transactionCount = 0;				// only used for OUT
	    completeSplitFlags = 0xFC;				// allow completes on 2,3,4,5,6,7
	}
        
        // calculate IOC and completion if necessary
	if (i == (frameCount-1))
	{
	    ioc = 1;
	    pNewSITD->completion = completion;
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
        pNewSITD->GetSharedLogical()->routeFlags = HostToUSBLong(
                                        ((pEP->direction == kUSBOut) ? 0 : (1 << kEHCIsiTDRouteDirectionPhase))
                                    |	(pEP->highSpeedPort <<  kEHCIsiTDRoutePortNumberPhase)
                                    |	(pEP->highSpeedHub <<  kEHCIsiTDRouteHubAddrPhase)
                                    |	(pEP->endpointNumber <<  kEHCIsiTDRouteEndpointPhase)
                                    |	(pEP->functionAddress << kEHCIsiTDRouteDeviceAddrPhase)
                                    );
        pNewSITD->GetSharedLogical()->timeFlags = HostToUSBLong(
					(completeSplitFlags << kEHCIsiTDTimeCMaskPhase) 
				    | 	(startSplitFlags << kEHCIsiTDTimeSMaskPhase)
					);
        pNewSITD->GetSharedLogical()->statFlags = HostToUSBLong(
					(ioc << kEHCIsiTDStatIOCPhase) 
				    | 	(reqCount << kEHCIsiTDStatLengthPhase)
				    |	(kEHCIsiTDStatStatusActive)
					);
        pNewSITD->GetSharedLogical()->buffPtr1 |= HostToUSBLong(	/* Buff pointer already set up */
					(transactionPosition << kEHCIsiTDBuffPtr1TPPhase)
				    |	(transactionCount)
					);
        pNewSITD->GetSharedLogical()->backPtr = HostToUSBLong(kEHCITermFlag);
        pNewSITD->myEndpoint = pEP;
	
	PutTDonToDoList(pEP, pNewSITD);
    }
    AddIsocFramesToSchedule(pEP);
    EnablePeriodicSchedule();
	
    return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::UIMCreateIsochTransfer(	short  			functionAddress,
					short 			endpointNumber,
					IOUSBIsocCompletion 	completion,
					UInt8 			direction,
					UInt64 			frameStart,
					IOMemoryDescriptor 	*pBuffer,
					UInt32 			frameCount,
					IOUSBIsocFrame 		*pFrames)
{
    AppleEHCIIsochEndpointPtr		pEP;

    USBLog(7, "%s[%p]::UIMCreateIsochTransfer - adr=%d:%d cbp=%p:%lx (cback=[%lx:%lx:%lx])", getName(), this,  
	    functionAddress, endpointNumber, pBuffer, 
	    pBuffer->getLength(), 
	    (UInt32)completion.action, (UInt32)completion.target, 
	    (UInt32)completion.parameter);
    if ( (frameCount == 0) || (frameCount > 200) )
    {
        USBLog(3,"%s[%p]::UIMCreateIsochTransfer bad frameCount: %d",getName(), this, frameCount);
        return kIOReturnBadArgument;
    }

    pEP = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);
    if(pEP == NULL)
    {
        USBLog(1, "%s[%p]::UIMCreateIsochTransfer - Endpoint not found", getName(), this);
        return kIOUSBEndpointNotFound;        
    }
    
    if (pEP->highSpeedHub)
	return CreateSplitIsochTransfer(pEP, completion, frameStart, pBuffer, frameCount, pFrames);
    else
	return CreateHSIsochTransfer(pEP, completion, frameStart, pBuffer, frameCount, pFrames);
}



IOReturn
AppleUSBEHCI::UIMCreateIsochTransfer(	short 				functionAddress,
					short 				endpointNumber,
					IOUSBIsocCompletion		completion,
					UInt8 				direction,
					UInt64 				frameNumberStart,
					IOMemoryDescriptor 		*pBuffer,
					UInt32 				frameCount,
					IOUSBLowLatencyIsocFrame	*pFrames,
					UInt32 				updateFrequency)
{
    AppleEHCIIsochEndpointPtr		pEP;

    USBLog(7, "%s[%p]::UIMCreateIsochTransfer - adr=%d:%d cbp=%p:%lx (cback=[%lx:%lx:%lx])", getName(), this,  
	    functionAddress, endpointNumber, pBuffer, 
	    pBuffer->getLength(), 
	    (UInt32)completion.action, (UInt32)completion.target, 
	    (UInt32)completion.parameter);
    if ( (frameCount == 0) || (frameCount > 200) )
    {
        USBLog(3,"%s[%p]::UIMCreateIsochTransfer bad frameCount: %d",getName(), this, frameCount);
        return kIOReturnBadArgument;
    }
    pEP = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);

    if(pEP == NULL)
    {
        USBLog(1, "%s[%p]::UIMCreateIsochTransfer - Endpoint not found", getName(), this);
        return kIOUSBEndpointNotFound;        
    }
    
    if (pEP->highSpeedHub)
	return CreateSplitIsochTransfer(pEP, completion, frameNumberStart, pBuffer, frameCount, (IOUSBIsocFrame*)pFrames, updateFrequency, true);
    else
	return CreateHSIsochTransfer(pEP, completion, frameNumberStart, pBuffer, frameCount, pFrames, updateFrequency);
}


void 
AppleUSBEHCI::AddIsocFramesToSchedule(AppleEHCIIsochEndpointPtr pEP)
{
    UInt64 				currFrame;
    AppleEHCIIsochListElement		*pTD;
    UInt16 				nextSlot;

    pTD = GetTDfromToDoList(pEP);
    if(pTD == NULL)
    {
	USBLog(7, "%s[%p]::AddIsocFramesToSchedule - no frames to add fn:%d EP:%d", getName(), this, pEP->functionAddress, pEP->endpointNumber);
	return;
    }

    // Don't get GetFrameNumber() unless we're going to use it
    //
    currFrame = GetFrameNumber();
    
    USBLog(7, "%s[%p]::AddIsocFramesToSchedule - fn:%d EP:%d inSlot (0x%x), currFrame: 0x%Lx", getName(), this, pEP->functionAddress, pEP->endpointNumber, pEP->inSlot, currFrame);
    
    while(pTD->frameNumber <= (currFrame+1))	// Add 1, and use <= so you never put in a new frame 
						// at less than 2 ahead of now. (EHCI spec, 7.2.1)
    {
	IOReturn	ret;
	
	// this transaction is old before it began, move to done queue
	USBLog(5, "%s[%p]::AddIsocFramesToSchedule - ignoring TD(%p) because it is too old (%Lx) vs (%Lx) ", getName(), this, pTD, pTD->frameNumber, currFrame);
	ret = pTD->UpdateFrameList();		// TODO - accumulate the return values
	PutTDonDoneQueue(pEP, pTD);
	    
	pTD = GetTDfromToDoList(pEP);
	if(pTD == NULL)
	{	
	    // Run out of transactions to move
	    ReturnIsocDoneQueue(pEP);
	    return;
	}
    }
    
    currFrame = pTD->frameNumber;		// start looking at the first available number
    if(pEP->inSlot > kEHCIPeriodicListEntries)
    {
	// This needs to be fixed up when we have variable length lists.
	pEP->inSlot = currFrame & (kEHCIPeriodicListEntries-1);
    }
    while (pEP->inSlot != (currFrame & (kEHCIPeriodicListEntries-1)))
    {
	// need to make sure that the match the slot with the frame in the TD
	pEP->inSlot = (pEP->inSlot + 1) & (kEHCIPeriodicListEntries-1);
    }
    do
    {
	nextSlot = (pEP->inSlot + 1) & (kEHCIPeriodicListEntries-1);
	if( nextSlot == _outSlot) 								// weve caught up with our tail
	{
	    USBLog(6, "%s[%p]::AddIsocFramesToSchedule - caught up nextSlot (0x%x) _outSlot (0x%x)", getName(), this, nextSlot, _outSlot);
	    break;
	}
	
	USBLog(6, "%s[%p]::AddIsocFramesToSchedule - checking TD(%p) FN(0x%Lx) against currFrame (0x%Lx)", getName(), this, pTD, pTD->frameNumber, currFrame);
	if(currFrame == pTD->frameNumber)
	{
	    if(_outSlot > kEHCIPeriodicListEntries)
	    {
		_outSlot = pEP->inSlot;
	    }
	    // Place TD in list
	    USBLog(6, "%s[%p]::AddIsocFramesToSchedule - linking TD (%p) with frame (0x%Lx) into slot (0x%x) - curr next log (%p) phys (%p)", getName(), 
								this, pTD, pTD->frameNumber, pEP->inSlot, _logicalPeriodicList[pEP->inSlot], USBToHostLong(_periodicList[pEP->inSlot]));
	    pTD->SetPhysicalLink(_periodicList[pEP->inSlot]);
	    pTD->_logicalNext = _logicalPeriodicList[pEP->inSlot];
	    _logicalPeriodicList[pEP->inSlot] = pTD;
	    _periodicList[pEP->inSlot] = HostToUSBLong(pTD->GetPhysicalAddrWithType());
	    USBLog(6, "%s[%p]::AddIsocFramesToSchedule - _periodicList[%x]:%x", getName(), this, pEP->inSlot, USBToHostLong(_periodicList[pEP->inSlot]));
		
	    // pTD->print();
	    // and fetch the next one
	    pTD = GetTDfromToDoList(pEP);
	}
	currFrame++;
	pEP->inSlot = nextSlot;
	USBLog(6, "%s[%p]::AddIsocFramesToSchedule - pEP->inSlot is now 0x%x", getName(), this, pEP->inSlot);	
    } while(pTD != NULL);

    USBLog(7, "%s[%p]::AddIsocFramesToSchedule - finished,  currFrame: %Lx", getName(), this, GetFrameNumber() );

}



void 
AppleUSBEHCI::ReturnIsocDoneQueue(AppleEHCIIsochEndpointPtr pEP)
{
    AppleEHCIIsochListElement		 	*pTD = GetTDfromDoneQueue(pEP);
    IOUSBIsocFrame 				*pFrames;
    
    USBLog(7, "%s[%p]::ReturnIsocDoneQueue (%p)", getName(), this, pEP);
    if (pTD)
    {
	// HS always stores the big frame pointer in the first TD
	pFrames = pTD->myFrames;
    }
    while(pTD)
    {
	if( pTD->completion.action != NULL)
	{
	    IOUSBIsocCompletionAction 	pHandler;
	    IOReturn			ret = kIOReturnSuccess;
	    
	    pHandler = pTD->completion.action;
	    pTD->completion.action = NULL;
            USBLog(7, "%s[%p]::ReturnIsocDoneQueue- calling handler(%p, %p, %p, %p)", getName(), this, pTD->completion.target, pTD->completion.parameter, ret, pTD->myFrames);
	    (*pHandler) (pTD->completion.target,  pTD->completion.parameter, pEP->accumulatedStatus, pFrames);
	    pEP->accumulatedStatus = kIOReturnSuccess;
	    pTD->Deallocate(this);
	    pTD = GetTDfromDoneQueue(pEP);
	    if (pTD)
		pFrames = pTD->myFrames;
	}
	else
	{
	    pTD->Deallocate(this);
	    pTD = GetTDfromDoneQueue(pEP);
	}
    }
    
}



IOReturn 		
AppleUSBEHCI::UIMHubMaintenance(USBDeviceAddress highSpeedHub, UInt32 highSpeedPort, UInt32 command, UInt32 flags)
{
    AppleUSBEHCIHubInfoPtr	hiPtr;

    switch (command)
    {
	case kUSBHSHubCommandAddHub:
	    USBLog(7, "%s[%p]::UIMHubMaintenance - adding hub %d with flags %x", getName(), this, highSpeedHub, flags);
	    hiPtr = GetHubInfo(highSpeedHub, 0);
	    if (hiPtr)
	    {
		USBLog(7, "%s[%p]::UIMHubMaintenance - adding hub which already exists (%d)", getName(), this, highSpeedHub);
		hiPtr->flags = flags;
	    }
	    else
	    {
		hiPtr = NewHubInfo(highSpeedHub, 0);
		if (!hiPtr)
		    return kIOReturnNoMemory;
		USBLog(7, "%s[%p]::UIMHubMaintenance - done creating new hub (%p) for address (%d)", getName(), this, hiPtr, highSpeedHub);
		hiPtr->flags = flags;
		hiPtr->bandwidthAvailable = 1024;
	    }
	    break;
	    
	case kUSBHSHubCommandRemoveHub:
	    USBLog(7, "%s[%p]::UIMHubMaintenance - deleting hub %d", getName(), this, highSpeedHub);
	    DeleteHubInfo(highSpeedHub, 0xffff);	// remove all ports
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

    USBLog(6, "ReturnOneTransaction Enter with transaction %p",transaction);

    while(transaction!= NULL)
    {
	if(transaction->lastTDofTransaction)
	{
	    transaction = transaction->pLogicalNext;
	    break;
	}
	transaction = transaction->pLogicalNext;
	USBLog(7, "ReturnOneTransaction next transaction %p",transaction);
    }
    USBLog(6, "ReturnOneTransaction going with transaction %p",transaction);
    if(transaction == NULL)
    {
	// This works, sort of, NULL for an end transction means remove them all.
	// But there will be no callback
	USBLog(1, "%s[%p]::ReturnOneTransaction - returning all TDs on the queue", getName(), this);
    }
    else
    {
    
    }
    returnTransactions(pED, transaction);
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
    AppleEHCIQueueHead			*pED = head;
    AppleEHCIQueueHead			*pEDBack = NULL, *pEDBack1 = NULL;
    IOPhysicalAddress			pTDPhys;
    EHCIGeneralTransferDescriptor 	*pTD;
	
    UInt32 				noDataTimeout;
    UInt32				completionTimeout;
    UInt32				rem;
    UInt32				curFrame = GetFrameNumber32();

    for (; pED != 0; pED = (AppleEHCIQueueHead *)pED->_logicalNext)
    {
	USBLog(7, "%s[%p]::CheckEDListForTimeouts - checking ED [%p]", getName(), this, pED);
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
	pTD = pED->qTD;
	if (!pTD)
	{
	    USBLog(6, "%s[%p]::CheckEDListForTimeouts - no TD", getName(), this);
	    continue;
	}

	if (!pTD->command)
	{
	    USBLog(7, "%s[%p]::CheckEDListForTimeouts - found a TD without a command - moving on", getName(), this);
	    continue;
	}
	
	if (pTD == pED->TailTD)
	{
	    USBLog(1, "%s[%p]::CheckEDListForTimeouts - ED (%p) - TD is TAIL but there is a command - pTD (%p)", getName(), this, pED, pTD);
	    pED->print(5);
	}
	
	if(pTDPhys != pTD->pPhysical)
	{
	    USBLog(5, "%s[%p]::CheckEDListForTimeouts - pED (%p) - mismatched logical and physical - TD (%p) will be scavenged later", getName(), this, pED, pTD);
	    pED->print(5);
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
//	USBLog(3, "%s[%p]::CheckEDListForTimeouts @@@@@@@ Frame curr/first %lx/%lx time %lx", getName(), this, curFrame, firstActiveFrame, completionTimeout);
	    if ((curFrame - firstActiveFrame) >= completionTimeout)
	    {
		USBLog(2, "%s[%p]::CheckEDListForTimeout - Found a TD [%p] on QH [%p] past the completion deadline, timing out! (%x - %x)", getName(), this, pTD, pED, curFrame, firstActiveFrame);
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
//	USBLog(3, "%s[%p]::CheckEDListForTimeouts @@@@@@@ 5Sec: Frame curr/last %lx/%lx time %lx rem/last %lx/%lx", getName(), this, curFrame, pTD->lastFrame, noDataTimeout, pTD->lastRemaining, rem);
	if (pTD->lastRemaining != rem)
	{
	    // there has been some activity on this TD. update and move on
	    pTD->lastRemaining = rem;
	    continue;
	}
	if ((curFrame - pTD->lastFrame) >= noDataTimeout)
	{
	    USBLog(2, "(%p)Found a transaction which hasn't moved in 5 seconds, timing out! (%x - %x)", pTD, curFrame, pTD->lastFrame);
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
    UInt64		elapsedTime = 0;
    bool		allPortsDisconnected = false;

    // If we are not active anymore or if we're in ehciBusStateOff, then don't check for timeouts 
    //
    if ( isInactive() || (_onCardBus && _pcCardEjected) || !_ehciAvailable || (_ehciBusState != kEHCIBusStateRunning))
        return;

    // Check to see if our control or bulk lists have a TD that has timed out
    //
    CheckEDListForTimeouts(_AsyncHead);

    // See if it's time to check for Root Hub inactivity
    //
    if ( !_idleSuspend )
    {
        // Check to see if it's been kEHCICheckForRootHubConnectionsPeriod seconds
        // since we last checked this port
        //
        clock_get_uptime( &currentTime );
        SUB_ABSOLUTETIME(&currentTime, &_lastCheckedTime );
        absolutetime_to_nanoseconds(currentTime, &elapsedTime);
        elapsedTime /= 1000000000;				// Convert to seconds from nanoseconds
        
        if ( elapsedTime >= kEHCICheckForRootHubConnectionsPeriod )
        {
            USBLog(6,"%s[%p] Time to check for root hub inactivity on bus %d", getName(), this, _busNumber);
            clock_get_uptime( &_lastCheckedTime );
            
            // Check to see if the root hub has been inactive for kEHCICheckForRootHubInactivityPeriod seconds
            //
            allPortsDisconnected = RootHubAreAllPortsDisconnected();

            if ( allPortsDisconnected )
            {                
                // Find the last time we had a change in the root hub.  If it's been 30 secs or
                // more, then we are ready to suspend the ports
                //
                lastRootHubChangeTime = LastRootHubPortStatusChanged( false );
    
                clock_get_uptime( &currentTime );
                SUB_ABSOLUTETIME(&currentTime, &lastRootHubChangeTime );
                absolutetime_to_nanoseconds(currentTime, &elapsedTime);
                elapsedTime /= 1000000000;
            
                if ( elapsedTime >= kEHCICheckForRootHubInactivityPeriod )
                {
                    // Yes, nothing connected to this root hub and it's been more than kEHCICheckForRootHubInactivityPeriod secs since
                    // we last saw something happen on it, so let's suspend that bus
                    //
                    USBLog(5,"%s[%p] Time to suspend the ports of bus %d", getName(), this, _busNumber);
                    setPowerState( kEHCISetPowerLevelIdleSuspend, this);
                }
            }
        }
    }
}

#if 0
void AppleUSBEHCI::printED(AppleEHCIQueueHead * pED)
{
    if(pED == NULL)
    {
        USBLog(7, "%s[%p]::printED: Attempt to print null ED", getName(), this);
        return;
    }
    USBLog(5, "%s[%p]::printED: ----------------pED at %p", getName(), this, pED);
    USBLog(5, "%s[%p]::printED: nextQH:  %p", getName(), this, USBToHostLong(pED->GetSharedLogical()->nextQH));
    USBLog(5, "%s[%p]::printED: flags:   %p", getName(), this, USBToHostLong(pED->GetSharedLogical()->flags));
    USBLog(5, "%s[%p]::printED: slags:   %p", getName(), this, USBToHostLong(pED->GetSharedLogical()->splitFlags));
    USBLog(5, "%s[%p]::printED: CurrqTD: %p", getName(), this, USBToHostLong(pED->GetSharedLogical()->CurrqTDPtr));
    USBLog(5, "%s[%p]::printED: NextqTD: %p", getName(), this, USBToHostLong(pED->GetSharedLogical()->NextqTDPtr));
    USBLog(5, "%s[%p]::printED: AltqTD:  %p", getName(), this, USBToHostLong(pED->GetSharedLogical()->AltqTDPtr));
    USBLog(5, "%s[%p]::printED: TDFlags: %p", getName(), this, USBToHostLong(pED->GetSharedLogical()->qTDFlags));
    USBLog(5, "%s[%p]::printED: BuffPtr[0]: %p", getName(), this, USBToHostLong(pED->GetSharedLogical()->BuffPtr[0]));
    USBLog(5, "%s[%p]::printED: pLgNext: %p", getName(), this, pED->_logicalNext);
    USBLog(5, "%s[%p]::printED: pEDSharedPhys: %p", getName(), this, pED->_sharedPhysical);
    USBLog(5, "%s[%p]::printED: head qTD: %p", getName(), this, pED->qTD);
    USBLog(5, "%s[%p]::printED: last qTD: %p", getName(), this, pED->TailTD);
	if(pED->qTD != NULL)
	{
    USBLog(5, "%s[%p]::printED: head qTD physical: %p", getName(), this, pED->qTD->pPhysical);
//    USBLog(5, "%s[%p]::printED: head qTD flags: %p", getName(), this, USBToHostLong(pED->qTD->GetSharedLogical()->flags));
	}
    USBLog(5, "%s[%p]::printED: -----------------------------", getName(), this);
}
#endif


