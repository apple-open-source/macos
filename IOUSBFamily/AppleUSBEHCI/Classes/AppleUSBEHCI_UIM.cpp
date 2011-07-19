/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright © 1998-2010 Apple Inc.  All rights reserved.
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
#include "USBTracepoints.h"

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


#pragma mark General
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
    UInt32                                  myMaxPacketSize, multiple;
	UInt32									mySpeed = 0;
	AppleEHCIQueueHead *					pED;
	EHCIGeneralTransferDescriptorPtr		pTD;
    
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - Addr: %d, EPT#: %d, MPS: %d, speed: %d, hubAddr: %d, hubPort: %d, dir: %d", this, 
		   functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
	
    if ( (highSpeedHub == 0) && (speed != kUSBDeviceSpeedHigh) )
    {
		USBLog(1, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - new endpoint NOT fixing up speed", this);
		USBTrace( kUSBTEHCI, kTPEHCIMakeEmptyEndPoint , functionAddress, endpointNumber, speed, 1 );
		// speed = kUSBDeviceSpeedHigh;
    }
	
    pED = FindControlBulkEndpoint(functionAddress, endpointNumber, NULL, direction);
    if (pED != NULL)
    {
		USBLog(1, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - old endpoint found, abusing %p", this, pED);
		USBTrace( kUSBTEHCI, kTPEHCIMakeEmptyEndPoint , functionAddress, endpointNumber, speed, 2 );
        pED->GetSharedLogical()->flags = 0xffffffff;
    }
	
    pED = AllocateQH();
	if (pED == NULL)
        return NULL;
	
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
	
    if (speed == kUSBDeviceSpeedFull)
    {
        mySpeed = 0;
    }
    else if (speed == kUSBDeviceSpeedLow)
	{
		mySpeed = 1  << kEHCIEDFlags_SPhase;
	}
    else if (speed == kUSBDeviceSpeedHigh)
	{
		mySpeed = 2  << kEHCIEDFlags_SPhase;
	}
    
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - mySpeed %d", this, (uint32_t)mySpeed);
	
    if( maxPacketSize > 1024 )
    {
		multiple		= ((maxPacketSize-1)/1024)+1;
		UInt32 oneMp	= (maxPacketSize+(multiple-1))/multiple;
		myMaxPacketSize = oneMp << kEHCIEDFlags_MPSPhase;
	}
	else
	{
		multiple		= 1;
		myMaxPacketSize = ((UInt32) maxPacketSize) << kEHCIEDFlags_MPSPhase;
	}
	
    pED->_direction = direction;
	pED->_functionNumber = functionAddress;
	pED->_endpointNumber = endpointNumber;
	pED->_speed = speed;
	
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - MPS = %d, setting flags to 0x%x", this, maxPacketSize, (uint32_t)(myFunctionAddress | myEndPointNumber | myMaxPacketSize | mySpeed));
    
    pED->GetSharedLogical()->flags = HostToUSBLong(myFunctionAddress | myEndPointNumber | myMaxPacketSize | mySpeed);
	
	
    pED->GetSharedLogical()->splitFlags = HostToUSBLong( (multiple << kEHCIEDSplitFlags_MultPhase)	// multiple transaction per traversal
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
    
    USBLog(7, "AppleUSBEHCI[%p]::MakeEmptyEndPoint - pointing NextqTDPtr to %x", this, (uint32_t)pTD->pPhysical);
    pED->GetSharedLogical()->NextqTDPtr = HostToUSBLong( (UInt32)pTD->pPhysical & ~0x1F);
	
    pED->_qTD = pTD;
    pED->_TailTD =  pED->_qTD;
	pED->_numTDs = 1;
	
    pED->_responseToStall = 0;
	
	// For inactive detection
    pED->_inactiveTD = 0;
	
    return pED;
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
			USBTrace( kUSBTEHCI, kTPEHCIDeleteEndpoint, functionAddress, endpointNumber, direction, kIOReturnBadArgument );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDDelete (endpointNumber, direction) in 9
        //
        USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: Attempting operation on root hub", this);
        return SimulateEDDelete( endpointNumber, direction);
    }
	
    piEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL));
    if (piEP)
	{
		USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: deleting Isoch endpoint(%p)", this, piEP);
		return DeleteIsochEP(piEP);
	}
    
    pED = FindControlBulkEndpoint (functionAddress, endpointNumber, &pEDQueueBack, direction);
    
    if(pED == NULL)
    {
		
		pED = FindInterruptEndpoint(functionAddress, endpointNumber, direction, NULL);
		if(pED == NULL)
		{
			USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint, endpoint not found", this);
			return kIOUSBEndpointNotFound;
		}
		USBLog(5, "AppleUSBEHCI[%p]::UIMDeleteEndpoint: deleting Int endpoint(%p)", this, pED);
		unlinkIntEndpoint(pED);
		ReturnInterruptBandwidth(pED);
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

#pragma mark ControlBulkCommon
AppleEHCIQueueHead * 
AppleUSBEHCI::AddEmptyCBEndPoint(UInt8					functionAddress,
								 UInt8					endpointNumber,
								 UInt16					maxPacketSize,
								 UInt8					speed,
								 USBDeviceAddress       highSpeedHub,
								 int                  	highSpeedPort,
								 UInt8					direction)
{
    AppleEHCIQueueHead *	CBED;
    UInt32					cFlag;
    UInt32					myDataToggleCntrl;
	
    USBLog(7, "AppleUSBEHCI[%p]::AddEmptyCBEndPoint speed %d @(%d, %d)", this, speed, highSpeedHub, highSpeedPort);
	
    CBED = MakeEmptyEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
	
    if (CBED == NULL)
		return(NULL);
	
    cFlag = 0;
    if (kEHCIEDDirectionTD == direction)
    {
		myDataToggleCntrl = 1 << kEHCIEDFlags_DTCPhase;
		if (speed != kUSBDeviceSpeedHigh)
		{
			cFlag = 1 << kEHCIEDFlags_CPhase;
		}
    }
    else
    {
		myDataToggleCntrl = 0;
    }
	
    CBED->GetSharedLogical()->flags |= HostToUSBLong(myDataToggleCntrl | cFlag);
	
	// note that it is possible for pED to be NULL here, which means it won't really be linked in until later
	linkAsyncEndpoint(CBED);
    
    return CBED;
}


void AppleUSBEHCI::checkHeads(void)
{
	// Usually want this turned off
#if defined(DONT_CHECK_UIM_QHS)
    AppleEHCIQueueHead *	pEDQueue;
	EHCIQueueHeadShared *   pQH;
	
	pEDQueue = _AsyncHead;
	if(pEDQueue == NULL)
	{
		return;
	}
	pQH = pEDQueue->GetSharedLogical();
	if( (pQH->flags & kEHCIEDFlags_H) == 0)
	{	
		USBLog(3, "AppleUSBEHCI[%p]::checkHeads ********** Head with no H bit: %lx", this, (long)pEDQueue);
	}
	pEDQueue = OSDynamicCast(AppleEHCIQueueHead, pEDQueue->_logicalNext);

	while(pEDQueue != NULL)
	{
		pQH = pEDQueue->GetSharedLogical();
		if( (pQH->flags & kEHCIEDFlags_H) != 0)
		{
			USBLog(3, "AppleUSBEHCI[%p]::checkHeads @@@@@@@@@@@ not Head with H bit: %lx", this, (long)pEDQueue);
		}
		pEDQueue = OSDynamicCast(AppleEHCIQueueHead, pEDQueue->_logicalNext);
	}	
#endif
}



AppleEHCIQueueHead * 
AppleUSBEHCI::FindControlBulkEndpoint( short				functionNumber, 
									   short				endpointNumber, 
									   AppleEHCIQueueHead	**pEDBack,
									   short				direction)
{
    UInt32					unique;
    AppleEHCIQueueHead *	pEDQueue;
    AppleEHCIQueueHead *	pEDQueueNext;
    AppleEHCIQueueHead *	pEDQueueBack;
	EHCIQueueHeadShared *   pQH;
    short					EDDirection;
	
    // search for endpoint descriptor
    unique = (UInt32) ((((UInt32) endpointNumber) << kEHCIEDFlags_ENPhase) | ((UInt32) functionNumber));
    
	//checkHeads();
    
    pEDQueue = _AsyncHead;
    pEDQueueBack = NULL;
	//USBLog(3, "AppleUSBEHCI[%p]::FindControlBulkEndpoint fn: %d, ep: %d, dir: %d", this, functionNumber, endpointNumber, direction);
    if(pEDQueue == NULL)
    {
		USBLog(7, "AppleUSBEHCI[%p]::FindControlBulkEndpoint - Active queue is empty", this);
    }
	else
	{
		if (!_pEHCIRegisters->AsyncListAddr && !_wakingFromHibernation)
		{
			USBLog(1, "AppleUSBEHCI[%p]::FindControlBulkEndpoint.. AsyncListAddr is NULL but _AsyncHead is not!!", this);
		}
	}

    
	// See if the EP is in the active queue, do not move inactive EDs to the inactive queue. This will be done in timeout
	
    while (pEDQueue != NULL)
	{
		pEDQueueNext = OSDynamicCast(AppleEHCIQueueHead, pEDQueue->_logicalNext);		// Grab this before you mess with links
		EDDirection = pEDQueue->_direction;
		pQH = pEDQueue->GetSharedLogical();
		if( ( (USBToHostLong(pQH->flags) & kEHCIUniqueNumNoDirMask) == unique) && ( ((EDDirection == kEHCIEDDirectionTD) || (EDDirection) == direction)) ) 
		{
			//USBLog(5, "AppleUSBEHCI[%p]::FindControlBulkEndpoint (active) - found pEDQueue: %lx", this, (long)pEDQueue);
			if (pEDBack)
				*pEDBack = pEDQueueBack;

			checkHeads();
			return pEDQueue;
		} 
		else 
		{
			//USBLog(6, "AppleUSBEHCI[%p]::FindControlBulkEndpoint (active) - found active pEDQueue: %lx", this, (long)pEDQueue);
			pEDQueueBack = pEDQueue;			
		}
		pEDQueue = pEDQueueNext;
    }
	
	
	// See if the ED is in the inactive queue, activate it if necessary.
	pEDQueue = _InactiveAsyncHead;
    if(pEDQueue == NULL)
    {
		checkHeads();
		USBLog(7, "AppleUSBEHCI[%p]::FindControlBulkEndpoint - InactiveQueue is NULL", this);
		return NULL;
    }

	pEDQueueBack = NULL;
   
    do {
		EDDirection = pEDQueue->_direction;
		pQH = pEDQueue->GetSharedLogical();
		if( ( (USBToHostLong(pQH->flags) & kEHCIUniqueNumNoDirMask) == unique) && ( ((EDDirection == kEHCIEDDirectionTD) || (EDDirection) == direction)) ) 
		{
			// Unlink from the inactive queue.
			if(pEDQueueBack == NULL)
			{
				//USBLog(6, "AppleUSBEHCI[%p]::FindControlBulkEndpoint (inactive) - found pEDQueue head of inactive list: %lx", this, (long)pEDQueue);
				// head of list
				_InactiveAsyncHead = OSDynamicCast(AppleEHCIQueueHead, pEDQueue->_logicalNext);
			}
			else 
			{
				//USBLog(6, "AppleUSBEHCI[%p]::FindControlBulkEndpoint (inactive) - found pEDQueue in inactive list: %lx", this, (long)pEDQueue);
				pEDQueueBack->_logicalNext = pEDQueue->_logicalNext;
			}

			USBLog(5, "AppleUSBEHCI[%p]::FindControlBulkEndpoint (inactive) - linking to active list: %lx", this, (long)pEDQueue);				
			// Link this to the Aysnc queue
			linkAsyncEndpoint(pEDQueue);
			
			if (!_pEHCIRegisters->AsyncListAddr && !_wakingFromHibernation)
			{
				USBLog(1, "AppleUSBEHCI[%p]::FindControlBulkEndpoint.. AsyncListAddr is NULL after linkAsyncEndpoint!!", this);
			}
			
			if(pEDBack != NULL)
			{
				// Find its back pointer. It should be immediatly after the head, so this will usually not loop
				if(pEDQueue == _AsyncHead)
				{
					*pEDBack = NULL;
				}
				else 
				{
					pEDQueueBack = _AsyncHead;
					while(pEDQueueBack != NULL)
					{
						pEDQueueNext = OSDynamicCast(AppleEHCIQueueHead, pEDQueueBack->_logicalNext);
						if(pEDQueueNext == pEDQueue)
						{
							break;
						}
						pEDQueueBack = pEDQueueNext;
					}
					*pEDBack = pEDQueueBack;
				}
			}
			checkHeads();
			
			return pEDQueue;
		}
		else
		{
			//USBLog(6, "AppleUSBEHCI[%p]::FindControlBulkEndpoint (inactive) - inactive list: %lx", this, (long)pEDQueue);			
		}
		pEDQueueBack = pEDQueue;
		pEDQueue = OSDynamicCast(AppleEHCIQueueHead, pEDQueue->_logicalNext);
    } while (pEDQueue != NULL);
	
	checkHeads();

    return NULL;
}



#pragma mark Control
IOReturn 
AppleUSBEHCI::UIMCreateControlEndpoint(UInt8 functionAddress, UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed)
{
    IOLog("EHCIUIM -- UIMCreateControlEndpoint old version called with no split params\n");
    return(kIOReturnInternalError);
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
AppleUSBEHCI::UIMCreateControlEndpoint(UInt8 functionAddress, UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed,
									   USBDeviceAddress highSpeedHub, int highSpeedPort)
{
    AppleEHCIQueueHead	*pEP;
	
    USBLog(3, "AppleUSBEHCI[%p]::UIMCreateControlEndpoint(%d, %d, %d, %d @(%d, %d))", this,
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

    pEP = AddEmptyCBEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, kEHCIEDDirectionTD);
	if (!pEP)
	{
		USBError(1, "AppleUSBEHCI[%p]::UIMCreateControlEndpoint - could not add empty endpoint", this);
		return kIOReturnNoResources;
	}
	
	// note that by the time the _queueType is updated, the endpoint is already active on the QH list. However, because we are inside of the 
	// workloop gate, and this field is not used by either the HC hardware or by the FilterInterrupt routine, this is OK.
	pEP->_queueType = kEHCITypeControl;
 	printAsyncQueue(7, "UIMCreateControlEndpoint", true, false);
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBEHCI::UIMCreateControlTransfer(short				functionAddress,
									   short				endpointNumber,
									   IOUSBCompletion		completion,
									   void*				CBP,
									   bool					bufferRounding,
									   UInt32				bufferSize,
									   short				direction)
{
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "AppleUSBEHCI[%p]::UIMCreateControlTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
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
    IOReturn					status;
    IOUSBCompletion				completion = command->GetUSLCompletion();
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateControlTransfer adr=%d:%d cbp=%p:%x br=%s cback=[%p:%p] dir=%d)",
		   this, functionAddress, endpointNumber, CBP, (uint32_t)bufferSize,
		   bufferRounding ? "YES":"NO", completion.target, completion.parameter, direction);
    
    // search for endpoint descriptor
    pEDQueue = FindControlBulkEndpoint(functionAddress, endpointNumber, NULL, kEHCIEDDirectionTD);
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateControlTransfer -- found endpoint at %p", this, pEDQueue);
	
    if (pEDQueue == NULL)
    {
        USBLog(3, "AppleUSBEHCI[%p]::UIMCreateControlTransfer- Could not find endpoint!", this);
        return kIOUSBEndpointNotFound;
    }
	
    status = allocateTDs(pEDQueue, command, CBP, bufferSize, direction, true);
	
    if (status == kIOReturnSuccess)
    {
		USBLog(7, "AppleUSBEHCI[%p]::UIMCreateControlTransfer allocateTDS done - CMD = 0x%x, STS = 0x%x", this, USBToHostLong(_pEHCIRegisters->USBCMD), USBToHostLong(_pEHCIRegisters->USBSTS));
		printAsyncQueue(7, "UIMCreateControlTransfer", true, false);
		EnableAsyncSchedule(false);
    }
    else
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateControlTransfer (adr=%d:%d) - allocateTDs returned error %x", this, functionAddress, endpointNumber, status);
    }
    
    return status;
}


#pragma mark AllocateTDs
IOReturn 
AppleUSBEHCI::allocateTDs(AppleEHCIQueueHead* pEDQueue, IOUSBCommand *command, IOMemoryDescriptor* CBP, UInt32 bufferSize, UInt16 direction, Boolean controlTransaction)
{
	
    EHCIGeneralTransferDescriptorPtr	pTD1, pTD, pTDnew, pTDLast;
    UInt32								myToggle = 0;
	UInt32								myCerr = (3 << kEHCITDFlags_CerrPhase);
	UInt32								debugRetryCount = 0;
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
	
    if (controlTransaction)
    {
		myToggle = 0;	// Use data0 for setup 
		if (direction != kEHCIEDDirectionTD)
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
		USBTrace( kUSBTEHCI, kTPEHCIAllocateTDs , (uintptr_t)this, endpoint, kIOReturnNotPermitted, 0);
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
	pEDQueue->_numTDs++;
	
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
			pEDQueue->_numTDs--;
			return kIOReturnNoMemory;
		}
		if (dmaCommand->getMemoryDescriptor() != CBP)
		{
			USBError(1, "AppleUSBEHCI[%p]::allocateTDs - mismatched CBP (%p) and dmaCommand memory descriptor (%p)", this, CBP, dmaCommand->getMemoryDescriptor());
			DeallocateTD(pTD);
			pEDQueue->_numTDs--;
			return kIOReturnInternalError;
		}
	}

	debugRetryCount = ((gUSBStackDebugFlags & kUSBDebugRetryCountMask) >> kUSBDebugRetryCountShift);
	if (debugRetryCount and (debugRetryCount < 3))
	{
		USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - using retryCount of %d", this, (int)debugRetryCount);
		myCerr = debugRetryCount << kEHCITDFlags_CerrPhase;
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
			
			USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - gen64IOVMSegments returned length of %d (out of %d) and start of %p:%p", this, (uint32_t)totalPhysLength, (uint32_t)bufferSize, (void*)dmaAddrHighBits, (void*)dmaStartAddr);
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
						USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - segment is larger than 1 TD - using %d pages for this TD", this, (uint32_t)(kEHCIPagesPerTD - curTDsegment));
						bytesToSchedule = (kEHCIPagesPerTD - curTDsegment) * kEHCIPageSize;
					}
				}
				else
					bytesToSchedule = totalPhysLength;
				
				bytesThisTD += bytesToSchedule;
				transferOffset += bytesToSchedule;

				
				// <radr://6973523> Overrun with large interrupt transfer with unusual packet size.
				// With an interrupt endpoint which has a max packet size not a submultiple of 4k
				// with a transfer size which needs more than 1 TD, you get overrun errors at the 
				// end of the TD because the TD size is not an exact multiple of max packet
				// This adjusts the end of the TD so it is an exact multiple of max packet
				
				if( ((kEHCIPagesPerTD-1) == curTDsegment) && (maxPacket != 0) && ( (kEHCIPageSize%maxPacket) != 0) )
				{
					// Last TD segment, and odd sized max packet
					
					if(transferOffset < bufferSize)
					{
						// Not completely filled
						UInt32 ovBytes;
						ovBytes = bytesThisTD % maxPacket;	// number of bytes we need to knock off end of transfer to make even packet transfer.
						
						if(bytesToSchedule > ovBytes)		// Not a full TD. This is the driver's problem, not ours.
						{
							bytesToSchedule -= ovBytes;
							bytesThisTD -= ovBytes;
							transferOffset -= ovBytes;
							USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - Adjusted for odd maxpacket: bytesToSchedule:%d, bytesThisTD:%d, transferOffset:%d, ovBytes:%d", this, (int)bytesToSchedule, (int)bytesThisTD, (int)transferOffset, (int)ovBytes);
						}
					}
				}
				
                // If our transfer for this TD does not end on a page boundary, we need to close the TD
                //
                if ( (transferOffset < bufferSize) && ((dmaStartOffset+bytesToSchedule) & kEHCIPageOffsetMask) )
                {
                    USBLog(6, "AppleUSBEHCI[%p]::allocateTDs - non-last transfer didn't end at end of page (%d, %d)", this, (uint32_t)dmaStartOffset, (uint32_t)bytesToSchedule);
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
					USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - didn't fill up this TD (segment %d) - going back for more", this, (uint32_t)curTDsegment);
					continue;
				}
			}
			
            flags = kEHCITDioc;				// Want to interrupt on completion
			
			USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - i have %d bytes in %d segments", this, (uint32_t)bytesThisTD, (uint32_t)curTDsegment);
			for (segment = 0; segment < curTDsegment; segment++)
			{
				USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - addr[%d]:0x%x", this, (uint32_t)segment, (uint32_t)USBToHostLong(pTD->pShared->BuffPtr[segment]));
			}
			
            flags |= (bytesThisTD << kEHCITDFlags_BytesPhase);
			pTD->tdSize = bytesThisTD;	// Note for statistics
			pTD->flagsAtError = 0xffffffff; // A value you'll never see in the flags word
			pTD->errCount = 0;			// for software error handling, no errors yet
            pTD->pShared->altTD = HostToUSBLong(pTD1->pPhysical);	// point alt to first TD, will be fixed up later
			flags |= myDirection | myToggle | kEHCITDStatus_Active | myCerr;
			
			// this is for debugging
            // pTD->traceFlag = trace;
            pTD->traceFlag = false;
            pTD->pQH = pEDQueue;
			
			
            // only supply a callback when the entire buffer has been
            // transfered.
			
            // USBLog(5, "AppleUSBEHCI[%p]:new control path for data toggle");
			// adjust data toggle for multi-TD control transfers
			// this will adjust toggle on the next TD since the flags are already set above.
			if (controlTransaction)
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
				pTD->callbackOnTD = true;
				pTD->logicalBuffer = CBP;
				pTD->pShared->flags = HostToUSBLong(flags);
				pTD->multiXferTransaction = command->GetMultiTransferTransaction();
				pTD->finalXferInTransaction = command->GetFinalTransferInTransaction();
				if ((pEDQueue->_queueType == kEHCITypeControl) || (pEDQueue->_queueType == kEHCITypeBulk))
					_controlBulkTransactionsOut++;
				//if (trace)printTD(pTD);
            }
			else
            {
				pTD->callbackOnTD = false;
				//myToggle = 0;	// Only set toggle on first TD
				pTDnew = AllocateTD();
				if (pTDnew == NULL)
				{
					status = kIOReturnNoMemory;
					USBError(1, "AppleUSBEHCI[%p]::allocateTDs can't allocate new TD", this);
				}
				else
				{
					pEDQueue->_numTDs++;
					pTD->pShared->nextTD = HostToUSBLong(pTDnew->pPhysical);
					pTD->pLogicalNext = pTDnew;
					pTD->pShared->flags = HostToUSBLong(flags);		// Doesn't matter about flags, not linked in yet
					// if (trace)printTD(pTD);
					pTD = pTDnew;
					curTDsegment = 0;
					bytesThisTD = 0;
					USBLog(7, "AppleUSBEHCI[%p]::allocateTDs - got another TD - going to fill it up too (%d, %d)", this, (uint32_t)transferOffset, (uint32_t)bufferSize);
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
		pTD->callbackOnTD = true;
		pTD->multiXferTransaction = command->GetMultiTransferTransaction();
		pTD->finalXferInTransaction = command->GetFinalTransferInTransaction();
		if ((pEDQueue->_queueType == kEHCITypeControl) || (pEDQueue->_queueType == kEHCITypeBulk))
			_controlBulkTransactionsOut++;
		pTD->logicalBuffer = CBP;
		// pTD->traceFlag = trace;
		pTD->traceFlag = false;
		pTD->pQH = pEDQueue;
		flags = kEHCITDioc | myDirection | myToggle | kEHCITDStatus_Active | myCerr;
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
    pTDLast->callbackOnTD = pTD1->callbackOnTD;
    pTDLast->multiXferTransaction = pTD1->multiXferTransaction;
    pTDLast->finalXferInTransaction = pTD1->finalXferInTransaction;
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
		USBLog(3, "AppleUSBEHCI[%p::allocateTDs  returning status 0x%x", this, status);
    }
	USBLog(7, "AppleUSBEHCI[%p::allocateTDs  end: _numTDs now %d on %p", this, (uint32_t)pEDQueue->_numTDs, pEDQueue);
    return status;
}


#pragma mark Scavanging
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
	
    if ((flags & kEHCITDStatus_Halted) == 0)
    {
		// transaction completed sucessfully
		// Clear error state flag
		pEndpoint->_responseToStall = 0;
		return	kIOReturnSuccess;
    }
	
    if ( (flags & kEHCITDStatus_BuffErr) != 0)
    {	
		// Buffer over or under run error - i.e. the controller could not keep up from or to system memory
		if ( (flags & kEHCITDFlags_PID) == (1 << kEHCITDFlags_PIDPhase) )	
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
		if ( (flags & kEHCITDFlags_PID) == (1 << kEHCITDFlags_PIDPhase) )	
		{	
			// An in token
			return kOHCIITDConditionDataOverrun;
		}
		// for out token, we let the other error processing handle it, since the data over/underrun conditions are IN only
    }
	
    if ( (flags & kEHCITDFlags_Cerr) != 0)
    {	
		// A STALL
		// Check endpoint to see if we need to send a not responding instead
		if (  ((USBToHostLong(pEndpoint->GetSharedLogical()->flags) & kEHCIEDFlags_S) >> kEHCIEDFlags_SPhase) != 2)
		{
			// A full/low speed transaction, must be behind a transaction translator
			if ( (USBToHostLong(pEndpoint->GetSharedLogical()->splitFlags) & kEHCIEDSplitFlags_SMask) == 0)
			{
				// Not an Int transaction
				// Commute first and every other STALL to a not responding, you can't tell the differernce
				// according to the 2.0 spec where it goes completely off the rails.
				// See section 11.17.1 and appendicies A-14 and A-23
				
				pEndpoint->_responseToStall = 1 - pEndpoint->_responseToStall;	// Switch 0 - 1
				if (pEndpoint->_responseToStall == 1)
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
	
    if ( (flags & kEHCITDStatus_XactErr) != 0)
    {
		
		if (  ((USBToHostLong(pEndpoint->GetSharedLogical()->flags) & kEHCIEDFlags_S) >> kEHCIEDFlags_SPhase) != 2)
		{       // A split transaction
			return(kIOUSBHighSpeedSplitError);
		}
		USBLog(5, "mungeEHCIStatus - XactErr - not responding");
		return kOHCIITDConditionDeviceNotResponding;	// Can't tell this from any other transaction err
    }	
    
    if ( (flags & kEHCITDStatus_PingState) != 0)			// Int/Isoc gave error to transaction translator
    {
		return kOHCIITDConditionDeviceNotResponding;
    }	
	
    USBLog(1, "mungeECHIStatus condition we're not expecting 0x%x", (uint32_t)flags);
	USBTrace( kUSBTEHCI, kTPEHCIMungeECHIStatus , 0, flags, kOHCIITDConditionCRC, 0);
    
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
		if (status == (UInt32) kIOUSBHighSpeedSplitError)
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
			USBLog(_nextDoneQueue > 0 ? 3 : 7, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing queued level %d", this, (uint32_t)_nextDoneQueue);
			_nextDoneQueue++;
			return kIOReturnSuccess;
		} else 
		{
			USBError(1, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing queue buffer overflow (%d)", this, (int)_nextDoneQueue);
			currentQueue = _nextDoneQueue--;
			doOnce = TRUE;
			doLoop = FALSE;
		}
		
	}
	_nextDoneQueue++;
	
    while (doOnce || (doLoop && (currentQueue < _nextDoneQueue)))
	{
		doOnce = FALSE;
		
		USBLog(7, "+AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing (now at level %d)", this, (uint32_t)currentQueue);
		
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
			if (pHCDoneTD == stopAt)
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
					USBLog(4, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - TD (%p) - got transferStatus 0x%x with flags (0x%x)", this, pHCDoneTD, (uint32_t)transferStatus, (uint32_t)flags);
				}
				errStatus = TranslateStatusToUSBError(transferStatus);
				accumErr = errStatus;
				if (errStatus)
				{
					UInt32	myFlags = USBToHostLong(pHCDoneTD->pQH->GetSharedLogical()->flags);
					USBLog(4, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - got error 0x%x (%s), for bus: 0x%x, addr:  %d, ep: %d", this, errStatus, USBStringFromReturn(errStatus), (uint32_t)_busNumber, (uint32_t)((myFlags & kEHCIEDFlags_FA) >> kEHCIEDFlags_FAPhase), (uint32_t)((myFlags & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase));
				}
			}
			
			bufferSizeRemaining += (flags & kEHCITDFlags_Bytes) >> kEHCITDFlags_BytesPhase;
			
			if (pHCDoneTD->callbackOnTD)
			{
				if ( pHCDoneTD->command == NULL )
				{
					USBError (1, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing pHCDoneTD->command is NULL (%p)", this, pHCDoneTD);
				}
				else
				{
					IOUSBCompletion completion = pHCDoneTD->command->GetUSLCompletion();
					if (!safeAction || (safeAction == completion.action))
					{
						// remove flag before completing
						pHCDoneTD->callbackOnTD = false;
						
						_UIMDiagnostics.totalBytes -= bufferSizeRemaining;
							
						Complete(completion, errStatus, bufferSizeRemaining);
						
						if(pHCDoneTD->pQH)
						{
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
							USBError(1, "The EHCI driver has detected an error [pHCDoneTD->pQH == NULL]");
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
			if(pHCDoneTD->pQH)
			{
				pHCDoneTD->pQH->_numTDs--;
				USBLog(7, "AppleUSBEHCI[%p]::EHCIUIMDoDoneQueueProcessing - _numTDs now: %d on %p", this, (uint32_t)pHCDoneTD->pQH->_numTDs, pHCDoneTD->pQH);
			}
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
		USBLog(7, "AppleUSBEHCI[%p]::scavengeIsocTransactions - before reversal, cachedConsumer = 0x%x", this, (uint32_t)cachedConsumer);
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
		
		USBLog(7, "AppleUSBEHCI[%p]::scavengeIsocTransactions - after reversal, cachedConsumer[0x%x]", this, (uint32_t)cachedConsumer);
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
                USBLog(1, "AppleUSBEHCI[%p]::scavengeIsocTransactions - EP (%p) still had %d TDs on the reversed list!!", this, pEP, (uint32_t)pEP->onReversedList);
				USBTrace( kUSBTEHCI, kTPEHCIScavengeIsocTransactions , (uintptr_t)this, (uintptr_t)pEP, pEP->onReversedList, 0);
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
    uint64_t						timeStamp;
	
    pEP = pTD->_pEndpoint;
 	timeStamp = mach_absolute_time();
    if (pEP == NULL)
    {
		USBError(1, "AppleUSBEHCI[%p]::scavengeAnIsocEndPoint - could not find endpoint associated with iTD (%p)", this, pTD->_pEndpoint);
    }
    else
    {	
		if (!pTD->_lowLatency)
			ret = pTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);		// TODO - accumulate the return values
		PutTDonDoneQueue(pEP, pTD, true);
    }

#if 0
    if (pTD->_completion.action != NULL)
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
    UInt32								flags = 0, countq = 0, count = 0, flagsCErr = 0, debugRetryCount = 0;
    Boolean								TDisHalted, shortTransfer, foundNextTD, foundAltTD;
    AppleEHCIQueueHead					*pQH;
    
    while( (pListElem != NULL) && (countq++ < 150000) )
    {
		count = 0;
		pQH = OSDynamicCast(AppleEHCIQueueHead, pListElem);
		if (pQH)
		{
			qTD = qHead = pQH->_qTD;
			qEnd = pQH->_TailTD;
			if (((qTD == NULL) || (qEnd == NULL)) && (qTD != qEnd))
			{
				USBError(1, "The EHCI driver found a device queue with invalid head (%p) or tail (%p) - flags 0x%x", qTD, qEnd, (uint32_t) pQH->GetSharedLogical()->flags);
			}
			TDisHalted = false;
			shortTransfer = false;
			foundNextTD = false;
			foundAltTD = false;
			while( qTD && (qTD != qEnd) && (count++ < 150000) )
			{	
				// This end point has transactions
				flags = USBToHostLong(qTD->pShared->flags);
				flagsCErr = (flags & kEHCITDFlags_Cerr) >> kEHCITDFlags_CerrPhase;
				
				//USBLog(3, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue - flagsCErr: %d, debugRetryCount: %d", this, flagsCErr, (int)debugRetryCount);
				if (!flagsCErr)
				{
					_UIMDiagnostics.totalErrors++;	// I was checking to see if we're gathering statistics, but its less work just to note this.
					
					if( (gUSBStackDebugFlags & kUSBEnableErrorLogMask) != 0)
					{
						debugRetryCount = (gUSBStackDebugFlags & kUSBDebugRetryCountMask) >> kUSBDebugRetryCountShift;
						int endpoint = (USBToHostLong(pQH->GetSharedLogical()->flags)  & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase;
						USBLog(1, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue - %s TD[%p] Function(%d) EP (%d) had %d retry/retries - flagsCErr[%p] flags [%p]", 
							   this, 
							   (pQH->_queueType == kEHCITypeControl) ? "CONTROL" : ((pQH->_queueType == kEHCITypeBulk) ? "BULK" : "UNKNOWN"),
							   qTD,
							   (int)pQH->_functionNumber, endpoint, 
							   (int)debugRetryCount, (void*)flagsCErr, (void*)flags);						

						// Now try to software restart transaction if appropriate.
						if ( ((flags & kEHCITDStatus_XactErr) != 0) && ((flags & kEHCITDStatus_Halted) != 0) )	// Halted due to transaction error
						{
							if (flags == qTD->flagsAtError)
							{
								qTD->errCount++;
							}
							else
							{
								qTD->errCount = 1;	// Transaction moved since we last had an error (or first error), so start counting
							}
							
							qTD->flagsAtError = flags; // remember flags for last time, if transaction moves at all, they'll be different.
							if (qTD->errCount < 3)
							{
								_UIMDiagnostics.recoveredErrors++;		
								if(qTD->errCount == 2)
								{
									_UIMDiagnostics.errors2Strikes++;		
								}
								USBLog(1, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue - halted due to bus error, try restarting the transaction (%x), err count: %d", this, (uint32_t)flags, (uint32_t)qTD->errCount); 
								
								flags &= ~ kEHCITDFlags_Status;	// mask out status
								flags |= kEHCITDStatus_Active | (1<<kEHCITDFlags_CerrPhase);	// make active, with 1 err to go.
								qTD->pShared->flags = HostToUSBLong(flags);	// restart transaction
								pQH->GetSharedLogical()->qTDFlags = HostToUSBLong(flags); // and in the overlay.
								break;
							}
							else
							{
								_UIMDiagnostics.errors3Strikes++;		
								USBLog(1, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue - 3 consecutive errors, completing TD with error (%x), err count: %d", this, (uint32_t)flags, (uint32_t)qTD->errCount); 
							}
						}

					}
							   
							   
				
				}
				if(!TDisHalted && !shortTransfer)
				{
					if ((flags & kEHCITDStatus_Active) != 0)
					{	// Command is still alive, go to next queue
						break;
					}
					_UIMDiagnostics.totalBytes += qTD->tdSize;
					
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
				if (qTD->callbackOnTD)
				{
					// We have the complete command
					
					USBLog(7, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue - TD (%p) on ED (%p)", this, qTD, pQH); 
					if (doneQueue == NULL)
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
						USBError(1, "The EHCI driver found a NULL Transfer Descriptor - Queue flags 0x%x", (uint32_t) pQH->GetSharedLogical()->flags);
						break;
					}
					flags = USBToHostLong(pQH->GetSharedLogical()->qTDFlags);
					if (flags & kEHCITDStatus_Halted)
					{
						if (TDisHalted || shortTransfer)
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
							if ((flags & kEHCITDStatus_Active) != 0)	// Next command is still active
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
						USBError(1, "The EHCI driver found a NULL Transfer Descriptor - Queue flags 0x%x", (uint32_t) pQH->GetSharedLogical()->flags);
						break;
					}
				}
			}
			if (count > pQH->_numTDs)
			{
				USBLog(1, "AppleUSBEHCI[%p]::scavengeAnEndpointQueue looks like bad ed queue, count: %d, pQH->_numTDs: %d", this, (uint32_t)count, (uint32_t)pQH->_numTDs);
				USBTrace( kUSBTEHCI, kTPEHCIScavengeAnEndpointQueue , (uintptr_t)this, count, 0, 0);
			}
		}
		pListElem = (IOUSBControllerListElement*)pListElem->_logicalNext;
    }
    if (doneQueue != NULL)
    {
		EHCIUIMDoDoneQueueProcessing(doneQueue, kIOReturnSuccess, safeAction, NULL);
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
    if (err != kIOReturnSuccess)
    {
		USBLog(1, "AppleUSBEHCI[%p]::scavengeCompletedTransactions err isoch list %x", this, err);
		USBTrace( kUSBTEHCI, kTPEHCIScavengeCompletedTransactions , (uintptr_t)this, err, 0, 1);
    }
	
    if ( _AsyncHead != NULL )
    {
		err = scavengeAnEndpointQueue(_AsyncHead, safeAction);
		if (err != kIOReturnSuccess)
		{
			USBLog(1, "AppleUSBEHCI[%p]::scavengeCompletedTransactions err async queue %x", this, err);
			USBTrace( kUSBTEHCI, kTPEHCIScavengeCompletedTransactions , (uintptr_t)this, err, 0, 2);
		}
	}
	
	// Need to do the same for the Inactive queue as well, or else completions could be lost if
	// the QH were trimmed at an awkward time
    if ( _InactiveAsyncHead != NULL )
	{
		err = scavengeAnEndpointQueue(_InactiveAsyncHead, safeAction);
		if (err != kIOReturnSuccess)
		{
			USBLog(1, "AppleUSBEHCI[%p]::scavengeCompletedTransactions err inactive async queue %x", this, err);
			USBTrace( kUSBTEHCI, kTPEHCIScavengeCompletedTransactions , (uintptr_t)this, err, 0, 4);
		}
    }
	
    if ( _logicalPeriodicList != NULL )
    {
		// rdar:://8247421 - we used to scavenge all of the Periodic List entries. However, since this only scavenges Interrupt
		// transactions, (isoch is scavenged in the FilterInterrupt routine) then we only need to scavenge to the MaxPollingInterval(32)
		// as the periodic list just repeats itself after that.
        for(i = 0; i < kEHCIMaxPollingInterval; i++)
        {
            if (GetPeriodicListLogicalEntry(i) != NULL)
            {
                err1 = scavengeAnEndpointQueue(GetPeriodicListLogicalEntry(i), safeAction);
                if (err1 != kIOReturnSuccess)
                {
                    err = err1;
                    USBLog(1, "AppleUSBEHCI[%p]::scavengeCompletedTransactions err periodic queue[%d]:0x%x", this, i, err);
					USBTrace( kUSBTEHCI, kTPEHCIScavengeCompletedTransactions , (uintptr_t)this, err, i, 3);
                }
            }
        }
    }
}


#pragma mark Bulk
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
    AppleEHCIQueueHead 		*pEP;
    
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateBulkEndpoint(adr=%d:%d, max=%d, dir=%d)", this, functionAddress, endpointNumber, maxPacketSize, direction);
	
    if ( (direction != kUSBOut) && (direction != kUSBIn) )
    {
		USBLog(1, "AppleUSBEHCI[%p]::UIMCreateBulkEndpoint - wrong direction %d", this, direction);
		USBTrace( kUSBTEHCI, kTPEHCICreateBulkEndpoint , (uintptr_t) this, functionAddress, endpointNumber, direction);
		return kIOReturnBadArgument;
    }	
	
    if (highSpeedHub == 0)
		speed = kUSBDeviceSpeedHigh;
    else
		speed = kUSBDeviceSpeedFull;
	
    pEP = AddEmptyCBEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
    if (pEP == NULL)
	    return kIOReturnNoResources;
	
	// note that by the time the _queueType is updated, the endpoint is already active on the QH list. However, because we are inside of the 
	// workloop gate, and this field is not used by either the HC hardware or by the FilterInterrupt routine, this is OK.
	pEP->_queueType = kEHCITypeBulk;
	
	printAsyncQueue(7, "UIMCreateBulkEndpoint", true, false);
    return kIOReturnSuccess;
	
}



IOReturn 
AppleUSBEHCI::UIMCreateBulkTransfer(IOUSBCommand* command)
{
    AppleEHCIQueueHead 			*pEDQueue;
    IOReturn					status;
    IOMemoryDescriptor*			buffer = command->GetBuffer();
    short						direction = command->GetDirection();
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer - adr=%d:%d(%s) cbp=%p:%x cback=[%p:%p:%p])", this,
		   command->GetAddress(), command->GetEndpoint(), direction == kUSBIn ? "in" : "out",buffer, (int)command->GetReqCount(), 
		   command->GetUSLCompletion().action, command->GetUSLCompletion().target, command->GetUSLCompletion().parameter);
    
    pEDQueue = FindControlBulkEndpoint(command->GetAddress(), command->GetEndpoint(), NULL, direction);
    
    if (pEDQueue == NULL)
    {
        USBLog(3, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer- Could not find endpoint for addr(%d) ep (%d)!", this, (int)command->GetAddress(), (int)command->GetEndpoint());
        return kIOUSBEndpointNotFound;
    }
	
	status = allocateTDs(pEDQueue, command, buffer, command->GetReqCount(), direction, false );
    if (status == kIOReturnSuccess)
	{
		USBLog(7, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer allocateTDS done - CMD = 0x%x, STS = 0x%x", this, USBToHostLong(_pEHCIRegisters->USBCMD), USBToHostLong(_pEHCIRegisters->USBSTS));
		EnableAsyncSchedule(false);
	}
    else
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateBulkTransfer- allocateTDs (adr=%d:%d(%s))  returned error %x", this, command->GetAddress(), command->GetEndpoint(), direction == kUSBIn ? "in" : "out", status);
		USBTrace( kUSBTEHCI, kTPEHCICreateBulkTransfer, (uintptr_t)this, status, 0, 0);
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



void 
AppleUSBEHCI::returnTransactions(AppleEHCIQueueHead *pED, EHCIGeneralTransferDescriptor *untilThisOne, IOReturn error, bool clearToggle)
{
    EHCIGeneralTransferDescriptorPtr	doneQueue = NULL, doneTail= NULL;
    bool								removedSome = false;
	
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions, pED(%p) until (%p), clearToggle: %d", this, pED, untilThisOne, clearToggle);
    pED->print(7, this);
	
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
    
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions, pED->qTD (L:%p P:0x%x) pED->TailTD (L:%p P:0x%x)", this, pED->_qTD, (uint32_t)pED->_qTD->pPhysical, pED->_TailTD, (uint32_t)pED->_TailTD->pPhysical);
	
	if (USBToHostLong(pED->GetSharedLogical()->NextqTDPtr) != (pED->_qTD->pPhysical & ~0x1F))
	{
		// 7592955 - make sure that the NextqTDPtr points to something we know and trust before we clear the "bytes to transfer" bits
		USBLog(5, "AppleUSBEHCI[%p]::returnTransactions - NextqTDPtr(%p) AltqTDPtr(%p) - changing NextqTDPtr(%p)", this, (void*)USBToHostLong(pED->GetSharedLogical()->NextqTDPtr), (void*)USBToHostLong(pED->GetSharedLogical()->AltqTDPtr), (void*)(pED->_qTD->pPhysical & ~0x1F));
		pED->GetSharedLogical()->NextqTDPtr = HostToUSBLong( (UInt32)pED->_qTD->pPhysical & ~0x1F);
	}
	
    USBLog(5, "AppleUSBEHCI[%p]::returnTransactions: clearing ED bit, qTDFlags = %x", this, USBToHostLong(pED->GetSharedLogical()->qTDFlags));
    if (clearToggle)
		pED->GetSharedLogical()->qTDFlags = 0;									// Ensure that next TD is fetched (not the ALT) and reset the data toggle
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
		
		linkAsyncEndpoint(pED);
		if (_myBusState == kUSBBusStateRunning)
			EnableAsyncSchedule(false);				// only do this is we are running and we just linked something back in
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
    
    USBLog(5, "AppleUSBEHCI[%p]::HandleEndpointAbort: Addr: %d, Endpoint: %d,%d, clearToggle: %d", this, functionAddress, endpointNumber, direction, clearToggle);
	
    if (functionAddress == _rootHubFuncAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "AppleUSBEHCI[%p]::HandleEndpointAbort: bad params - endpNumber: %d", this, endpointNumber );
			USBTrace( kUSBTEHCI, kTPEHCIHandleEndpointAbort , functionAddress, endpointNumber, kIOReturnBadArgument, 2 );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDDelete (endpointNumber, direction) in 9
        //
        USBLog(5, "AppleUSBEHCI[%p]::HandleEndpointAbort: Attempting operation on root hub", this);
        return SimulateEDAbort( endpointNumber, direction);
    }
	
    piEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL));
    if (piEP)
    {
		return AbortIsochEP(piEP);
    }
    
    pED = FindControlBulkEndpoint (functionAddress, endpointNumber, &pEDQueueBack, direction);
    if(pED != NULL)
    {
		if (pED->_aborting)
		{
			// 7946083 - don't allow the abort to recurse
			USBLog(1, "AppleUSBEHCI[%p]::HandleEndpointAbort - Control/Bulk endpoint [%p] already aborting - not recursing", this, pED);
			return kIOUSBClearPipeStallNotRecursive;
		}
		pED->_aborting = true;
		HaltAsyncEndpoint(pED, pEDQueueBack);
		returnTransactions(pED, NULL, kIOUSBTransactionReturned, clearToggle);				// this will unhalt the EP
    }
    else
    {
		pED = FindInterruptEndpoint(functionAddress, endpointNumber, direction, NULL);
		
		if(pED == NULL)
		{
			USBLog(1, "AppleUSBEHCI::HandleEndpointAbort, endpoint not found");
			USBTrace( kUSBTEHCI, kTPEHCIHandleEndpointAbort , functionAddress, endpointNumber, kIOUSBEndpointNotFound, 3 );
			return kIOUSBEndpointNotFound;
		}
		if (pED->_aborting)
		{
			// 7946083 - don't allow the abort to recurse
			USBLog(1, "AppleUSBEHCI[%p]::HandleEndpointAbort - Interrupt endpoint [%p] already aborting - not recursing", this, pED);
			return kIOUSBClearPipeStallNotRecursive;
		}
		pED->_aborting = true;
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
		USBTrace( kUSBTEHCI, kTPEHCIHandleEndpointAbort , USBToHostLong(pED->GetSharedLogical()->qTDFlags), kEHCITDStatus_Halted, 0, 4 );
	}
	pED->_aborting = false;
    return kIOReturnSuccess;
	
}

#pragma mark Async
void 
AppleUSBEHCI::linkAsyncEndpoint(AppleEHCIQueueHead *CBED)
{
    IOPhysicalAddress			newHorizPtr;
	AppleEHCIQueueHead			*pEDHead = _AsyncHead;
	UInt32						newPhysicalAddr = NULL;
	int							retries = 500;
	
    // Point first endpoint to itself
    newHorizPtr = CBED->GetPhysicalAddrWithType();		// Its a queue head
	
    USBLog(7, "AppleUSBEHCI[%p]::linkAsynEndpoint pEDHead %p", this, pEDHead);
	
    if(pEDHead == NULL)
    {
		CBED->GetSharedLogical()->flags |= HostToUSBLong(kEHCIEDFlags_H);
		CBED->SetPhysicalLink(newHorizPtr);
		CBED->_logicalNext = NULL;
		_AsyncHead = CBED;
		newPhysicalAddr = HostToUSBLong(CBED->_sharedPhysical);
		if (!isInactive())
		{
			_pEHCIRegisters->AsyncListAddr = newPhysicalAddr;
			
			if (_pEHCIRegisters->AsyncListAddr != newPhysicalAddr)
			{
				USBLog(1, "AppleUSBEHCI[%p]::linkAsyncEndpoint - AsyncListAddr did not stick right away..", this);
			}
			
			IOSync();
			
			while (retries-- && (_pEHCIRegisters->AsyncListAddr != newPhysicalAddr))
			{
				if ((retries % 10) == 0)
					{
						USBLog(1, "AppleUSBEHCI[%p]::linkAsyncEndpoint - AsyncListAddr not sticking yet, retries=%d", this, retries);
					}
			}
		
#if DEBUG_LEVEL != DEBUG_LEVEL_PRODUCTION
			if (!_pEHCIRegisters->AsyncListAddr)
				panic("AppleUSBEHCI::linkAsyncEndpoint.. AsyncListAddr is NULL after filling in with CBED->_sharedPhysical!!\n");
#endif
		}
		else
		{
			USBLog(2, "AppleUSBEHCI[%p]::linkAsyncEndpoint - I am inActive, so I don't need to set AsyncListAddr", this);
		}
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
	if (!_pEHCIRegisters->AsyncListAddr && !_wakingFromHibernation)
	{
		USBLog(1, "AppleUSBEHCI[%p]::linkAsyncEndpoint.. AsyncListAddr is NULL and we are not waking from hibernation!!\n", this);
	}
}



// This one can link the QH to the Async queue, or the inactive queue
// as necessary
void AppleUSBEHCI::maybeLinkAsyncEndpoint(AppleEHCIQueueHead *CBED)
{
	// For now, just link to active
	linkAsyncEndpoint(CBED);
}



void 
AppleUSBEHCI::unlinkAsyncEndpoint(AppleEHCIQueueHead * pED, AppleEHCIQueueHead * pEDQueueBack)
{
    UInt32					CMD, STS, count;	
	AppleEHCIQueueHead		*pNewHeadED = NULL;
	
    if( (pEDQueueBack == NULL) && (pED->_logicalNext == NULL) )
    {
        USBLog(7, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: removing sole endpoint %lx", this, (long)pED);
		// this is the only endpoint in the queue. we will leave list processing disabled
		DisableAsyncSchedule(true);
		printAsyncQueue(7, "unlinkAsyncEndpoint", true, false);
		pED->GetSharedLogical()->flags &= ~HostToUSBLong(kEHCIEDFlags_H);
		_AsyncHead = NULL;
		_pEHCIRegisters->AsyncListAddr = NULL;
		//pED->print(5);
    }
    else
    {
		USBLog(7, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: removing endpoint from queue %lx",this, (long)pED);
		
		// have to take this out of the queue
		
		if(_AsyncHead == pED)
		{
			USBLog(7, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: removing head endpoint %lx", this, (long)pED);
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
			printAsyncQueue(7, "unlinkAsyncEndpoint", true, false);
			pNewHeadED = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext); 

			_AsyncHead = pNewHeadED;
			
			// do not set the AsyncListAddr because the HC could be using it. The HC should update it since we are live
			// _pEHCIRegisters->AsyncListAddr = HostToUSBLong(pED->_logicalNext->_sharedPhysical);

			pEDQueueBack->SetPhysicalLink(pED->_logicalNext->GetPhysicalAddrWithType());
			
			// Make sure the one we give back does not have the H bit set
			pED->GetSharedLogical()->flags &= ~HostToUSBLong(kEHCIEDFlags_H);
			
			// Set the H bit on the next queue element, its now the head.
			pNewHeadED->GetSharedLogical()->flags |= HostToUSBLong(kEHCIEDFlags_H);
			//printAsyncQueue(7, "unlinkAsyncEndpoint");
			//pNewHeadED->print(5);
		}
		else if(pEDQueueBack != NULL)
		{
			printAsyncQueue(7, "unlinkAsyncEndpoint", true, false);
			pEDQueueBack->SetPhysicalLink(pED->GetPhysicalLink());
			pEDQueueBack->_logicalNext = pED->_logicalNext;
			printAsyncQueue(7, "unlinkAsyncEndpoint", true, false);
		}
		else
		{
			USBLog(7, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: ED not head, but pEDQueueBack not NULL",this);
		}
		
		if (isInactive())
		{
			USBLog(2, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: I am inactive, so not worrying about STS and CMD",this);
			return;
		}
		
		STS = USBToHostLong(_pEHCIRegisters->USBSTS);
		CMD = USBToHostLong(_pEHCIRegisters->USBCMD);
		
		// 5664375 - only need to do the following if the Async list is enabled in the CMD register
		if (CMD & kEHCICMDAsyncEnable) 
		{
			// ED is unlinked, now tell controller
			
			// 5664375 first make sure that the controller knows it is enabled..
			for (count=0; (count < 100) && !(STS & kEHCISTSAsyncScheduleStatus); count++)
			{
				IOSleep(1);
				STS = USBToHostLong(_pEHCIRegisters->USBSTS);
			}
			if (count)
			{
				USBLog(2, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: waited %d ms for the asynch schedule to come ON in the STS register", this, (int)count);
			}
			if (!(STS & kEHCISTSAsyncScheduleStatus))
			{
				USBLog(1, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint - the schedule status didn't go ON in the STS register!!", this);
				USBTrace( kUSBTEHCI, kTPEHCIUnlinkAsyncEndpoint , (uintptr_t)this, STS, kEHCISTSAsyncScheduleStatus, 1 );
			}
			else
			{
				// ring the doorbell
				_pEHCIRegisters->USBCMD = HostToUSBLong(CMD | kEHCICMDAsyncDoorbell);
				
				// Wait for controller to acknowledge
				
				STS = USBToHostLong(_pEHCIRegisters->USBSTS);
				count = 0;
				
				while((STS & kEHCIAAEIntBit) == 0)
				{
					IOSleep(1);
					STS = USBToHostLong(_pEHCIRegisters->USBSTS);
					count++;
					if ((count % 1000) == 0)
					{
						USBLog(2, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: count(%d) USBCMD(%p) USBSTS(%p) USBINTR(%p) ", this, (int)count, (void*)USBToHostLong(_pEHCIRegisters->USBCMD), (void*)USBToHostLong(_pEHCIRegisters->USBSTS), (void*)USBToHostLong(_pEHCIRegisters->USBIntr));
					}
					if ( count > 10000)
					{
						// Bail out after 10 seconds
						break;
					}
				};
				
				USBLog(7, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: delayed for %d ms after ringing the doorbell", this, (int)count);
				
				// Clear request
				_pEHCIRegisters->USBSTS = HostToUSBLong(kEHCIAAEIntBit);
				IOSync();
				if ((_pEHCIRegisters->AsyncListAddr & 0xFFFFFFE0) == pED->_sharedPhysical)
				{
					USBError(1, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint - pED[%p] seems to still be the AsyncListAddr after doorbell", this, pED);
					if (pNewHeadED)
					{
						_pEHCIRegisters->AsyncListAddr = HostToUSBLong(pNewHeadED->_sharedPhysical);
						IOSync();
					}
				}
			}
		}
		else
		{
			USBLog(5, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint  Async schedule was disabled", this);
			// make sure it is OFF in the status register as well before we leave this routine
			STS = USBToHostLong(_pEHCIRegisters->USBSTS);
			for (count=0; (count < 100) && (STS & kEHCISTSAsyncScheduleStatus); count++)
			{
				IOSleep(1);
				STS = USBToHostLong(_pEHCIRegisters->USBSTS);
			}
			if (count)
			{
				USBLog(2, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint: waited %d ms for the asynch schedule to go OFF in the STS register", this, (int)count);
			}
			STS = USBToHostLong(_pEHCIRegisters->USBSTS);
			if (STS & kEHCISTSAsyncScheduleStatus)
			{
				USBLog(1, "AppleUSBEHCI[%p]::unlinkAsyncEndpoint - the schedule status didn't go OFF in the STS register!!", this);
				USBTrace( kUSBTEHCI, kTPEHCIUnlinkAsyncEndpoint , (uintptr_t)this, STS, kEHCISTSAsyncScheduleStatus, 2 );
			}
		}
    }
}	


void
AppleUSBEHCI::printAsyncQueue(int level, const char* str, bool printSkipped, bool printTDs)
{
    AppleEHCIQueueHead *pED = _AsyncHead;
    
    if (pED)
    {
		USBLog(level, "AppleUSBEHCI[%p]::printAsyncQueue called from %s", this, str);
		USBLog(level, "--------------------");
		USBLog(level, "AppleUSBEHCI[%p]::printAsyncQueue: _AsyncHead[%p], AsyncListAddr[0x%x]", this, _AsyncHead, USBToHostLong(_pEHCIRegisters->AsyncListAddr));
		while (pED)
		{
			pED->print(level, this);
			if (printTDs)
			{
				EHCIGeneralTransferDescriptorPtr	td = pED->_qTD;
				while (td != pED->_TailTD)
				{
					printTD(td, level);
					td = td->pLogicalNext;
				}
			}
			pED = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext);
		}
    }
    else
    {
		USBLog(level, "AppleUSBEHCI[%p]::printAsyncQueue: NULL Async Queue called from %s", this, str);
    }
}



void
AppleUSBEHCI::printInactiveQueue(int level, const char* str, bool printSkipped, bool printTDs)
{
    AppleEHCIQueueHead *pED = _InactiveAsyncHead;
    
    if (pED)
    {
		USBLog(level, "AppleUSBEHCI[%p]::printInactiveQueue called from %s", this, str);
		USBLog(level, "--------------------");
		USBLog(level, "AppleUSBEHCI[%p]::printInactiveQueue: _InactiveAsyncHead[%p]", this, _InactiveAsyncHead);
		while (pED)
		{
			pED->print(level, this);
			if (printTDs)
			{
				// TODO
			}
			
			pED = OSDynamicCast(AppleEHCIQueueHead, pED->_logicalNext);
		}
    }
    else
    {
		USBLog(level, "AppleUSBEHCI[%p]::printAsyncQueue: NULL Async Queue called from %s", this, str);
    }
}



void
AppleUSBEHCI::printPeriodicList(int level, const char* str, bool printSkipped, bool printTDs)
{
		// TODO
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
	USBLog(level, "AppleUSBEHCI[%p]::printTD: pPhysical:   0x%x", this, (uint32_t)(pTD->pPhysical));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: pLogicalNext: %p", this, (pTD->pLogicalNext));
	USBLog(level, "AppleUSBEHCI[%p]::printTD: logicalBuffer:   %p", this, (pTD->logicalBuffer));	
	USBLog(level, "AppleUSBEHCI[%p]::printTD: callbackOnTD: %s", this, pTD->callbackOnTD ? "TRUE" : "FALSE");	
	USBLog(level, "AppleUSBEHCI[%p]::printTD: multiXferTransaction: %s", this, pTD->multiXferTransaction ? "TRUE" : "FALSE");	
	USBLog(level, "AppleUSBEHCI[%p]::printTD: finalXferInTransaction: %s", this, pTD->finalXferInTransaction ? "TRUE" : "FALSE");
	if (level < 7)
	{
		USBTrace( kUSBTEHCIDumpQueues, kTPEHCIDumpTD1, (uintptr_t)this, (uintptr_t)pTD, (uint32_t)pTD->pPhysical, (uintptr_t)pTD->pLogicalNext );
		USBTrace( kUSBTEHCIDumpQueues, kTPEHCIDumpTD2, (uintptr_t)this, (uint32_t)USBToHostLong(pTD->pShared->nextTD), (uint32_t)USBToHostLong(pTD->pShared->altTD), (uint32_t)USBToHostLong(pTD->pShared->flags) );
		USBTrace( kUSBTEHCIDumpQueues, kTPEHCIDumpTD3, (uintptr_t)this, (uint32_t)USBToHostLong(pTD->pShared->BuffPtr[0]), (uint32_t)USBToHostLong(pTD->pShared->BuffPtr[1]), (uint32_t)USBToHostLong(pTD->pShared->BuffPtr[2]) );
		USBTrace( kUSBTEHCIDumpQueues, kTPEHCIDumpTD4, (uintptr_t)this, (uint32_t)USBToHostLong(pTD->pShared->BuffPtr[3]), (uint32_t)USBToHostLong(pTD->pShared->BuffPtr[4]), 0 );
		USBTrace( kUSBTEHCIDumpQueues, kTPEHCIDumpTD5, (uintptr_t)this, (uint32_t)USBToHostLong(pTD->pShared->extBuffPtr[0]), (uint32_t)USBToHostLong(pTD->pShared->extBuffPtr[1]), (uint32_t)USBToHostLong(pTD->pShared->extBuffPtr[2]) );
		USBTrace( kUSBTEHCIDumpQueues, kTPEHCIDumpTD6, (uintptr_t)this, (uint32_t)USBToHostLong(pTD->pShared->extBuffPtr[3]), (uint32_t)USBToHostLong(pTD->pShared->extBuffPtr[4]), 0 );
		USBTrace( kUSBTEHCIDumpQueues, kTPEHCIDumpTD7, (uintptr_t)this, (uintptr_t)pTD->logicalBuffer, (uint32_t)pTD->callbackOnTD, (uintptr_t)pTD->multiXferTransaction );
		USBTrace( kUSBTEHCIDumpQueues, kTPEHCIDumpTD8, (uintptr_t)this, (uint32_t)pTD->finalXferInTransaction, 0, 0 );
	}
}



#pragma mark Interrupt
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
	
	USBLog(7, "AppleUSBEHCI[%p]::FindInterruptEndpoint - _greatestPeriod[%d]", this, (int)_greatestPeriod);
    for(i= 0; i < _greatestPeriod; i++)
    {
		pListElem = GetPeriodicListLogicalEntry(i);
		USBLog(7, "AppleUSBEHCI[%p]::FindInterruptEndpoint - i (%d) pListElem[%p]", this, i, pListElem);
		while ( pListElem != NULL)
		{
			pEDQueue = OSDynamicCast(AppleEHCIQueueHead, pListElem);
			if (pEDQueue && (pEDQueue != _dummyIntQH[i % kEHCIMaxPollingInterval]))
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
    AppleEHCIQueueHead *			pEP;
    IOUSBControllerListElement *	pLE;
    IOUSBControllerListElement *	temp;
    AppleUSBEHCIHubInfo *			hiPtr = NULL;
    AppleUSBEHCITTInfo *			ttiPtr = NULL;
	IOReturn						err;
	UInt32							currentToggle = 0;
	
    if (_rootHubFuncAddress == functionAddress)
	{
        return RootHubStartTimer32(pollingRate);
	}
	
    USBLog(EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::+UIMCreateInterruptEndpoint (%d, %d, %s, %s, %d, %d)", this, functionAddress, endpointNumber, 
		   (direction == kUSBIn) ? "in" : "out",
		   (speed == kUSBDeviceSpeedLow) ? "lo" : (speed == kUSBDeviceSpeedFull) ? "full" : "high", maxPacketSize, pollingRate);
	
    if( (speed == kUSBDeviceSpeedLow) && (maxPacketSize > 8) )
    {
		USBLog (1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - incorrect max packet size (%d) for low speed", this, maxPacketSize);
		return kIOReturnBadArgument;
    }
    
    if (pollingRate == 0)
		return kIOReturnBadArgument;
	
    // If the interrupt already exists, then we need to delete it first, as we're probably trying
    // to change the Polling interval via SetPipePolicy().
    //
    pEP = FindInterruptEndpoint(functionAddress, endpointNumber, direction, &temp);
    if ( pEP != NULL )
    {
        IOReturn ret;
        USBLog(3, "AppleUSBEHCI[%p]: UIMCreateInterruptEndpoint endpoint already existed -- deleting it", this);
		
		currentToggle = USBToHostLong(pEP->GetSharedLogical()->qTDFlags) & (kEHCITDFlags_DT);
		if ( currentToggle != 0)
		{
			USBLog(6,"AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint:  Preserving a data toggle of 1 before of the EP that we are going to delete!", this);
		}
		
        ret = UIMDeleteEndpoint(functionAddress, endpointNumber, direction);
        if ( ret != kIOReturnSuccess)
        {
            USBLog(3, "AppleUSBEHCI[%p]: UIMCreateInterruptEndpoint deleting endpoint returned 0x%x", this, ret);
            return ret;
        }
    }
    else
	{
        USBLog(5, "AppleUSBEHCI[%p]: UIMCreateInterruptEndpoint endpoint does NOT exist (this is normal)", this);
	}
	
	
    // Now go ahead and create a new endpoint
    pEP = MakeEmptyIntEndPoint(functionAddress, endpointNumber, maxPacketSize, speed, highSpeedHub, highSpeedPort, direction);
	
    if (pEP == NULL)
    {
        USBError(1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - could not create empty endpoint", this);
		return kIOReturnNoResources;
    }
	
    pEP->_maxPacketSize = maxPacketSize;
	pEP->_bInterval = pollingRate;										// set the original polling rate
	pEP->GetSharedLogical()->qTDFlags |= HostToUSBLong(currentToggle);	// Restore any data toggle from a deleted EP

	if (speed == kUSBDeviceSpeedHigh)
	{
		// for HS interrupt endpoints, the bInterval is an exponent 2^(bInterval-1)
		// which expresses a polling interval in microframes
		if (pollingRate > 16)
		{
			USBLog(1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - invalid polling rate (%d) for HS endpoint", this, pollingRate);
			pollingRate = 16;
		}
		pEP->_pollingRate = 1 << (pollingRate-1);
		
	}
	else
	{
		// this is a split transaction packet
		pEP->_pollingRate = pollingRate;		
		if (highSpeedHub)
		{
			// get the "master" hub Info for this hub to check the flags
			hiPtr = AppleUSBEHCIHubInfo::FindHubInfo(_hsHubs, highSpeedHub);
			if (!hiPtr)
			{
				USBLog (1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - no hub in list", this);
				return kIOReturnInternalError;
			}
			ttiPtr = hiPtr->GetTTInfo(highSpeedPort);
			if (!ttiPtr)
			{
				USBLog (1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - no TT infoavailable", this);
				return kIOReturnInternalError;
			}
		}
		else 
		{
			USBLog(1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - classic speed endpoint with no highSpeedHub - invalid!", this);
			DeallocateED(pEP);
			return kIOReturnBadArgument;
		}
		
	}
	
	err = AllocateInterruptBandwidth(pEP, ttiPtr);
	if (err)
	{
		USBLog(1, "AppleUSBEHCI[%p]::UIMCreateInterruptEndpoint - AllocateInterruptBandwidth returned err(%p)", this, (void*)err);
		DeallocateED(pEP);
		return err;
	}
	
    if(_greatestPeriod < (pEP->_startFrame + 1))
    {
		_greatestPeriod = pEP->_startFrame + 1;
    }
	
    linkInterruptEndpoint(pEP);
	
    USBLog(7, "AppleUSBEHCI[%p]::-UIMCreateInterruptEndpoint", this);
    return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
    IOReturn					status = kIOReturnSuccess;
    AppleEHCIQueueHead *		pEDQueue;
    IOUSBCompletion				completion = command->GetUSLCompletion();
    IOMemoryDescriptor*			buffer = command->GetBuffer();
    short						direction = command->GetDirection(); // our local copy may change
	
    USBLog(7, "AppleUSBEHCI[%p]::UIMCreateInterruptTransfer - adr=%d:%d cbp=%p:%qx br=%s cback=[%p:%p:%p])", this,  
		   command->GetAddress(), command->GetEndpoint(), command->GetBuffer(), 
		   (uint64_t)command->GetBuffer()->getLength(), command->GetBufferRounding()?"YES":"NO", 
		   completion.action, completion.target, 
		   completion.parameter);
	
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
		USBTrace( kUSBTEHCI, kTPEHCICreateInterruptTransfer , (uintptr_t)this, command->GetEndpoint(), kIOUSBEndpointNotFound, 1 );
		return kIOUSBEndpointNotFound;
    }
	
	
	if ( pEDQueue->_maxPacketSize == 0 )
	{
		USBLog(6, "AppleUSBEHCI[%p]::UIMCreateInterruptTransfer - maxPacketSize is 0, returning kIOUSBNotEnoughBandwidth", this);
		return kIOReturnNoBandwidth;
	}
	
    status = allocateTDs(pEDQueue, command, buffer, command->GetBuffer()->getLength(), direction, false );
    if(status != kIOReturnSuccess)
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateInterruptTransfer ((adr=%d:%d(%s)) allocateTDs returned error 0x%x", this, command->GetAddress(), command->GetEndpoint(), direction == kUSBIn ? "in" : "out", status);
		USBTrace( kUSBTEHCI, kTPEHCICreateInterruptTransfer , (uintptr_t)this, command->GetAddress(), status, 2 );
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



static IOUSBControllerListElement* 
FindIntEDqueue(IOUSBControllerListElement *start, UInt8 pollingRate)
{
    AppleEHCIQueueHead				*pQH;
    IOUSBControllerIsochListElement	*pIsoch = OSDynamicCast(IOUSBControllerIsochListElement, start);
    
    if (pIsoch)
    {
		// the list starts with some Isoch elements and we need to find the end of them
		while (OSDynamicCast(IOUSBControllerIsochListElement, start->_logicalNext))
		{
			pIsoch = OSDynamicCast(IOUSBControllerIsochListElement, start->_logicalNext);
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
    
    if(pQH && (pQH->NormalizedPollingRate() < pollingRate))
    {
		return pIsoch;
    }
	
    while(start->_logicalNext != NULL)
    {
		pQH = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
		if(pQH && (pQH->NormalizedPollingRate() < pollingRate))
		{
			break;
		}
		start = (IOUSBControllerListElement*)start->_logicalNext;
    }
	
    return start;
}



void  
AppleUSBEHCI::linkInterruptEndpoint(AppleEHCIQueueHead *pEP)
{
    short							pollingRate;
    UInt16							maxPacketSize;
    int								offset;
    IOUSBControllerListElement		*pLE;
    UInt32							newHorizPtr;
	
    maxPacketSize = pEP->_maxPacketSize;
    offset = pEP->_startFrame;

	pollingRate = pEP->NormalizedPollingRate();

    USBLog(7, "AppleUSBEHCI[%p]::linkInterruptEndpoint %p rate %d", this, pEP, pollingRate);
	pEP->print(7, this);
    newHorizPtr = pEP->GetPhysicalAddrWithType();
    while( offset < kEHCIPeriodicListEntries)
    {
		pLE = FindIntEDqueue(GetPeriodicListLogicalEntry(offset), pollingRate);
		if(pLE == NULL)
		{	// Insert as first in queue
			if(pEP->_logicalNext == NULL)
			{
				pEP->_logicalNext = GetPeriodicListLogicalEntry(offset);
				pEP->SetPhysicalLink(GetPeriodicListPhysicalEntry(offset));
			}
			else if (pEP->_logicalNext != GetPeriodicListLogicalEntry(offset))
			{
				USBError(1, "The Apple EHCI driver has found an endpoint with an incorrect link at the begining.");
			}
			
			SetPeriodicListEntry(offset, pEP);
			USBLog(7, "AppleUSBEHCI[%p]::linkInterruptEndpoint - inserted at top of list %d - next logical (%p) next physical (%p)", this, offset, pEP->_logicalNext, (void*)pEP->GetPhysicalLink());
		}
		else if (pEP != pLE)
		{	// Insert in middle/end of queue
			
			if(pEP->_logicalNext == NULL)
			{
				pEP->_logicalNext = pLE->_logicalNext;
				pEP->SetPhysicalLink(pLE->GetPhysicalLink());
			}
			else if (pEP->_logicalNext != pLE->_logicalNext)
			{
				USBError(1, "The Apple EHCI driver has found an endpoint with an incorrect link in the middle.");
			}
			// Point queue element to new endpoint
			pLE->_logicalNext = pEP;
			pLE->SetPhysicalLink(newHorizPtr);
			USBLog(7, "AppleUSBEHCI[%p]::linkInterruptEndpoint - inserted into list %d - next logical (%p) next physical (%p)", this, offset, pEP->_logicalNext, (void*)pEP->GetPhysicalLink());
		}
		else
		{
			// Else was already linked
			USBLog(7, "AppleUSBEHCI[%p]::linkInterruptEndpoint - (%p) already linked into %d (%p)", this, pEP, offset, pLE);
		}
		
		offset += pollingRate;
    } 
    _periodicEDsInSchedule++;
}



void 
AppleUSBEHCI::unlinkIntEndpoint(AppleEHCIQueueHead * pED)
{
    int								i;
    IOUSBControllerListElement *	pListElem;
    int								maxPacketSize;
    Boolean							foundED = false;
    short							pollingRate;
	
	pollingRate = pED->NormalizedPollingRate();
		
    USBLog(7, "+AppleUSBEHCI[%p]::unlinkIntEndpoint(%p) pollingRate(%d)", this, pED, pollingRate);
    
    maxPacketSize   =  (USBToHostLong(pED->GetSharedLogical()->flags)  & kEHCIEDFlags_MPS) >> kEHCIEDFlags_MPSPhase;
    
    for(i= pED->_startFrame; i < kEHCIPeriodicListEntries; i += pollingRate)
    {
		pListElem = GetPeriodicListLogicalEntry(i);
		if (pED == pListElem)
		{
			SetPeriodicListEntry(i, pED->_logicalNext);
			foundED = true;
			USBLog(7, "AppleUSBEHCI[%p]::unlinkIntEndpoint- found ED at top of list %d, new logical=%p, new physical=0x%x", this, i, GetPeriodicListLogicalEntry(i), GetPeriodicListPhysicalEntry(i));
		}
		else
		{
			while(pListElem != NULL)
			{
				if (pListElem->_logicalNext == pED)
				{
					pListElem->_logicalNext = pED->_logicalNext;
					pListElem->SetPhysicalLink(pED->GetPhysicalLink());
					foundED = true;
					USBLog(7, "AppleUSBEHCI[%p]::unlinkIntEndpoint- found ED in list %d, new logical=%p, new physical=%p", this, i, pED->_logicalNext, (void*)pED->GetPhysicalLink());
					break;
				}
				pListElem = OSDynamicCast(IOUSBControllerListElement, pListElem->_logicalNext);
			}
			if (pListElem == NULL)
			{
				USBLog(7, "AppleUSBEHCI[%p]::unlinkIntEndpoint endpoint not found in list %d", this, i);
			}
			
		}
    }
	
    IOSleep(1);				// make sure to clear the period list traversal cache
    pED->_logicalNext = NULL;
    
    if (foundED)
		_periodicEDsInSchedule--;
	
    USBLog(7, "-AppleUSBEHCI[%p]::unlinkIntEndpoint(%p)", this, pED);
}



#pragma mark Isoch
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
    AppleUSBEHCITTInfo				*ttiPtr = NULL;
    UInt32							xtraRequest;
	UInt32							decodedInterval;
	IOReturn						err;
    
    // we do not create an isoch endpoint in the hardware itself. isoch transactions are handled by 
    // TDs linked firectly into the Periodic List. There are two types of isoch TDs - split (siTD)
    // and high speed (iTD). This method will allocate an internal data structure which will be used
    // to track bandwidth, etc.
	
    USBLog(EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint(%d, %d, %d, %d, %d, %d)", this, functionAddress, endpointNumber, (uint32_t)maxPacketSize, direction, highSpeedHub, highSpeedPort);
	
    if (highSpeedHub == 0)
    {
        // Use a UInt16 to keep track of isoc bandwidth, then you can use
        // a pointer to it or to hiPtr->bandwidthAvailable for full speed.
		if ((interval == 0) || (interval > 16))
		{
			USBError(1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint: bad interval %d", this, interval);
			return kIOReturnBadArgument;
		}
		decodedInterval = (1 << (interval - 1));
    }
    else
    {
        // in this case we have a FS/LS device connected through a HS hub
        hiPtr = AppleUSBEHCIHubInfo::FindHubInfo(_hsHubs, highSpeedHub);
        if (!hiPtr)
        {
            USBLog (1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint - No hub in list", this);
			return kIOReturnInternalError;
        }
        ttiPtr = hiPtr->GetTTInfo(highSpeedPort);
        if (!ttiPtr)
        {
            USBLog (1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint - No TTI in list", this);
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
        USBLog(5,"AppleUSBEHCI[%p]::UIMCreateIsochEndpoint endpoint already exists, attempting to change maxPacketSize to %d", this, (uint32_t)maxPacketSize);
		// this is for High Speed devices
        curMaxPacketSize = pEP->maxPacketSize;
        if (maxPacketSize == curMaxPacketSize) 
		{
            USBLog(4, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint maxPacketSize (%d) the same, no change", this, (uint32_t)maxPacketSize);
            return kIOReturnSuccess;
        }
		ReturnIsochBandwidth(pEP);
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
    }
	else
	{
		pEP = OSDynamicCast(AppleEHCIIsochEndpoint, CreateIsochronousEndpoint(functionAddress, endpointNumber, direction));

		if (pEP == NULL) 
			return kIOReturnNoMemory;
		
		pEP->highSpeedHub = highSpeedHub;
		pEP->highSpeedPort = highSpeedPort;
		pEP->interval = decodedInterval;

		if (ttiPtr)
		{
			pEP->_speed = kUSBDeviceSpeedFull;
		}
		else
		{
			pEP->_speed = kUSBDeviceSpeedHigh;
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
			USBLog(5,"AppleUSBEHCI[%p]::UIMCreateIsochEndpoint high speed 2 size %d, mult %d: %d", this, (uint32_t)maxPacketSize, pEP->mult, pEP->oneMPS);
		}

		pEP->maxPacketSize = maxPacketSize;
		pEP->inSlot = kEHCIPeriodicListEntries+1;
		pEP->ttiPtr = ttiPtr;
	}

	err = AllocateIsochBandwidth(pEP, ttiPtr);

	if (err)
	{
		USBLog(1, "AppleUSBEHCI[%p]::UIMCreateIsochEndpoint - AllocateIsochBandwidth returned err(%p)", this, (void*)err);
		
		// Set the maxPacketSize to 0 if we fail, which indicates that the endpoint has zero bandwidth allocated
		pEP->maxPacketSize = 0;
		DeleteIsochEP(pEP);
	}
	
    return err;
}



IOReturn 
AppleUSBEHCI::AbortIsochEP(AppleEHCIIsochEndpoint* pEP)
{
    UInt32								slot;
    IOReturn							err;
    IOUSBControllerIsochListElement		*pTD;
    uint64_t							timeStamp;
    
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(6, "+AppleUSBEHCI[%p]::AbortIsochEP[%p] - start - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP,  _outSlot, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)_consumerCount, (uint32_t)_producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
	}
	
    USBLog(7, "AppleUSBEHCI[%p]::AbortIsochEP (%p)", this, pEP);
	
    // we need to make sure that the interrupt routine is not processing the periodic list
	_inAbortIsochEP = true;
	pEP->aborting = true;
	
    // DisablePeriodicSchedule();
	timeStamp = mach_absolute_time();
	
    // now make sure we finish any periodic processing we were already doing (for MP machines)
    while (_filterInterruptActive)
		;
	
    // now scavange any transactions which were already on the done queue, but don't put any new ones onto the scheduled queue, since we
    // are aborting
    err = scavengeIsocTransactions(NULL, false);
    if (err)
    {
		USBLog(1, "AppleUSBEHCI[%p]::AbortIsochEP - err (0x%x) from scavengeIsocTransactions", this, err);
		USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uintptr_t)this, err, 0, 1 );
    }
    
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(6, "+AppleUSBEHCI[%p]::AbortIsochEP[%p] - after scavenge - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP,  _outSlot, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue,(uint32_t) pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)_consumerCount, (uint32_t)_producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
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
			thing = GetPeriodicListLogicalEntry(slot);
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
							SetPeriodicListEntry(slot, nextThing);
							thing = NULL;																// to cause prevThing to remain unchanged (NULL) at the bottom of the loop
							if (nextThing == NULL)
							{
								if (!stopAdvancing)
								{
									USBLog(7, "AppleUSBEHCI[%p]::AbortIsochEP(%p) - advancing _outslot from 0x%x to 0x%x", this, pEP, _outSlot, (uint32_t)nextSlot);
									_outSlot = nextSlot;
								}
								else
								{
									USBLog(6, "AppleUSBEHCI[%p]::AbortIsochEP(%p) - would have advanced _outslot from 0x%x to 0x%x", this, pEP, _outSlot, (uint32_t)nextSlot);
								}
							}
                        }
                        
						// Note:  we will do an IOSleep() after we unlink everything so we don't need to wait here for the processor to be
						// done with the TD
                        
                        err = pTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);											// TODO - accumulate the return values or force abort err
						OSDecrementAtomic( &(pEP->scheduledTDs));
						if ( pEP->scheduledTDs < 0 )
						{
							USBLog(1, "AppleUSBEHCI[%p]::AbortIsochEP (%p) - scheduleTDs is negative! (%d)", this, pEP, (uint32_t)pEP->scheduledTDs);
							USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uintptr_t)this, (uintptr_t)pEP, (uint32_t)pEP->scheduledTDs, 2 );
						}
                        PutTDonDoneQueue(pEP, pTD, true	);
                    }
					else if (pTD->_pEndpoint == NULL)
					{
						USBLog(1, "AppleUSBEHCI[%p]::AbortIsochEP (%p) - NULL endpoint in pTD %p", this, pEP, pTD);
						USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uintptr_t)this, (uintptr_t)pEP, (uintptr_t)pTD, 3 );
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
		err = pTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);
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
		USBLog(1, "+AppleUSBEHCI[%p]::AbortIsochEP[%p] - done - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP,  _outSlot, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)_consumerCount, (uint32_t)_producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
		USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uintptr_t)pEP,  _outSlot, (uint32_t)pEP->inSlot, 4 );
		USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, (uintptr_t)pEP->toDoList, 5 );
		USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uint32_t)pEP->deferredTDs, (uintptr_t)pEP->deferredQueue, (uint32_t)pEP->scheduledTDs, 6 );
		USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uint32_t)pEP->onProducerQ, (uint32_t)_consumerCount, (uint32_t)_producerCount, 7 );
		USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, (uintptr_t)pEP->doneQueue, 8 );
	}
	else
	{
		USBLog(6, "-AppleUSBEHCI[%p]::AbortIsochEP[%p] - done - all clean - _outSlot (0x%x), pEP->inSlot (0x%x)", this, pEP, _outSlot, pEP->inSlot);
	}
    
	pEP->aborting = false;
    
    return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::DeleteIsochEP(AppleEHCIIsochEndpoint *pEP)
{
    IOUSBControllerIsochEndpoint* 		curEP, *prevEP;
    UInt32								currentMaxPacketSize;
	
    USBLog(7, "AppleUSBEHCI[%p]::DeleteIsochEP (%p)", this, pEP);
    if (pEP->activeTDs)
    {
		USBLog(6, "AppleUSBEHCI[%p]::DeleteIsochEP- there are still %d active TDs - aborting", this, (uint32_t)pEP->activeTDs);
		AbortIsochEP(pEP);
		if (pEP->activeTDs)
		{
			USBError(1, "AppleUSBEHCI[%p]::DeleteIsochEP- after abort there are STILL %d active TDs", this, (uint32_t) pEP->activeTDs);
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
	
	ReturnIsochBandwidth(pEP);
	DeallocateIsochEP(pEP);
	
    return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::CreateHSIsochTransfer(AppleEHCIIsochEndpoint * pEP, IOUSBIsocCommand * command)
{
    UInt32								bufferSize;
    AppleEHCIIsochTransferDescriptor 	*pNewITD = NULL;
    IOByteCount							transferOffset;
    UInt32								buffPtrSlot, transfer, frameNumberIncrease;
    unsigned							i,j;
    USBPhysicalAddress32							*buffP, *buffHighP;
    UInt32								*transactionPtr = 0;
    UInt32								*savedTransactionPtr = 0;
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
	UInt32								numberOfTDs = 0;
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
	
	USBLog(7,"AppleUSBEHCI[%p]::CreateHSIsochTransfer  transfersPerTD will be %d, frameNumberIncrease = %d", this, (uint32_t)transfersPerTD, (uint32_t)frameNumberIncrease);
	
	if ( frameNumberIncrease > 1 )
	{
		// Now that we have a frameNumberIncrease, set the epInterval to 8 so that our calculations are the same as 
		// if we had an interval of 8
		//
		epInterval = 8;		
	}
	
    // Format all the TDs, attach them to the pseudo endpoint. 
    // let the frame interrupt routine put them in the periodic list
	
	// We will set the IOC bit on the last TD of the transaction, unless this is a low latency request and the
	// updateFrequency is between 1 and 8.  0 and > 8 mean only update at the end of the transaction.
	
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
			
			if (lowLatency)
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
			USBTrace( kUSBTEHCI, kTPEHCICreateHSIsochTransfer, (uintptr_t)this, kIOReturnNoMemory, 0, 0);
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
			
			USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - Addr (0x%x) Length (%d) BufferSize (%d)", this, (uint32_t)dmaStartAddr, (uint32_t)segLen, (uint32_t)bufferSize);
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
			
			USBLog(7, "AppleUSBEHCI[%p]::CreateHSIochTransfer - getPhysicalSegment returned start of 0x%x; length:%d ; Buff Ptr0:%x", this, (uint32_t)dmaStartAddr, (uint32_t)segLen, *(buffP-1));
			
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
			
			USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - baseTransferIndex: %d, microFrame: %d, frameIndex: %d, interval %d", this, (uint32_t)baseTransferIndex, (uint32_t) transfer, (uint32_t)(baseTransferIndex + transfer), (uint32_t)epInterval);
			USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - forming transaction length (%d), pageOffset (0x%x), page (%d)", this, (uint32_t)trLen, (uint32_t)pageOffset, (uint32_t)page);
			
			*transactionPtr = HostToUSBLong(kEHCI_ITDStatus_Active |  (trLen<< kEHCI_ITDTr_LenPhase) | 
											(pageOffset << kEHCI_ITDTr_OffsetPhase) | (page << kEHCI_ITDTr_PagePhase) );
			
			// Advance to the next transacton element, which depends on the interval
			//
			savedTransactionPtr = transactionPtr;
			transactionPtr += epInterval;
			
			// Adjust the page and pageOffset for the next transaction
			pageOffset += trLen;
			if (pageOffset >= kEHCIPageSize)
			{
				pageOffset -= kEHCIPageSize;
				page++;
			}
		}
		
		// Now, check to see if we need to set the IOC bit for the last transaction in this TD.  At this point, we have
		// a TD with transfers for 1ms, so this is the granularity that we need for updateFrequency.
		//
		if (lowLatency && (updateFrequency > 0) && (updateFrequency <= 8 ))
		{
			if (numberOfTDs % updateFrequency == 0)
			{
				USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - Setting IOC bit on TD baseTransferIndex: %d, microFrame: %d, frameIndex: %d, interval %d", this, (uint32_t)baseTransferIndex, (uint32_t) transfer, (uint32_t)(baseTransferIndex + transfer), (uint32_t)epInterval);
				*savedTransactionPtr |= HostToUSBLong(kEHCI_ITDTr_IOC);
			}
		}
		
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
		numberOfTDs++;
    }
	
	// Add the completion action to the last TD
	//
	USBLog(7, "AppleUSBEHCI[%p]::CreateHSIsochTransfer - in TD (%p) setting _completion action (%p)", this, pNewITD, pNewITD->_completion.action);
	pNewITD->_completion = command->GetUSLCompletion();
	
	// Always set the IOC bit on the last transaction.
	if (savedTransactionPtr)
		*savedTransactionPtr |= HostToUSBLong(kEHCI_ITDTr_IOC);
	
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
	
	USBLog(7, "EHCI::CreateSplitIsochTransfer - pEP(%p) firstAvailableFrame(%d) frameNumberStart(%d) frameCount(%d)", pEP, (int)pEP->firstAvailableFrame, (int)frameNumberStart, (int)frameCount);
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
                USBLog(1,"AppleUSBEHCI[%p]::CreateSplitIsochTransfer - Isoch frame (%d) too big (%d) MPS (%d)", this, (uint32_t)(i + 1), pFrames[i].frReqCount, (uint32_t)pEP->maxPacketSize);
				USBTrace( kUSBTEHCI, kTPEHCICreateSplitIsochTransfer, (i + 1), pFrames[i].frReqCount, pEP->maxPacketSize, 1 );
                return kIOReturnBadArgument;
            }
            bufferSize += pFrames[i].frReqCount;
        } else
        {
            if (pLLFrames[i].frReqCount > kUSBMaxFSIsocEndpointReqCount)
            {
                USBLog(1,"AppleUSBEHCI[%p]::CreateSplitIsochTransfer(LL) - Isoch frame (%d) too big (%d) MPS (%d)", this, (uint32_t)(i + 1), pLLFrames[i].frReqCount, (uint32_t)pEP->maxPacketSize);
				USBTrace( kUSBTEHCI, kTPEHCICreateSplitIsochTransfer, (i + 1), pLLFrames[i].frReqCount, pEP->maxPacketSize, 2 );
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
			USBTrace( kUSBTEHCI, kTPEHCICreateSplitIsochTransfer, (uintptr_t)this, 0, kIOReturnNoMemory, 3 );
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
		if (segLen > reqCount)
		{
			segLen = reqCount;
		}
		if (segLen > (kEHCIPageSize-pageOffset))
		{
			segLen = kEHCIPageSize-pageOffset;
		}
		pNewSITD->GetSharedLogical()->buffPtr0 = HostToUSBLong(dmaStartAddr);
		pNewSITD->GetSharedLogical()->extBuffPtr0 = HostToUSBLong(dmaAddrHighBits);
		USBLog(7, "AppleUSBEHCI[%p]::CreateSplitIocTransfer - gen64IOVMSegments returned start of %p:%p; length %d", this, (void*)dmaAddrHighBits, (void*)dmaStartAddr, (uint32_t)segLen);
		
		transferOffset += segLen;
		bufferSize -= segLen;
        reqLeft = reqCount - segLen;
		
		if (reqLeft==0)
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
			if (segLen > reqLeft)
			{
				segLen = reqLeft;
			}
			if (segLen > kEHCIPageSize)
			{
				segLen = kEHCIPageSize;
			}
			USBLog(7, "AppleUSBEHCI[%p]::CreateSplitIocTransfer - gen64IOVMSegments returned start of %p:%p; length %d", this, (void*)dmaAddrHighBits, (void*)dmaStartAddr, (uint32_t)segLen);
			transferOffset += segLen;
			bufferSize -= segLen;
		}
		
        pNewSITD->_pFrames = pFrames;
        pNewSITD->_frameNumber = frameNumberStart + i;
		pNewSITD->_frameIndex = i;
		USBLog(7, "EHCI::CreateSplitIsochTransfer - SITD(%p) scheduled for frame(%d)", pNewSITD, (int)pNewSITD->_frameNumber);
        
		// NEW 11-15-04 SS and CS calculated by AllocateIsochBandwidth
		if (pEP->direction == kUSBOut)
		{
			// 7307654: 188 (kEHCIFSBytesPeruFrame) byte request should only be one SS packet
			UInt8		calcNumSS = ((reqCount-1) / kEHCIFSBytesPeruFrame) + 1;

			completeSplitFlags = 0;									// dont use complete split for OUT transactions
			startSplitFlags = pEP->pSPE->_SSflags;					// bitmask of bits to send SSplit on (created in HubInfo.cpp)
			
			if (calcNumSS > pEP->pSPE->_numSS)
			{
				USBLog(1, "AppleUSBEHCI[%p]::CreateSplitIsochTransfer - more SS needed than we have available! Error!", this);
				calcNumSS = pEP->pSPE->_numSS;
			}
			if (calcNumSS < pEP->pSPE->_numSS)
			{
				USBLog(7, "AppleUSBEHCI[%p]::CreateSplitIsochTransfer - not using all of the SS we had reserved. This is OK", this);
			}
			// we have start splits reserved, but we may not need all of them
			if (calcNumSS > 1)
			{
				transactionCount = calcNumSS;						// number of start splits
				transactionPosition = 1;							// beginning of a multi-part transfer
			}
			else
			{
				transactionCount = 1;								// only need one transfer
				transactionPosition = 0;							// total transfer is in this microframe
			}
		}
		else
		{
			// IN transactions
			startSplitFlags = pEP->pSPE->_SSflags;						// issue the SSplit on microframe 0
			transactionPosition = 0;									// only used for OUT
			transactionCount = 0;										// only used for OUT
			completeSplitFlags = pEP->pSPE->_CSflags;				// allow completes on 2,3,4,5,6,7
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
			{
				ioc = (((i+1) % 8) == 0) ? 1 : 0;
			}
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
			if (!(_errataBits & kErrataNoCSonSplitIsoch))				// MCP79 does NOT want to do this
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
		myStatFlags = kEHCIsiTDStatIOC | kEHCIsiTDStatStatusActive;

		if (!(_errataBits & kErrataNoCSonSplitIsoch))										// MCP79 does NOT want to do this
			myStatFlags |= kEHCUsiTDStatStatusSplitXState;
		
		pDummySITD->GetSharedLogical()->statFlags = HostToUSBLong(myStatFlags);
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
	 if (pEP == NULL)
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
	 
	 if (pEP == NULL)
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
	
	USBLog(7, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - adr=%d:%d IOMD=%p, frameStart: %qd, count: %d, pFrames: %p", this,  
		   command->GetAddress(), command->GetEndpoint(), 
		   command->GetBuffer(), 
		   command->GetStartFrame(), (uint32_t)command->GetNumFrames(), command->GetFrameList());
	
    if ( (command->GetNumFrames() == 0) || (command->GetNumFrames() > 1000) )
    {
        USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer bad frameCount: %d", this, (uint32_t)command->GetNumFrames());
        return kIOReturnBadArgument;
    }
	
    pEP = OSDynamicCast(AppleEHCIIsochEndpoint, FindIsochronousEndpoint(command->GetAddress(), command->GetEndpoint(), command->GetDirection(), NULL));
	
    if (pEP == NULL)
    {
        USBLog(1, "AppleUSBEHCI[%p]::UIMCreateIsochTransfer - Endpoint not found", this);
		USBTrace( kUSBTEHCI, kTPEHCICreateIsochTransfer, (uintptr_t)this, 0, 0, kIOUSBEndpointNotFound );
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
            USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer request frame WAY too old.  frameNumberStart: %d, curFrameNumber: %d.  Returning 0x%x", this, (uint32_t)command->GetStartFrame(), (uint32_t) curFrameNumber, kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
        USBLog(5,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %d, curFrameNumber: %d.  USBIsocFrame Ptr: %p, First ITD: %p", this, (uint32_t)command->GetStartFrame(), (uint32_t)curFrameNumber, command->GetFrameList(), pEP->toDoEnd);
    } 
	else 
    {					// frameNumberStart > curFrameNumber
        if (command->GetStartFrame() > (curFrameNumber + maxOffset))
        {
            USBLog(3,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer request frame too far ahead!  frameNumberStart: %d, curFrameNumber: %d", this, (uint32_t)command->GetStartFrame(), (uint32_t) curFrameNumber);
            return kIOReturnIsoTooNew;
        }
        frameDiff = command->GetStartFrame() - curFrameNumber;
        diff32 = (UInt32)frameDiff;
        if (diff32 < _istKeepAwayFrames)
        {
            USBLog(5,"AppleUSBEHCI[%p]::UIMCreateIsochTransfer WARNING! - frameNumberStart less than 2 ms (is %d)!  frameNumberStart: %d, curFrameNumber: %d", this, (uint32_t) diff32, (uint32_t)command->GetStartFrame(), (uint32_t) curFrameNumber);
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
    uint64_t									timeStamp;
	bool										fetchNewTDFromToDoList = true;
	bool										lostRegisterAccess = false;

    if (pEP->toDoList == NULL)
    {
		USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - no frames to add fn:%d EP:%d", this, pEP->functionAddress, pEP->endpointNumber);
		return;
    }
	
    if (pEP->aborting)
    {
		USBLog(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - EP (%p) is aborting - not adding", this, pEP);
		USBTrace( kUSBTEHCI, kTPEHCIAddIsocFramesToSchedule, (uintptr_t)this, pEP->functionAddress, pEP->endpointNumber, 1 );
		return;
    }
	
	// rdar://6693796 Test to see if the pEP is inconsistent at this point. If so, log a message..
	if ((pEP->doneQueue != NULL) && (pEP->doneEnd == NULL))
	{
		USBError(1, "AppleUSBEHCI::AddIsocFramesToSchedule - inconsistent EP queue. pEP[%p] doneQueue[%p] doneEnd[%p] doneQueue->_logicalNext[%p] onDoneQueue[%d]", pEP, pEP->doneQueue, pEP->doneEnd, pEP->doneQueue->_logicalNext, (int)pEP->onDoneQueue);
		IOSleep(1);			// to try to flush the syslog - time is of the essence here, though, but this is an error case
		// the inconsistency should be taken care of inside of the PutTDOnDoneQueue method
	}
	
	USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - scheduledTDs = %d, deferredTDs = %d", this, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->deferredTDs);
	
	if (_pEHCIRegisters->USBSTS == kEHCIInvalidRegisterValue)
	{
		USBLog(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - registers not accessible. Giving up", this);
		return;
	}
	
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
	timeStamp = mach_absolute_time();
    while(pEP->toDoList->_frameNumber <= (currFrame+_istKeepAwayFrames))		// Add keepaway, and use <= so you never put in a new frame 
																				// at less than 2 ahead of now. (EHCI spec, 7.2.1)
    {
		IOReturn	ret;
		UInt64		newCurrFrame;
		
		// this transaction is old before it began, move to done queue
		pTD = GetTDfromToDoList(pEP);
		//USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - ignoring TD(%p) because it is too old (%Lx) vs (%Lx) ", this, pTD, pTD->_frameNumber, currFrame);
		ret = pTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);		// TODO - accumulate the return values
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
			USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - calling thread_call_enter1(_returnIsochDoneQueueThread) scheduledTDs = %d, deferredTDs = %d", this, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->deferredTDs);
			bool alreadyQueued = thread_call_enter1(_returnIsochDoneQueueThread, (thread_call_param_t) pEP);
			if ( alreadyQueued )
			{
				USBLog(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - thread_call_enter1(_returnIsochDoneQueueThread) was NOT scheduled.  That's not good", this);
				USBTrace( kUSBTEHCI, kTPEHCIAddIsocFramesToSchedule, (uintptr_t)this, 0, 0, 2 );
			}
			return;
		}
		newCurrFrame = GetFrameNumber();
		if (newCurrFrame == 0)
		{
			lostRegisterAccess = true;						// we lost access to our registers
			break;
		}
		
		if (newCurrFrame > currFrame)
		{
			//USBLog(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - Current frame moved (0x%Lx->0x%Lx) resetting", this, currFrame, newCurrFrame);
			currFrame = newCurrFrame;
		}		
    }
    
	if (!lostRegisterAccess)
	{
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
			if ( nextSlot == _outSlot) 								// weve caught up with our tail
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
			
			if (currFrame == pTD->_frameNumber)
			{
				IOUSBControllerListElement					*linkAfter = NULL;
				IOUSBControllerIsochListElement				*prevIsochLE;				// could be split or HS
				AppleEHCISplitIsochTransferDescriptor		*prevSITD;
				
				if (_outSlot > kEHCIPeriodicListEntries)
				{
					_outSlot = firstOutSlot;
				}
				// Place TD in list
				
				//USBLog(5, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - linking TD (%p) with frame (0x%Lx) into slot (0x%x) - curr next log (%p) phys (%p)", this, pTD, pTD->_frameNumber, pEP->inSlot, _logicalPeriodicList[pEP->inSlot], (void *)USBToHostLong(_periodicList[pEP->inSlot]));
				//pTD->print(7);
				if (pEP->pSPE)
				{
					prevIsochLE = OSDynamicCast(IOUSBControllerIsochListElement, GetPeriodicListLogicalEntry(pEP->inSlot));
					while (prevIsochLE)
					{
						prevSITD = OSDynamicCast(AppleEHCISplitIsochTransferDescriptor, prevIsochLE);
						if (prevSITD)
						{
							AppleEHCIIsochEndpoint	*pPrevEP = OSDynamicCast(AppleEHCIIsochEndpoint, prevSITD->_pEndpoint);

							if (pPrevEP && pPrevEP->pSPE && (pPrevEP->pSPE->_myTT == pEP->pSPE->_myTT) && (pPrevEP->pSPE->_startTime < pEP->pSPE->_startTime))
								linkAfter = prevSITD;
						}
						prevIsochLE = OSDynamicCast(IOUSBControllerIsochListElement, prevIsochLE->_logicalNext);
					}
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
					pTD->SetPhysicalLink(GetPeriodicListPhysicalEntry(pEP->inSlot));
					pTD->_logicalNext = GetPeriodicListLogicalEntry(pEP->inSlot);
					SetPeriodicListEntry(pEP->inSlot, pTD);
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
	}
	
	// Unlock, reenable preemption, so we can log
	IOSimpleLockUnlock(_isochScheduleLock);
	if (!lostRegisterAccess && (finFrame - startFrame) > 1)
	{
		USBLog(1, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - end -  startFrame(0x%Lx) finFrame(0x%Lx)", this, startFrame, finFrame);
	}
	
	USBLog(7, "AppleUSBEHCI[%p]::AddIsocFramesToSchedule - finished,  currFrame: %Lx", this, GetFrameNumber() );
}



IOReturn 		
AppleUSBEHCI::UIMHubMaintenance(USBDeviceAddress highSpeedHub, UInt32 highSpeedPort, UInt32 command, UInt32 flags)
{
    AppleUSBEHCIHubInfo		*hiPtr;
	
    switch (command)
    {
		case kUSBHSHubCommandAddHub:
			USBLog(7, "AppleUSBEHCI[%p]::UIMHubMaintenance - adding hub %d with flags 0x%x", this, highSpeedHub, (uint32_t)flags);
			hiPtr = AppleUSBEHCIHubInfo::AddHubInfo(&_hsHubs, highSpeedHub, flags);
			if (!hiPtr)
				return kIOReturnNoMemory;
			USBLog(7, "AppleUSBEHCI[%p]::UIMHubMaintenance - done creating new hub (%p) for address (%d)", this, hiPtr, highSpeedHub);
			break;
			
		case kUSBHSHubCommandRemoveHub:
			USBLog(7, "AppleUSBEHCI[%p]::UIMHubMaintenance - deleting hub %d", this, highSpeedHub);
			AppleUSBEHCIHubInfo::DeleteHubInfo(&_hsHubs, highSpeedHub);			// remove hub info (and all tt info)
			break;
			
		default:
			return kIOReturnBadArgument;
    }
    return kIOReturnSuccess;
}
#define	kEHCIUIMScratchFirstActiveFrame	0

void 
AppleUSBEHCI::ReturnOneTransaction(EHCIGeneralTransferDescriptor 	*transaction,
								   AppleEHCIQueueHead				*pED,
								   AppleEHCIQueueHead				*pEDBack,
								   IOReturn							err,
								   Boolean							inactive)	// Is this endpoint already inactive, so don't inactivate it
{
	if(!inactive)
	{
		// make sure it is halted, since we should leave it linked
		HaltAsyncEndpoint(pED, pEDBack);
	}
	
    // USBLog(6, "ReturnOneTransaction Enter with transaction %p",transaction);
	
    while(transaction!= NULL)
    {
		if(transaction->callbackOnTD)
		{
			if (!transaction->multiXferTransaction)
			{
				USBLog(2, "AppleUSBEHCI[%p]::ReturnOneTransaction - found the end of a non-multi transaction(%p)!", this, transaction);
				transaction = transaction->pLogicalNext;
				break;
			}
			// this is a multi-TD transaction (control) - check to see if we are at the end of it
			else if (transaction->finalXferInTransaction)
			{
				USBLog(2, "AppleUSBEHCI[%p]::ReturnOneTransaction - found the end of a MULTI transaction(%p)!", this, transaction);
				transaction = transaction->pLogicalNext;
				break;
			}
			else
			{
				USBLog(2, "AppleUSBEHCI[%p]::ReturnOneTransaction - returning the non-end of a MULTI transaction(%p)!", this, transaction);
				// keep going around the loop - this is a multiXfer transaction and we haven't found the end yet
			}
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
		USBTrace( kUSBTEHCI, kTPEHCIReturnOneTransaction, (uintptr_t)this, err, 0, 0);
    }
    returnTransactions(pED, transaction, err, true);
}



UInt32 
AppleUSBEHCI::findBufferRemaining(AppleEHCIQueueHead *pED)
{
	UInt32 flags, bufferSizeRemaining;
	
	flags = USBToHostLong(pED->GetSharedLogical()->qTDFlags);
	
	bufferSizeRemaining = (flags & kEHCITDFlags_Bytes) >> kEHCITDFlags_BytesPhase;
	
	return(bufferSizeRemaining);
}



bool
AppleUSBEHCI::CheckEDListForTimeouts(AppleEHCIQueueHead *head)
{
    AppleEHCIQueueHead				*pED = head;
    AppleEHCIQueueHead				*pEDBack = NULL, *pEDBack1 = NULL;
    IOPhysicalAddress				pTDPhys;
    EHCIGeneralTransferDescriptor 	*pTD;
	EHCIQueueHeadShared				*pQH;
	
    UInt32 				noDataTimeout;
    UInt32				completionTimeout;
    UInt32				rem;
    UInt32				curFrame = GetFrameNumber32();
	
	if (curFrame == 0)
	{
		USBLog(2, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - curFrame is 0, not doing anything", this);
		return false;
	}
	
    for (; pED != 0; pED = (AppleEHCIQueueHead *)pED->_logicalNext)
    {
		USBLog(7, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - checking ED [%p]", this, pED);
		pED->print(7, this);
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
		
		// Find the QH
		pQH = pED->GetSharedLogical();
		// get the top TD
		pTDPhys = USBToHostLong(pQH->CurrqTDPtr) & kEHCIEDTDPtrMask;
		pTD = pED->_qTD;
		if (!pTD)
		{
			USBLog(7, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - no TD", this);
			continue;
		}

		if(head != _InactiveAsyncHead)
		{
			if( (pTD == pED->_TailTD)	&&// No TDs on this ED, may be inactive
				((pQH->qTDFlags & kEHCITDStatus_Active) == 0) )	// Its inactive
			{			
				//USBLog(7, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - ED [%p] not active", this, pED);
				if(pTDPhys == pED->_lastSeenTD)
				{
					//USBLog(7, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - ED [%p] still same not active TD: %lx", this, pED, (long)pTDPhys);
					// Still the same TD as last time, has it been here long enough.
					if ((curFrame - pED->_lastSeenFrame) >= 750)	// Lets make that slightly less than 1 second, will be caught by 2 timeout timers. Tunable number
					{
						// Trim queue head
						USBLog(5, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - found a QH (%lx) Inactive for long enough, trimming", this, (long)pED);
						unlinkAsyncEndpoint(pED, pEDBack);
						if( (pQH->qTDFlags & kEHCITDStatus_Active) != 0)	// Became active while unlinking
						{
							// This should never happen, but just in case
							USBError(1, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - pEDQueue: %p, became active while unlinking, let this be scavenged from the inactive queue", this, pED);
						}
						pED->_logicalNext = _InactiveAsyncHead;
						_InactiveAsyncHead = pED;
						return(true);	// Get the upper layer to restart this, I can't figure out how
					}
					else
					{
						//USBLog(6, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - found a QH (%lx) Inactive for: %ld", this, (long)pED, (long)curFrame - pED->_lastSeenFrame);
					}

				}
				else 
				{
					pED->_lastSeenTD = pTDPhys;	// don't time it out next time
					pED->_lastSeenFrame = curFrame;
				}
			}
			else 
			{
				pED->_lastSeenTD = 0;	// don't time it out next time
			}
		}
		
		if (!pTD->command)
		{
			USBLog(7, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - found a TD without a command - moving on", this);
			continue;
		}
		
		if (pTD == pED->_TailTD)
		{
			USBLog(1, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - ED (%p) - TD is TAIL but there is a command - pTD (%p)", this, pED, pTD);
			USBTrace( kUSBTEHCI, kTPEHCICheckEDListForTimeouts, (uintptr_t)this, (uintptr_t)pED, (uintptr_t)pTD, 0);
			pED->print(5, this);
		}
		
		if((pTDPhys != pTD->pPhysical) && !(pED->GetSharedLogical()->qTDFlags & USBToHostLong(kEHCITDStatus_Halted | kEHCITDStatus_Active)  ))
		{
			USBLog(6, "AppleUSBEHCI[%p]::CheckEDListForTimeouts - pED (%p) - mismatched logical and physical - TD (L:%p - P:%p) will be scavenged later", this, pED, pTD, (void*)pTD->pPhysical);
			pED->print(7, this);
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
				uint32_t	myFlags = USBToHostLong(pED->GetSharedLogical()->flags);
				
				USBLog(2, "AppleUSBEHCI[%p]::CheckEDListForTimeout - Found a TD [%p] on QH [%p] past the completion deadline, timing out! (0x%x - 0x%x)", this, pTD, pED, (uint32_t)curFrame, (uint32_t)firstActiveFrame);
				USBError(1, "AppleUSBEHCI[%p]::Found a transaction past the completion deadline on bus 0x%x, timing out! (Addr: %d, EP: %d)", this, (uint32_t) _busNumber, ((myFlags & kEHCIEDFlags_FA) >> kEHCIEDFlags_FAPhase), ((myFlags & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase) );
				pED->print(2, this);
				_UIMDiagnostics.timeouts++;
				ReturnOneTransaction(pTD, pED, pEDBack, kIOUSBTransactionTimeout, head == _InactiveAsyncHead);
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
			uint32_t	myFlags = USBToHostLong(pED->GetSharedLogical()->flags);
			
			USBLog(2, "AppleUSBEHCI[%p]CheckEDListForTimeout:  Found a transaction (%p) which hasn't moved in 5 seconds, timing out! (0x%x - 0x%x)", this, pTD, (uint32_t)curFrame, (uint32_t)pTD->lastFrame);
			USBError(1, "AppleUSBEHCI[%p]::Found a transaction which hasn't moved in 5 seconds on bus 0x%x, timing out! (Addr: %d, EP: %d)", this, (uint32_t) _busNumber, ((myFlags & kEHCIEDFlags_FA) >> kEHCIEDFlags_FAPhase), ((myFlags & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase) );
			_UIMDiagnostics.timeouts++;
			ReturnOneTransaction(pTD, pED, pEDBack, kIOUSBTransactionTimeout, head == _InactiveAsyncHead);
			
			continue;
		}
    }
	
	return(false);
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
	UInt32			retries = 0;
	
    // If we are not active anymore or if we're in ehciBusStateOff, then don't check for timeouts 
    //
    if ( isInactive() || !_controllerAvailable || (_myBusState != kUSBBusStateRunning) || _wakingFromHibernation)
	{
		if (_controlBulkTransactionsOut)
		{
			USBLog(1, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - aborting check with outstanding transactions! _myBusState(%d) _wakingFromHibernation(%s)", this, (int)_myBusState, _wakingFromHibernation ? "TRUE" : "FALSE");
			USBTrace( kUSBTEHCI, kTPEHCICheckForTimeouts, (uintptr_t)this, _myBusState, _wakingFromHibernation, 0);
		}
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
			USBLog(6, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Async USBCMD and USBSTS not synched ON (#%d)", this, _asynchScheduleUnsynchCount);
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
			USBLog(6, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Async USBCMD and USBSTS not synched OFF (#%d)", this, _asynchScheduleUnsynchCount);
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
			USBLog(6, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Periodic USBCMD and USBSTS not synched ON (#%d)", this, _periodicScheduleUnsynchCount);
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
			USBLog(6, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Periodic USBCMD and USBSTS not synched ON (#%d)", this, _periodicScheduleUnsynchCount);
			if (_periodicScheduleUnsynchCount >= 10)
			{
				USBError(1, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - Periodic USBCMD and USBSTS not synched ON (#%d)", this, _periodicScheduleUnsynchCount);
			}
		}
		else
			_periodicScheduleUnsynchCount = 0;
	}

    // Check to see if our control or bulk lists have a TD that has timed out
    while(CheckEDListForTimeouts(_AsyncHead))
	{
		if (retries++ > 100)
		{
			USBError(1, "AppleUSBEHCI[%p]::UIMCheckForTimeouts - exceeded 100 calls to CheckEDListForTimeouts on the AsyncHead!", this);
			break;
		}
	}
	
	// Even more important to do that in the inactive list, that's where you likely find them
    CheckEDListForTimeouts(_InactiveAsyncHead);
	
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
	printAsyncQueue(7, "+UIMEnableAddressEndpoints", true, false);
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
						maybeLinkAsyncEndpoint(pQH);
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

		// look throught the inactive list
		pQH = _InactiveAsyncHead;
		while (pQH)
		{
			if (pQH->_functionNumber == address)
			{
				USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAddressEndpoints- found matching QH[%p] with _queueType (%d) on inactive list", this, pQH, pQH->_queueType);
				if(_InactiveAsyncHead == pQH)
				{
					_InactiveAsyncHead = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
				}
				else
				{
					pPrevQH->_logicalNext = pQH->_logicalNext;
				}

				pQH->_logicalNext = _disabledQHList;
				_disabledQHList = pQH;
				pQH = pPrevQH;
			}
			pPrevQH = pQH;
			pQH = pQH ? OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext) : _InactiveAsyncHead;
		}
		
		pLEBack = NULL;
		
		for(i= 0; i < _greatestPeriod; i++)
		{
			pLE = GetPeriodicListLogicalEntry(i);
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
				pLE = pLE ? pLE->_logicalNext : GetPeriodicListLogicalEntry(i);
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
	printAsyncQueue(7, "-UIMEnableAddressEndpoints", true, false);
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
	printAsyncQueue(7, "+UIMEnableAllEndpoints", true, false);
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
					maybeLinkAsyncEndpoint(pQH);
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
		
		// look throught the inactive list
		pQH = _InactiveAsyncHead;
		while (pQH)
		{
			USBLog(5, "AppleUSBEHCI[%p]::UIMEnableAllEndpoints- found matching QH[%p] with _queueType (%d) on inactive list", this, pQH, pQH->_queueType);
			_InactiveAsyncHead = OSDynamicCast(AppleEHCIQueueHead, pQH->_logicalNext);
			pQH->_logicalNext = _disabledQHList;
			_disabledQHList = pQH;
			pQH = _InactiveAsyncHead;
		}
		
		pLEBack = NULL;
		
		for(i= 0; i < _greatestPeriod; i++)
		{
			GetPeriodicListLogicalEntry(i);
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
				pLE = GetPeriodicListLogicalEntry(i);
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
	printAsyncQueue(7, "-UIMEnableAllEndpoints", true, false);
	return kIOReturnSuccess;
}


// New Periodic Scheduling code August 2009
#pragma mark Periodic Scheduling

// Allocate needed bytes for Periodic Interrupt Transfers on a HS bus (Isoch is on another bus)
// This includes pure HS endpoints as well as the HS part of Split endpoints
IOReturn			
AppleUSBEHCI::AllocateInterruptBandwidth(AppleEHCIQueueHead	*pED, AppleUSBEHCITTInfo *pTT)
{
	int			frameNum, uFrameNum;
	int			effectiveFrame, effectiveuFrame;
	int			index;
	UInt8		startFrame = 0xFF;
	UInt8		startuFrame = 0xFF;						// this is unsigned, but the permanent one can be signed
	UInt16		minBandwidthUsed = 0xFFFF;
	bool		undoAllocation = false;
	IOReturn	err;
	UInt16		realPollingRate, realMPS, FSbytesNeeded, HSallocation;
	UInt32		splitFlags;
	
	USBLog(5, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - pED[%p] _speed(%d)", this, pED, (int)pED->_speed);

	// Set the adjusted polling rate before doing anything else
	if (pED->_speed == kUSBDeviceSpeedHigh)
	{
		// _pollingRate is measured in uFrames.. limit to 32 frames worth max
		if (pED->_pollingRate > (kEHCIMaxPollingInterval * kEHCIuFramesPerFrame))
			pED->_pollingRate = kEHCIMaxPollingInterval * kEHCIuFramesPerFrame;
	} 
	else 
	{
		// Full/Low speed device
		
		if (pED->_pollingRate > kEHCIMaxPollingInterval)
			realPollingRate = kEHCIMaxPollingInterval;
		else
		{
			// we need to convert this _pollingRate to the next lower power of 2
			int		count = 0;
			realPollingRate = pED->_pollingRate;
			while ((realPollingRate >> count) != 1) 
				count++;
			realPollingRate = (1 << count);
		}
		pED->_pollingRate = realPollingRate;
	}

	// Special case a zero MPS endpoint always succeeds
	if (pED->_maxPacketSize == 0)
	{
		USBLog(5, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - 0 MPS always succeeds!", this);
		
		// Default our start frames in this case
		pED->_startFrame = 0;
		pED->_startuFrame = 0;
		
		return kIOReturnSuccess;
	}
	
	ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "+AllocateInterruptBandwidth");

	if (pED->_speed == kUSBDeviceSpeedHigh)
	{
		// now calculate the needed bandwidth - this is all on the HS bus
		if (pED->_direction == kUSBIn)
		{
			HSallocation = kEHCIHSTokenChangeDirectionOverhead + kEHCIHSDataChangeDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime;
		}
		else
		{
			HSallocation = kEHCIHSTokenSameDirectionOverhead + kEHCIHSDataSameDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime;
		}
		
		HSallocation += ((pED->_maxPacketSize * 7) / 6);					// account for bit stuffing

		USBLog(5, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - pEP[%p] HSallocation[%d]", this, pED, HSallocation);
		
		// if _pollingRate is 1 (poll every uFrame.. highly unusual) then we will end up using [0][0] since we have to use every uFrame
		// if _pollingRate is 2 (poll every other uFrame) we could use[0][0] or [0][1], but it has to be one of those
		// if _polingRate is 8 (once per ms) then we could use any microframe in frame [0]
		// if _pollingRate is 64 (once every 8 ms) then we have 64 microframes to look at, etc.
		for (index = 0; index < pED->_pollingRate; index++)
		{
			if (_periodicBandwidthUsed[index / kEHCIuFramesPerFrame][index % kEHCIuFramesPerFrame] < minBandwidthUsed)
			{
				startFrame = index / kEHCIuFramesPerFrame;
				startuFrame = index % kEHCIuFramesPerFrame;
				minBandwidthUsed = _periodicBandwidthUsed[startFrame][startuFrame];
			}
		}
		if ((startFrame == 0xFF) || (startuFrame == 0xFF))
		{
			USBLog(1, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - could not find bandwidth", this);
			return kIOReturnNoBandwidth;
		}
		USBLog(3, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - using startFrame[%d] startuFrame[%d]", this, startFrame, startuFrame);
		pED->_startFrame = startFrame;
		pED->_startuFrame = startuFrame;

		for (index = (pED->_startFrame * kEHCIuFramesPerFrame) + pED->_startuFrame; 
			 index < (kEHCIMaxPollingInterval * kEHCIuFramesPerFrame);
			 index += pED->_pollingRate)
		{
			UInt16 frameNum = index / kEHCIuFramesPerFrame;
			UInt16 uFrameNum = index % kEHCIuFramesPerFrame;

			pED->GetSharedLogical()->splitFlags |= HostToUSBLong(1 << (uFrameNum));
			USBLog(5, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - adding %d bytes to frameNum(%d) uFrameNum(%d)", this, HSallocation, frameNum, uFrameNum);
			err = ReservePeriodicBandwidth(frameNum, uFrameNum, HSallocation);
			if (err == kIOReturnNoBandwidth)
			{
				USBLog(1, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - not enough bandwidth", this);
				undoAllocation = true;
			}
		}
		if (undoAllocation)
		{
			for (index = (pED->_startFrame * kEHCIuFramesPerFrame) + pED->_startuFrame; 
				 index < (kEHCIMaxPollingInterval * kEHCIuFramesPerFrame);
				 index += pED->_pollingRate)
			{
				UInt16 frameNum = index / kEHCIuFramesPerFrame;
				UInt16 uFrameNum = index % kEHCIuFramesPerFrame;

				USBLog(5, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - returning %d bytes to frameNum(%d) uFrameNum(%d)", this, HSallocation, frameNum, uFrameNum);
				ReleasePeriodicBandwidth(frameNum, uFrameNum, HSallocation);
			}
			
			// Report that we didn't get the bandwidth
			err = kIOReturnNoBandwidth;
		}
		else
		{
			// All is good, we got our desired bandwidth
			ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-AllocateInterruptBandwidth");
			err = kIOReturnSuccess;
		}
	}
	else 
	{
		// at this point we know that the endpoint is a Split Transaction endpoint, since regular FS and LS allocation doesn't come through the EHCI driver
		if (!pTT)
		{
			USBLog(1, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth.. no pTT, cannot proceed", this);
			err = kIOReturnBadArgument;
			goto ErrorExit;
		}
		
		pTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "+AllocateInterruptBandwidth");
		
		// start with the bus allocation on the FS bus
		if (pED->_speed == kUSBDeviceSpeedFull)
		{
			FSbytesNeeded = kEHCIFSSplitInterruptOverhead + pTT->_thinkTime + pED->_maxPacketSize;
		}
		else 
		{
			// LS devices are at 1/8 the speed..
			FSbytesNeeded = kEHCILSSplitInterruptOverhead + pTT->_thinkTime + (8 * pED->_maxPacketSize);
		}
		
		realMPS = (pED->_maxPacketSize * 7) / 6;
		
		pED->_pSPE = AppleUSBEHCISplitPeriodicEndpoint::NewSplitPeriodicEndpoint(pTT, kUSBInterrupt, pED, FSbytesNeeded, pED->_pollingRate);
		if (!pED->_pSPE)
		{
			USBLog (1, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - no SplitPeriodicEndpoint available", this);
			err = kIOReturnNoMemory;
			goto ErrorExit;
		}
		
		// this call gets the allocation needed on the Transaction Translator (the FS or LS parts)
		err = pTT->AllocatePeriodicBandwidth(pED->_pSPE);
		if (err)
		{
			USBLog (1, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - pTT->AllocatePeriodicBandwidth returned err(%p)", this, (void*)err);
			
			pED->_pSPE->release();
			pED->_pSPE = NULL;
			goto ErrorExit;
		}
		
		pED->_startFrame = pED->_pSPE->_startFrame;
		
		// now allocate the High Speed part of the bandwidth
		// Note:  Why don't we do something with this error?
		err = AllocateHSPeriodicSplitBandwidth(pED->_pSPE);
		
		splitFlags = USBToHostLong(pED->GetSharedLogical()->splitFlags);
		splitFlags &= ~(kEHCIEDSplitFlags_SMask + kEHCIEDSplitFlags_CMask);
		splitFlags |= (pED->_pSPE->_SSflags << kEHCIEDSplitFlags_SMaskPhase);
		splitFlags |= (pED->_pSPE->_CSflags << kEHCIEDSplitFlags_CMaskPhase);
		pED->GetSharedLogical()->splitFlags = HostToUSBLong(splitFlags);
		
		pTT->CalculateSPEsToAdjustAfterChange(pED->_pSPE, true);
		AdjustSPEs(pED->_pSPE, true);
		
		pTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "-AllocateInterruptBandwidth");
		ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-AllocateInterruptBandwidth");
		pTT->print(EHCISPLITTRANSFERLOGGING, "-AllocateInterruptBandwidth");
		
		err =  kIOReturnSuccess;
	}
	
ErrorExit:
	
	USBLog(5, "AppleUSBEHCI[%p]::AllocateInterruptBandwidth - returning 0x%x(%s)", this, err, USBStringFromReturn(err));

	return err;
}



IOReturn
AppleUSBEHCI::ReturnInterruptBandwidth(AppleEHCIQueueHead	*pED)
{
	int				index;
	IOReturn		err;
	
	USBLog(5, "AppleUSBEHCI[%p]::ReturnInterruptBandwidth - pED[%p] _speed(%d)", this, pED, (int)pED->_speed);

	if (pED->_maxPacketSize == 0)
	{
		if (pED->_pSPE)
		{
			USBLog(1, "AppleUSBEHCI[%p]::ReturnInterruptBandwidth - 0 MPS - unexpected _pSPE(%p) in pED(%p)", this, pED->_pSPE, pED);
		}
		USBLog(5, "AppleUSBEHCI[%p]::ReturnInterruptBandwidth - 0 MPS always succeeds (no bandwidth to return)!", this);

		return kIOReturnSuccess;
	}
	
	ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "+ReturnInterruptBandwidth");
	pED->print(EHCISPLITTRANSFERLOGGING, this);
	if (pED->_speed == kUSBDeviceSpeedHigh)
	{
		UInt16				HSallocation;
		
		if (pED->_direction == kUSBIn)
		{
			HSallocation = kEHCIHSTokenChangeDirectionOverhead + kEHCIHSDataChangeDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime;
		}
		else
		{
			HSallocation = kEHCIHSTokenSameDirectionOverhead + kEHCIHSDataSameDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime;
		}
		HSallocation += ((pED->_maxPacketSize * 7) / 6);					// account for bit stuffing

		for (index = (pED->_startFrame * kEHCIuFramesPerFrame) + pED->_startuFrame; 
			 index < (kEHCIMaxPollingInterval * kEHCIuFramesPerFrame);
			 index += pED->_pollingRate)
		{
			UInt16 frameNum = index / kEHCIuFramesPerFrame;
			UInt16 uFrameNum = index % kEHCIuFramesPerFrame;
			
			ReleasePeriodicBandwidth(frameNum, uFrameNum, HSallocation);
		}
		ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-ReturnInterruptBandwidth");
		return kIOReturnSuccess;
	}
	if (!pED->_pSPE)
	{
		USBLog (1, "AppleUSBEHCI[%p]::ReturnInterruptBandwidth - no SplitPeriodicEndpoint available", this);
		return kIOReturnInternalError;
	}
	
	if (!pED->_pSPE->_myTT)
	{
		USBLog (1, "AppleUSBEHCI[%p]::ReturnInterruptBandwidth - no TT available", this);
		return kIOReturnInternalError;
	}
	
	pED->_pSPE->_myTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "+ReturnInterruptBandwidth");
	err = pED->_pSPE->_myTT->DeallocatePeriodicBandwidth(pED->_pSPE);
	pED->_pSPE->_FSBytesUsed = 0;

	if (err)
	{
		USBLog (1, "AppleUSBEHCI[%p]::ReturnInterruptBandwidth - pTT->ReturnInterruptBandwidth returned err(%p)", this, (void*)err);
		return kIOReturnInternalError;
	}
	err = ReturnHSPeriodicSplitBandwidth(pED->_pSPE);
	if (err)
	{
		USBLog (1, "AppleUSBEHCI[%p]::ReturnInterruptBandwidth - pTT->ReturnInterruptBandwidth returned err(%p)", this, (void*)err);
		return kIOReturnInternalError;
	}

	pED->_pSPE->_myTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "-ReturnInterruptBandwidth");
	ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-ReturnInterruptBandwidth");
	pED->_pSPE->_myTT->print(EHCISPLITTRANSFERLOGGING, "-ReturnInterruptBandwidth");
	pED->_pSPE->release();
	pED->_pSPE = NULL;
	
	return err;
}



IOReturn
AppleUSBEHCI::AllocateIsochBandwidth(AppleEHCIIsochEndpoint	*pEP, AppleUSBEHCITTInfo *pTT)
{
	UInt16		minBandwidthUsed = 0xFFFF;
	UInt8		startFrame = 0xFF;
	UInt8		startuFrame = 0xFF;						// this is unsigned, but the permanent one can be signed
	bool		undoAllocation = false;
	UInt16		FSbytesNeeded, HSallocation;
	int			index;
	IOReturn	err;
	
	USBLog(5, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - pEP[%p] _speed(%d) maxPacketSize(%d)", this, pEP, (int)pEP->_speed, (int)pEP->maxPacketSize);
	
	// special case. a zero MPS endpoint always succeeds
	if (pEP->maxPacketSize == 0)
	{
		USBLog(5, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - 0 MPS always succeeds!", this);
		return kIOReturnSuccess;
	}
	
	ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "+AllocateIsochBandwidth");

	// High Speed allocation is the same as Interrupt, except pEP points to a different thing...
	if (pEP->_speed == kUSBDeviceSpeedHigh)
	{
		// _pollingRate is measured in uFrames.. limit to 32 frames worth max
		if (pEP->interval > (kEHCIMaxPollingInterval * kEHCIuFramesPerFrame))
			pEP->interval = kEHCIMaxPollingInterval * kEHCIuFramesPerFrame;
		
		// now calculate the needed bandwidth - this is all on the HS bus
		if (pEP->direction == kUSBIn)
		{
			HSallocation = kEHCIHSTokenChangeDirectionOverhead + kEHCIHSDataChangeDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime;
		}
		else
		{
			HSallocation = kEHCIHSTokenSameDirectionOverhead + kEHCIHSDataSameDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime;
		}
		HSallocation += ((pEP->maxPacketSize * 7) / 6);					// account for bit stuffing
		
		USBLog(6, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - pEP[%p] HSallocation[%d]", this, pEP, HSallocation);
		// if _pollingRate is 1 (poll every uFrame.. highly unusual) then we will end up using [0][0] since we have to use every uFrame
		// if _pollingRate is 2 (poll every other uFrame) we could use[0][0] or [0][1], but it has to be one of those
		// if _polingRate is 8 (once per ms) then we could use any microframe in frame [0]
		// if _pollingRate is 64 (once every 8 ms) then we have 64 microframes to look at, etc.
		for (index = 0; index < (int)pEP->interval; index++)
		{
			if (_periodicBandwidthUsed[index / kEHCIuFramesPerFrame][index % kEHCIuFramesPerFrame] < minBandwidthUsed)
			{
				startFrame = index / kEHCIuFramesPerFrame;
				startuFrame = index % kEHCIuFramesPerFrame;
				minBandwidthUsed = _periodicBandwidthUsed[startFrame][startuFrame];
			}
		}
		if ((startFrame == 0xFF) || (startuFrame == 0xFF))
		{
			USBLog(1, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - could not find bandwidth", this);
			return kIOReturnNoBandwidth;
		}
		USBLog(6, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - using startFrame[%d] startuFrame[%d]", this, startFrame, startuFrame);
		pEP->_startFrame = startFrame;
		pEP->_startuFrame = startuFrame;
		
		for (index = (pEP->_startFrame * kEHCIuFramesPerFrame) + pEP->_startuFrame; 
			 index < (kEHCIMaxPollingInterval * kEHCIuFramesPerFrame);
			 index += pEP->interval)
		{
			UInt16 frameNum = index / kEHCIuFramesPerFrame;
			UInt16 uFrameNum = index % kEHCIuFramesPerFrame;
			
			err = ReservePeriodicBandwidth(frameNum, uFrameNum, HSallocation);
			if (err == kIOReturnNoBandwidth)
			{
				USBLog(1, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - not enough bandwidth", this);
				undoAllocation = true;
			}
		}
		if (undoAllocation)
		{
			for (index = (pEP->_startFrame * kEHCIuFramesPerFrame) + pEP->_startuFrame; 
				 index < (kEHCIMaxPollingInterval * kEHCIuFramesPerFrame);
				 index += pEP->interval)
			{
				UInt16 frameNum = index / kEHCIuFramesPerFrame;
				UInt16 uFrameNum = index % kEHCIuFramesPerFrame;
				
				ReleasePeriodicBandwidth(frameNum, uFrameNum, HSallocation);
			}
			return kIOReturnNoBandwidth;
		}
		ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-AllocateIsochBandwidth");
		return kIOReturnSuccess;
	}
	
	// this is a FS Isoc endpoint (LS doesn't exist)
	
	// Allocation is done in two parts.. First it is allocated on the FS bus by calling into the TT object
	// Then we allocate on the HS bus in this routine
	
	// at this point we know that the endpoint is a Split Transaction endpoint, since regular FS and LS allocation doesn't come through the EHCI driver
	if (!pTT)
	{
		USBLog(1, "AppleUSBEHCI[%p]::AllocateIsochBandwidth.. no pTT, cannot proceed", this);
		return kIOReturnInternalError;		
	}
	
	pTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "+AllocateIsochBandwidth");
	// start with the bus allocation on the FS bus
	FSbytesNeeded = kEHCIFSSplitIsochOverhead + pTT->_thinkTime + pEP->maxPacketSize;
	
	if (pEP->interval != 1)
	{
		USBLog(2, "AppleUSBEHCI[%p]::AllocateIsochBandwidth.. interval != 1. unexpected. fixing.", this);
		pEP->interval = 1;
	}
	
	if (pEP->pSPE)
	{
		USBLog(5, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - reusing pSPE(%p)", this, pEP->pSPE);
		pEP->pSPE->_FSBytesUsed = FSbytesNeeded;
	}
	else 
	{
		
		USBLog(EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - FSBytesNeeded(%d) creating SPE", this, FSbytesNeeded); 

		pEP->pSPE = AppleUSBEHCISplitPeriodicEndpoint::NewSplitPeriodicEndpoint(pTT, kUSBIsoc, pEP, FSbytesNeeded, pEP->interval);
	}
	if (!pEP->pSPE)
	{
		USBLog (1, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - no SplitPeriodicEndpoint available", this);
		return kIOReturnInternalError;
	}
	
	// this call gets the allocation needed on the Transaction Translator (the FS or LS parts)
	err = pTT->AllocatePeriodicBandwidth(pEP->pSPE);
	if (err)
	{
		USBLog (3, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - pTT->AllocatePeriodicBandwidth returned err(%p)", this, (void*)err);
		
		pEP->pSPE->release();
		pEP->pSPE = NULL;
		return err;
	}
	
	pEP->_startFrame = pEP->pSPE->_startFrame;
	err = AllocateHSPeriodicSplitBandwidth(pEP->pSPE);
	
	pTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "-AllocateIsochBandwidth");
	ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-AllocateIsochBandwidth");

	pTT->CalculateSPEsToAdjustAfterChange(pEP->pSPE, true);
	AdjustSPEs(pEP->pSPE, true);

	
	USBLog(6, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - returning kIOReturnSuccess!", this);
	pEP->print(EHCISPLITTRANSFERLOGGING);
	pTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "-AllocateIsochBandwidth");
	ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-AllocateIsochBandwidth");
	pTT->print(EHCISPLITTRANSFERLOGGING, "-AllocateIsochBandwidth");
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::ReturnIsochBandwidth(AppleEHCIIsochEndpoint *pEP)
{
	int			index;
	IOReturn	err;
	UInt16		HSallocation;
	
	USBLog(5, "AppleUSBEHCI[%p]::ReturnIsochBandwidth - pEP[%p] _speed(%d)", this, pEP, (int)pEP->_speed);
	
	if (pEP->maxPacketSize == 0)
	{
		if (pEP->pSPE)
		{
			USBLog(1, "AppleUSBEHCI[%p]::ReturnIsochBandwidth - 0 MPS - unexpected pSPE(%p) in pEP(%p)", this, pEP->pSPE, pEP);
		}
		USBLog(5, "AppleUSBEHCI[%p]::ReturnIsochBandwidth - 0 MPS always succeeds (no bandwidth to return)!", this);
		return kIOReturnSuccess;
	}
	

	ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "+ReturnIsochBandwidth");
	if (pEP->_speed == kUSBDeviceSpeedHigh)
	{
		// now calculate the used bandwidth - this is all on the HS bus
		if (pEP->direction == kUSBIn)
		{
			HSallocation = kEHCIHSTokenChangeDirectionOverhead + kEHCIHSDataChangeDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime;
		}
		else
		{
			HSallocation = kEHCIHSTokenSameDirectionOverhead + kEHCIHSDataSameDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime;
		}
		HSallocation += ((pEP->maxPacketSize * 7) / 6);					// account for bit stuffing
		for (index = (pEP->_startFrame * kEHCIuFramesPerFrame) + pEP->_startuFrame; 
			 index < (kEHCIMaxPollingInterval * kEHCIuFramesPerFrame);
			 index += pEP->interval)
		{
			UInt16 frameNum = index / kEHCIuFramesPerFrame;
			UInt16 uFrameNum = index % kEHCIuFramesPerFrame;
			
			ReleasePeriodicBandwidth(frameNum, uFrameNum, HSallocation);
		}
		ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-ReturnIsochBandwidth");
		return kIOReturnSuccess;
	}
	
	// Split transaction case
	if (!pEP->pSPE)
	{
		USBLog (1, "AppleUSBEHCI[%p]::ReturnIsochBandwidth - no SplitPeriodicEndpoint available", this);
		return kIOReturnInternalError;
	}
	
	if (!pEP->pSPE->_myTT)
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReturnIsochBandwidth.. no pTT, cannot proceed", this);
		return kIOReturnInternalError;		
	}
	
	pEP->pSPE->_myTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "+ReturnIsochBandwidth");
	// this call gets the allocation needed on the Transaction Translator (the FS or LS parts)
	err = pEP->pSPE->_myTT->DeallocatePeriodicBandwidth(pEP->pSPE);
	pEP->pSPE->_FSBytesUsed = 0;
	if (err)
	{
		USBLog (1, "AppleUSBEHCI[%p]::ReturnIsochBandwidth - pTT->DeallocatePeriodicBandwidth returned err(%p)", this, (void*)err);
		return err;
	}
	ReturnHSPeriodicSplitBandwidth(pEP->pSPE);
	pEP->pSPE->_myTT->CalculateSPEsToAdjustAfterChange(pEP->pSPE, false);
	AdjustSPEs(pEP->pSPE, false);
	pEP->pSPE->_myTT->ShowHSSplitTimeUsed(EHCISPLITTRANSFERLOGGING, "-ReturnIsochBandwidth");
	ShowPeriodicBandwidthUsed(EHCISPLITTRANSFERLOGGING, "-ReturnIsochBandwidth");
	pEP->pSPE->_myTT->print(EHCISPLITTRANSFERLOGGING, "-ReturnIsochBandwidth");
	pEP->pSPE->release();
	pEP->pSPE = NULL;
	USBLog(5, "AppleUSBEHCI[%p]::ReturnIsochBandwidth - returning kIOReturnSuccess!", this);
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::AllocateHSPeriodicSplitBandwidth(AppleUSBEHCISplitPeriodicEndpoint *pSPE)
{
	int			frameNum, uFrameNum;
	int			effectiveFrame, effectiveuFrame;
	int			index;
	SInt8		startuFrame;
	UInt8		lastCSuFrame;							// last uFrame needed for a CS
	UInt16		SSoverhead, CSoverhead;					// number of bytes of overhead for each of these
	bool		undoAllocation = false;
	UInt16		realMPS;
	IOReturn	err;
	
	// now allocate the High Speed part of the bandwidth
	startuFrame = (pSPE->_startTime / kEHCIFSBytesPeruFrame) - 1;				// this could be negative

	pSPE->_SSflags = 0;
	pSPE->_CSflags = 0;
	
	if (pSPE->_direction == kUSBOut)
	{
		// OUT endpoints
		SSoverhead = kEHCIHSSplitSameDirectionOverhead + kEHCIHSDataSameDirectionOverhead + _controllerThinkTime;
		CSoverhead = (pSPE->_epType == kUSBInterrupt) ? kEHCIHSSplitChangeDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime : 0;
	}
	else
	{
		// IN endpoints
		SSoverhead = kEHCIHSSplitSameDirectionOverhead + _controllerThinkTime;
		CSoverhead = kEHCIHSSplitChangeDirectionOverhead + kEHCIHSDataChangeDirectionOverhead + _controllerThinkTime;
	}
	
	if (pSPE->_epType == kUSBIsoc)
	{
		if (pSPE->_direction == kUSBOut)
		{
			// Isoch OUT
			pSPE->_numCS = 0;
			// 7307654: 188 (kEHCIFSBytesPeruFrame) byte MPS should only be one SS packet
			pSPE->_numSS = ((pSPE->_isochEP->maxPacketSize - 1) / kEHCIFSBytesPeruFrame) + 1;
		}
		else
		{
			// Isoch IN
			lastCSuFrame = ((pSPE->_startTime + pSPE->_FSBytesUsed) / kEHCIFSBytesPeruFrame) + 1;
			pSPE->_numSS = 1;
			pSPE->_numCS = lastCSuFrame - (startuFrame + 1);
			if (lastCSuFrame <=6)
			{
				if ((startuFrame + 1) == 0)
					pSPE->_numCS++;
				else
					pSPE->_numCS += 2;
			}
			else if (lastCSuFrame == 7)
			{
				if ((startuFrame + 1) != 0)
					pSPE->_numCS++;
			}
		}
		realMPS = (pSPE->_isochEP->maxPacketSize * 7) / 6;
	}
	else
	{
		// Interrupt IN and OUT
		pSPE->_numSS = 1;
		if (startuFrame < 5)																// bus uFrame 4 or less, host uFrame 5 or less
			pSPE->_numCS = 3;
		else
			pSPE->_numCS = 2;
		
		realMPS = (pSPE->_intEP->_maxPacketSize * 7) / 6;

	}
	
	// allocate the SS bandwidth
	undoAllocation = false;
	// FS _polling rate is in ms
	USBLog (EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - calculating the frames - SSoverhead(%d) CSoverhead(%d)", this, SSoverhead, CSoverhead);
	for (frameNum = pSPE->_startFrame; frameNum < kEHCIMaxPollingInterval; frameNum += pSPE->_period)
	{
		int			hostuFrame;
		
		// allocate the SS tokens and any OUT data
		for (index = 0, uFrameNum = startuFrame; uFrameNum < startuFrame + pSPE->_numSS; uFrameNum++, index++)
		{
			UInt16			bytesToAllocate = 0;
			
			if (pSPE->_direction == kUSBOut)
			{
				bytesToAllocate = realMPS - (kEHCIFSBytesPeruFrame * index);
			}
			
			if (bytesToAllocate > kEHCIFSBytesPeruFrame)
				bytesToAllocate = kEHCIFSBytesPeruFrame;
			
			hostuFrame = uFrameNum + 1;
			
			effectiveFrame = frameNum;
			effectiveuFrame = uFrameNum;
			
			if (uFrameNum < 0)
			{
				effectiveFrame = (frameNum > 0) ? frameNum - 1 : kEHCIMaxPollingInterval - 1;
				effectiveuFrame = kEHCIuFramesPerFrame - 1;
			}
			else if (uFrameNum >= kEHCIuFramesPerFrame)
			{
				effectiveFrame = (frameNum + 1) % kEHCIMaxPollingInterval;
				effectiveuFrame = uFrameNum % kEHCIuFramesPerFrame;
			}
			
			USBLog (EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - SS index(%d) uFrameNum(%d) bytesToAllocate(%d) effectiveFrame(%d) effectiveuFrame(%d)", this, index, uFrameNum, bytesToAllocate, effectiveFrame, effectiveuFrame);
			if (hostuFrame >= kEHCIuFramesPerFrame)
			{
				// this should never happen
				USBLog(1, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - SS can't go into the next frame", this);
			}
			
			if (frameNum == pSPE->_startFrame)
			{
				// add this up, but only for the first harmonic
				USBLog (EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - setting a SS in host uFrame(%d)", this, hostuFrame);
				pSPE->_SSflags |= (1 << hostuFrame);
			}

			err = ReservePeriodicBandwidth(effectiveFrame, effectiveuFrame, SSoverhead+bytesToAllocate);
			if (err == kIOReturnNoBandwidth)
			{
				USBLog(5, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - not enough bandwidth", this);
				undoAllocation = true;
			}			
		}
		// allocate the CS tokens and the IN data bandwidth
		for (uFrameNum = (startuFrame + pSPE->_numSS + 1); uFrameNum < (startuFrame + pSPE->_numSS + 1 + pSPE->_numCS); uFrameNum++)
		{
			hostuFrame = uFrameNum + 1;
			
			effectiveFrame = frameNum;
			effectiveuFrame = uFrameNum;
			
			if (uFrameNum < 0)
			{
				effectiveFrame = (frameNum > 0) ? frameNum - 1 : kEHCIMaxPollingInterval - 1;
				effectiveuFrame = kEHCIuFramesPerFrame - 1;
			}
			else if (uFrameNum >= kEHCIuFramesPerFrame)
			{
				effectiveFrame = (frameNum + 1) % kEHCIMaxPollingInterval;
				effectiveuFrame = uFrameNum % kEHCIuFramesPerFrame;
			}
			
			USBLog (EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - CS index(%d) uFrameNum(%d) effectiveFrame(%d) effectiveuFrame(%d)", this, index, uFrameNum, effectiveFrame, effectiveuFrame);
			
			if (frameNum == pSPE->_startFrame)
			{
				if (hostuFrame >= kEHCIuFramesPerFrame)
				{
					// this is the back link case
					pSPE->_CSflags |= (1 << (hostuFrame % kEHCIuFramesPerFrame));
					pSPE->_wraparound = true;
				}
				else
				{
					USBLog (EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - setting a CS in host uFrame(%d)", this, hostuFrame);
					pSPE->_CSflags |= (1 << hostuFrame);
				}
			}
			
			err = ReservePeriodicBandwidth(effectiveFrame, effectiveuFrame, CSoverhead);
			
			// now account for the HS part of the actual data bytes, limiting it to the TT FS frame size
			if (pSPE->_myTT->_HSSplitINBytesUsed[effectiveFrame][effectiveuFrame] < kEHCIFSBytesPeruFrame)
			{
				UInt16			spaceAvailable = kEHCIFSBytesPeruFrame - pSPE->_myTT->_HSSplitINBytesUsed[effectiveFrame][effectiveuFrame];
				UInt16			bytesToAllocate = realMPS;
				
				// it is OK to go "over" on the HS side, so we will run it up to the MAX when that happens
				if (spaceAvailable < bytesToAllocate)
					bytesToAllocate = spaceAvailable;
				
				err = ReservePeriodicBandwidth(effectiveFrame, effectiveuFrame, bytesToAllocate);
			}
			if (realMPS > kEHCIFSBytesPeruFrame)
			{
				pSPE->_myTT->ReserveHSSplitINBytes(effectiveFrame, effectiveuFrame, kEHCIFSBytesPeruFrame);
			}
			else
			{
				pSPE->_myTT->ReserveHSSplitINBytes(effectiveFrame, effectiveuFrame, realMPS);
			}
			if (err == kIOReturnNoBandwidth)
			{
				USBLog(2, "AppleUSBEHCI[%p]::AllocateIsochBandwidth - not enough bandwidth", this);
				undoAllocation = true;
				// TODO: Deal with this error case!
			}			
		}
	}
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::ReturnHSPeriodicSplitBandwidth(AppleUSBEHCISplitPeriodicEndpoint *pSPE)
{
	UInt8		lastCSuFrame;							// last uFrame needed for a CS
	UInt16		bytesToReturn;
	UInt16		SSoverhead, CSoverhead;					// number of bytes of overhead for each of these
	int			frameNum, uFrameNum;
	int			effectiveFrame, effectiveuFrame;
	int			hostuFrame, index;
	SInt8		startuFrame;							// starting uFrame, calculated from _startTime
	UInt16		realMPS;
	IOReturn	err;
	
	if (pSPE->_direction == kUSBOut)
	{
		// OUT endpoints
		SSoverhead = kEHCIHSSplitSameDirectionOverhead + kEHCIHSDataSameDirectionOverhead + _controllerThinkTime;
		CSoverhead = (pSPE->_epType == kUSBInterrupt) ? kEHCIHSSplitChangeDirectionOverhead + kEHCIHSHandshakeOverhead + _controllerThinkTime : 0;
	}
	else
	{
		// IN endpoints
		SSoverhead = kEHCIHSSplitSameDirectionOverhead + _controllerThinkTime;
		CSoverhead = kEHCIHSSplitChangeDirectionOverhead + kEHCIHSDataChangeDirectionOverhead + _controllerThinkTime;
	}

	if (pSPE->_epType == kUSBIsoc)
	{
		realMPS = (pSPE->_isochEP->maxPacketSize * 7) / 6;
	}
	else
	{
		realMPS = (pSPE->_intEP->_maxPacketSize * 7) / 6;
	}
	
	
	if (!realMPS)
	{
		USBLog(5, "AppleUSBEHCI[%p]::ReturnHSPeriodicSplitBandwidth - pSPE[%p] has no maxPacketSize - nothing to do..", this, pSPE);
		return kIOReturnSuccess;
	}
	
	// Now return all of the HS bytes previously allocated
	lastCSuFrame = ((pSPE->_startTime + pSPE->_FSBytesUsed) / kEHCIFSBytesPeruFrame) + 1;
	
	
	startuFrame = (pSPE->_startTime / kEHCIFSBytesPeruFrame) - 1;				// this could be negative

	for (frameNum = pSPE->_startFrame; frameNum < kEHCIMaxPollingInterval; frameNum += pSPE->_period)
	{
		for (index = 0, uFrameNum = startuFrame; uFrameNum < startuFrame + pSPE->_numSS; uFrameNum++, index++)
		{
			UInt16			bytesAllocated = 0;
			
			if (pSPE->_direction == kUSBOut)
			{
				bytesAllocated = realMPS - (kEHCIFSBytesPeruFrame * index);
			}
			
			if (bytesAllocated > kEHCIFSBytesPeruFrame)
				bytesAllocated = kEHCIFSBytesPeruFrame;
			
			hostuFrame = uFrameNum + 1;
			
			effectiveFrame = frameNum;
			effectiveuFrame = uFrameNum;
			
			if (uFrameNum < 0)
			{
				effectiveFrame = (frameNum > 0) ? frameNum - 1 : kEHCIMaxPollingInterval - 1;
				effectiveuFrame = kEHCIuFramesPerFrame - 1;
			}
			else if (uFrameNum >= kEHCIuFramesPerFrame)
			{
				effectiveFrame = (frameNum + 1) % kEHCIMaxPollingInterval;
				effectiveuFrame = uFrameNum % kEHCIuFramesPerFrame;
			}
			
			USBLog (EHCISPLITTRANSFERLOGGING, "AppleUSBEHCI[%p]::ReturnHSPeriodicSplitBandwidth - SS index(%d) uFrameNum(%d) bytesAllocated(%d) effectiveFrame(%d) effectiveuFrame(%d)", this, index, uFrameNum, bytesAllocated, effectiveFrame, effectiveuFrame);
			
			ReleasePeriodicBandwidth(effectiveFrame, effectiveuFrame, SSoverhead + bytesAllocated);
		}
		for (uFrameNum = (startuFrame + pSPE->_numSS + 1); uFrameNum < (startuFrame + pSPE->_numSS + 1 + pSPE->_numCS); uFrameNum++)
		{
			hostuFrame = uFrameNum + 1;
			
			effectiveFrame = frameNum;
			effectiveuFrame = uFrameNum;
			
			if (uFrameNum < 0)
			{
				effectiveFrame = (frameNum > 0) ? frameNum - 1 : kEHCIMaxPollingInterval - 1;
				effectiveuFrame = kEHCIuFramesPerFrame - 1;
			}
			else if (uFrameNum >= kEHCIuFramesPerFrame)
			{
				effectiveFrame = (frameNum + 1) % kEHCIMaxPollingInterval;
				effectiveuFrame = uFrameNum % kEHCIuFramesPerFrame;
			}
			
			ReleasePeriodicBandwidth(effectiveFrame, effectiveuFrame, CSoverhead);
			
			bytesToReturn = realMPS;
			if (bytesToReturn > kEHCIFSBytesPeruFrame)
				bytesToReturn = kEHCIFSBytesPeruFrame;
			
			pSPE->_myTT->ReleaseHSSplitINBytes(effectiveFrame, effectiveuFrame, bytesToReturn);
			
			if (pSPE->_myTT->_HSSplitINBytesUsed[effectiveFrame][effectiveuFrame] < kEHCIFSBytesPeruFrame)
			{
				// these are the bytes to return to the overall bandwidth
				bytesToReturn = kEHCIFSBytesPeruFrame - pSPE->_myTT->_HSSplitINBytesUsed[effectiveFrame][effectiveuFrame];

				if (realMPS < bytesToReturn)
					bytesToReturn = realMPS;

				ReleasePeriodicBandwidth(effectiveFrame, effectiveuFrame, bytesToReturn);
			}
			
		}	
	}
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::AdjustSPEs(AppleUSBEHCISplitPeriodicEndpoint *pSPEChanged, bool added)
{
	AppleUSBEHCISplitPeriodicEndpoint		*pSPE;
	AppleUSBEHCITTInfo						*pTT = pSPEChanged ? pSPEChanged->_myTT : NULL;
	
	if (!pTT)
	{
		USBLog(1, "AppleUSBEHCI[%p]::AdjustSPEs - no TT - can't do anything!", this);
		return kIOReturnInternalError;
	}
	
	while (pTT->_pSPEsToAdjust->getCount())
	{
		UInt16			newStartTime;
		
		pSPE = (AppleUSBEHCISplitPeriodicEndpoint*)pTT->_pSPEsToAdjust->getFirstObject();
		pSPE->print(EHCISPLITTRANSFERLOGGING);
		pTT->_pSPEsToAdjust->removeObject(pSPE);
		newStartTime = pSPE->CalculateNewStartTimeFromChange(pSPEChanged);
		USBLog(5, "AppleUSBEHCI[%p]::AdjustSPEs - candidate(%p) newStartTime(%d)", this, pSPE, newStartTime);
		if (added)
		{
			// the SPE should not have moved to an earlier start time, although it may not have moved at all
			if (newStartTime < pSPE->_startTime)
			{
				USBLog(2, "AppleUSBEHCI[%p]::AdjustSPEs - newStartTime(%d) should be >= old _startTime (%d)", this, newStartTime, pSPE->_startTime);
			}
		}
		else
		{
			// the SPE should not have moved to a later start time, although it may not have moved at all
			if (newStartTime > pSPE->_startTime)
			{
				USBLog(1, "AppleUSBEHCI[%p]::AdjustSPEs - newStartTime(%d) should be <= old _startTime (%d)", this, newStartTime, pSPE->_startTime);
			}
		}
		ReturnHSPeriodicSplitBandwidth(pSPE);
		pSPE->_startTime = newStartTime;
		pSPE->_SSflags = 0;
		pSPE->_CSflags = 0;
		AllocateHSPeriodicSplitBandwidth(pSPE);
		if (pSPE->_epType == kUSBInterrupt)
		{
			UInt32	splitFlags;
			
			splitFlags = USBToHostLong(pSPE->_intEP->GetSharedLogical()->splitFlags);
			splitFlags &= ~(kEHCIEDSplitFlags_SMask + kEHCIEDSplitFlags_CMask);
			splitFlags |= (pSPE->_SSflags << kEHCIEDSplitFlags_SMaskPhase);
			splitFlags |= (pSPE->_CSflags << kEHCIEDSplitFlags_CMaskPhase);
			pSPE->_intEP->GetSharedLogical()->splitFlags = HostToUSBLong(splitFlags);
		}
		USBLog(5, "AppleUSBEHCI[%p]::AdjustSPEs - done with pSPE(%p)", this, pSPE);
		pSPE->print(EHCISPLITTRANSFERLOGGING);
	}
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::ReservePeriodicBandwidth(int frame, int uFrame, UInt16 bandwidth)
{
	
	// validate each of the params
	if (!bandwidth)
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReservePeriodicBandwidth - 0 bandwidth - nothing to do", this);
		return kIOReturnSuccess;
	}
	
	if (bandwidth > kEHCIHSMaxPeriodicBytesPeruFrame)
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReservePeriodicBandwidth - ERROR: bandwidth(%d) too big", this, (int)bandwidth);
		return kIOReturnBadArgument;
	}
	
	if ((frame < 0) || (frame > kEHCIMaxPollingInterval))
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReservePeriodicBandwidth - ERROR: invalid frame(%d)", this, frame);
		return kIOReturnBadArgument;
	}
	if ((uFrame < 0) || (uFrame > kEHCIuFramesPerFrame))
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReservePeriodicBandwidth - ERROR: invalid uFrame(%d)", this, uFrame);
		return kIOReturnBadArgument;
	}
	
	_periodicBandwidthUsed[frame][uFrame] += bandwidth;
	if (_periodicBandwidthUsed[frame][uFrame] > kEHCIHSMaxPeriodicBytesPeruFrame)
	{
		USBLog(5, "AppleUSBEHCI[%p]::ReservePeriodicBandwidth - frame[%d] uFrame[%d] reserved space(%d) over the limit - could be OK", this, frame, uFrame, _periodicBandwidthUsed[frame][uFrame]);
		return kIOReturnNoBandwidth;
	}
	
	USBLog(7, "AppleUSBEHCI[%p]::ReservePeriodicBandwidth - frame[%d] uFrame[%d] reserved bandwidth(%d)", this, frame, uFrame, _periodicBandwidthUsed[frame][uFrame]);
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::ReleasePeriodicBandwidth(int frame, int uFrame, UInt16 bandwidth)
{
	// validate each of the params
	if (!bandwidth)
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReleasePeriodicBandwidth - 0 bandwidth - nothing to do", this);
		return kIOReturnSuccess;
	}
	
	if (bandwidth > kEHCIHSMaxPeriodicBytesPeruFrame)
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReleasePeriodicBandwidth - ERROR: bandwidth(%d) too big", this, (int)bandwidth);
		return kIOReturnBadArgument;
	}
	
	if ((frame < 0) || (frame > kEHCIMaxPollingInterval))
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReleasePeriodicBandwidth - ERROR: invalid frame(%d)", this, frame);
		return kIOReturnBadArgument;
	}
	if ((uFrame < 0) || (uFrame > kEHCIuFramesPerFrame))
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReleasePeriodicBandwidth - ERROR: invalid uFrame(%d)", this, uFrame);
		return kIOReturnBadArgument;
	}
	
	if (bandwidth > _periodicBandwidthUsed[frame][uFrame])
	{
		USBLog(1, "AppleUSBEHCI[%p]::ReleasePeriodicBandwidth - ERROR: trying to release(%d) more than you have(%d) - i will take it to zero", this, (int)bandwidth, _periodicBandwidthUsed[frame][uFrame]);
		_periodicBandwidthUsed[frame][uFrame] = 0;
		return kIOReturnBadArgument;
	}

	_periodicBandwidthUsed[frame][uFrame] -= bandwidth;
	
	USBLog(7, "AppleUSBEHCI[%p]::ReleasePeriodicBandwidth - frame[%d] uFrame[%d] reserved bandwidth(%d)", this, frame, uFrame, _periodicBandwidthUsed[frame][uFrame]);
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCI::ShowPeriodicBandwidthUsed(int level, const char *fromStr)
{
	int		frame;
	
	USBLog(level, "AppleUSBEHCI[%p]::ShowPeriodicBandwidthUsed called from %s", this, fromStr);
	for (frame = 0; frame < kEHCIMaxPollingInterval; frame++)
		USBLog(level, "AppleUSBEHCI[%p]::ShowPeriodicBandwidthUsed - Frame %2.2d: [%3.3d] [%3.3d] [%3.3d] [%3.3d] [%3.3d] [%3.3d] [%3.3d] [%3.3d]",
			   this, frame,
			   _periodicBandwidthUsed[frame][0],
			   _periodicBandwidthUsed[frame][1],
			   _periodicBandwidthUsed[frame][2],
			   _periodicBandwidthUsed[frame][3],
			   _periodicBandwidthUsed[frame][4],
			   _periodicBandwidthUsed[frame][5],
			   _periodicBandwidthUsed[frame][6],
			   _periodicBandwidthUsed[frame][7]);
	return kIOReturnSuccess;
}


