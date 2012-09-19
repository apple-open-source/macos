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


#include <libkern/OSByteOrder.h>

extern "C" {
#include <kern/clock.h>
}

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMemoryCursor.h>

#include <IOKit/usb/IOUSBLog.h>
#include <IOKit/usb/IOUSBRootHubDevice.h>

#include "AppleUSBOHCI.h"
#include "AppleUSBOHCIMemoryBlocks.h"
#include "USBTracepoints.h"

#define super IOUSBControllerV3

static inline OHCIEDFormat
GetEDType(AppleOHCIEndpointDescriptorPtr pED)
{
    return ((USBToHostLong(pED->pShared->flags) & kOHCIEDControl_F) >> kOHCIEDControl_FPhase);
}



IOReturn 
AppleUSBOHCI::CreateGeneralTransfer(AppleOHCIEndpointDescriptorPtr queue, IOUSBCommand* command, IOMemoryDescriptor* CBP, UInt32 bufferSize, UInt32 flags, UInt32 type, UInt32 kickBits)
{
    AppleOHCIGeneralTransferDescriptorPtr	pOHCIGeneralTransferDescriptor = NULL,
											newOHCIGeneralTransferDescriptor = NULL;
    IOReturn								status = kIOReturnSuccess;
    IOByteCount								transferOffset;
    UInt32									pageCount;
    UInt32									altFlags;		// for all but the final TD
    IOUSBCompletion							completion = command->GetUSLCompletion();
	IODMACommand							*dmaCommand = command->GetDMACommand();
	UInt64									offset;
	IODMACommand::Segment32					segments32[2];
	IODMACommand::Segment64					segments64[2];
	UInt32									i;

    // Handy for debugging transfer lists
    flags |= (kOHCIGTDConditionNotAccessed << kOHCIGTDControl_CCPhase);
    
    // Clear bufferRounding for all but the last TD
    altFlags = flags & ~kOHCIGTDControl_R;			
    
    // Set the DI bits (Delay Interrupt) to 111b on all but the last TD
    // (this means that only the last TD will generate an interrupt)
    //
    altFlags |= ( 0x7 << kOHCIGTDControl_DIPhase );

    // FERG DEBUG
    // uncomment the next line to force the data to be put in TD list, but not be processed
    // this is handy for using USBProber/Macsbug to look at TD's to see if they're OK.
    // pEDQueue->dWord0 |= HostToUSBLong(kOHCIEDControl_K);
    // FERG DEBUG

    // 5-14-02 JRH
    // 2905718
    // before we do anything, check to make sure that the endpoint is not halted. if it is, return an immediate error.
    // 4-2-03 FAU
    // Don't do this for a control transaction to endpoint 0
    //
    if ( (USBToHostLong(queue->pShared->tdQueueHeadPtr) & kOHCIHeadPointer_H) && !( (type == kOHCIControlSetupType) && ( ((USBToHostLong(queue->pShared->flags) & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase) == 0 )) )
    {
        USBLog(1, "AppleUSBOHCI[%p]::CreateGeneralTransfer - trying to queue to a stalled pipe", this);
		USBTrace( kUSBTOHCI, kTPOHCICreateGeneralTransfer , (uintptr_t)this, kOHCIControlSetupType, kIOUSBPipeStalled, 0 );
        status = kIOUSBPipeStalled;
    }
    else if (bufferSize != 0)
    {
		if (CBP)
		{
			if (!dmaCommand)
			{
				USBError(1, "AppleUSBOHCI[%p]::CreateGeneralTransfer - no dmaCommand", this);
				status = kIOReturnInternalError;
			}
			else if (dmaCommand->getMemoryDescriptor() != CBP)
			{
				USBError(1, "AppleUSBOHCI[%p]::CreateGeneralTransfer - mismatched CBP (%p) and dmaCommand memory descriptor (%p)", this, CBP, dmaCommand->getMemoryDescriptor());
				status = kIOReturnInternalError;
			}
		}
		else
		{
			USBError(1, "AppleUSBOHCI[%p]::CreateGeneralTransfer - nonZero bufferSize and no CBP", this);
			status = kIOReturnInternalError;
		}
		if (!status)
		{
			transferOffset = 0;
			while (transferOffset < bufferSize)
			{
				offset = transferOffset;
				if (_errataBits & kErrataOnlySinglePageTransfers)
					pageCount = 1;
				else
					pageCount = 2;
				
				USBLog(7, "AppleUSBOHCI[%p]::CreateGeneralTransfer - getting segments - offset (%qd) pageCount (%d) transferOffset (%d) bufferSize (%d)", this, offset, (int)pageCount, (int)transferOffset, (int)bufferSize);
				status = dmaCommand->gen64IOVMSegments(&offset, segments64, &pageCount);
				if (status || ((pageCount != 1) && (pageCount != 2)))
				{
					USBError(1, "AppleUSBOHCI[%p]::CreateGeneralTransfer - could not generate segments - err (%p) pageCount (%d) offset (%qd) transferOffset (%d) bufferSize (%d) getMemoryDescriptor (%p)", this, (void*)status, (int)pageCount, offset, (int)transferOffset, (int)bufferSize, dmaCommand->getMemoryDescriptor());
					status = status ? status : kIOReturnInternalError;
					return status;
				}
				if (pageCount == 2)
				{
					USBLog(7, "AppleUSBOHCI[%p]::CreateGeneralTransfer  - after gen64IOVMSegments, offset (%qd) pageCount (%d) segments64[0].fIOVMAddr (%p) segments64[0].fLength (%d) segments64[1].fIOVMAddr (%p) segments64[1].fLength (%d)", this, offset, (int)pageCount, (void*)segments64[0].fIOVMAddr, (int)segments64[0].fLength, (void*)segments64[1].fIOVMAddr, (int)segments64[1].fLength);
				}
				else
				{
					USBLog(7, "AppleUSBOHCI[%p]::CreateGeneralTransfer  - after gen64IOVMSegments, offset (%qd) pageCount (%d) segments64[0].fIOVMAddr (%p) segments64[0].fLength (%d)", this, offset, (int)pageCount, (void*)segments64[0].fIOVMAddr, (int)segments64[0].fLength);
				}
				for (i=0; i< pageCount; i++)
				{
					if (((UInt32)(segments64[i].fIOVMAddr >> 32) > 0) || ((UInt32)(segments64[i].fLength >> 32) > 0))
					{
						USBError(1, "AppleUSBOHCI[%p]::CreateGeneralTransfer - generated segments (%d) not 32 bit -  offset (0x%qx) length (0x%qx) ", this, (int)i, segments64[0].fIOVMAddr, segments64[0].fLength);
						return kIOReturnInternalError;
					}
					// OK to convert to 32 bit (which it should have been already)
					segments32[i].fIOVMAddr = (UInt32)segments64[i].fIOVMAddr;
					segments32[i].fLength = (UInt32)segments64[i].fLength;
				}

				newOHCIGeneralTransferDescriptor = AllocateTD();
				if (newOHCIGeneralTransferDescriptor == NULL) 
				{
					status = kIOReturnNoMemory;
					break;
				}
	 
				// 3973735 - check to see if we have 2 pages, but we only need 1 to get to bufferSize
				if ((pageCount == 2) && (transferOffset + segments32[0].fLength >= bufferSize))
				{
					USBLog(6, "AppleUSBOHCI[%p]::CreateGeneralTransfer - bufferSize < Descriptor size - adjusting pageCount", this);
					pageCount = 1;
				}
				
				// if the first segment doesn't end on a page boundary, we will just do that much.
				if ((pageCount == 2) && ((((segments32[0].fIOVMAddr + segments32[0].fLength) & PAGE_MASK) != 0) || ((segments32[1].fIOVMAddr & PAGE_MASK) != 0)))
				{
					pageCount = 1; // we can only do one page here
					// must be a multiple of max packet size to avoid short packets
					if (segments32[0].fLength % ((USBToHostLong(queue->pShared->flags) & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase) != 0)
					{
						USBError(1, "AppleUSBOHCI[%p] CreateGeneralTransfer: non-multiple MPS transfer required -- giving up!", this);
						status = kIOReturnNoMemory;
						break;
					}
				}
				pOHCIGeneralTransferDescriptor = (AppleOHCIGeneralTransferDescriptorPtr)queue->pLogicalTailP;
				OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->currentBufferPtr, 0, segments32[0].fIOVMAddr);
				OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->nextTD, 0, newOHCIGeneralTransferDescriptor->pPhysical);
				if (pageCount == 2) 
				{
					// check to see if we need to use only part of the 2nd page
					if ((transferOffset + segments32[0].fLength + segments32[1].fLength) > bufferSize)
					{
						USBLog(6, "AppleUSBOHCI[%p]::CreateGeneralTransfer - bufferSize < Descriptor size - adjusting physical segment 1", this);
						segments32[1].fLength = bufferSize - (transferOffset + segments32[0].fLength);
					}
					OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->bufferEnd, 0, segments32[1].fIOVMAddr + segments32[1].fLength - 1);
					transferOffset += segments32[1].fLength;
					USBLog(7, "AppleUSBOHCI[%p]::CreateGeneralTransfer - added length of segment 1, transferOffset now %d", this, (int)transferOffset);
				}
				else
				{
					// need to check to make sure we need all of the 1st (and only) segment
					if ((transferOffset + segments32[0].fLength) > bufferSize)
					{
						USBLog(6, "AppleUSBOHCI[%p]::CreateGeneralTransfer - bufferSize < Descriptor size - adjusting physical segment 0", this);
						segments32[0].fLength = bufferSize - transferOffset;
					}
					OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->bufferEnd, 0, segments32[0].fIOVMAddr + segments32[0].fLength - 1);
				}
				
				pOHCIGeneralTransferDescriptor->pLogicalNext = newOHCIGeneralTransferDescriptor;
				pOHCIGeneralTransferDescriptor->pEndpoint = queue;
				pOHCIGeneralTransferDescriptor->pType = type;
				pOHCIGeneralTransferDescriptor->command = command;
				transferOffset += segments32[0].fLength;
				USBLog(7, "AppleUSBOHCI[%p]::CreateGeneralTransfer - added length of segment 0, transferOffset now %d", this, (int)transferOffset);

				// only supply a callback when the entire buffer has been transfered.
				if (transferOffset >= bufferSize)
				{
					pOHCIGeneralTransferDescriptor->pShared->ohciFlags = HostToUSBLong(flags);
					pOHCIGeneralTransferDescriptor->uimFlags |= kUIMFlagsCallbackTD;
					if (command->GetMultiTransferTransaction())
					{
						pOHCIGeneralTransferDescriptor->uimFlags |= kUIMFlagsMultiTDTransaction;
						if (command->GetFinalTransferInTransaction())
							pOHCIGeneralTransferDescriptor->uimFlags |= kUIMFlagsFinalTDinTransaction;
					}
				}
				else
				{
					pOHCIGeneralTransferDescriptor->pShared->ohciFlags = HostToUSBLong(altFlags);
					pOHCIGeneralTransferDescriptor->uimFlags &= ~kUIMFlagsCallbackTD;	// just to make sure. AllocateTD() does zero this
				}
				queue->pShared->tdQueueTailPtr = pOHCIGeneralTransferDescriptor->pShared->nextTD;
				queue->pLogicalTailP = newOHCIGeneralTransferDescriptor;
				OSWriteLittleInt32(&_pOHCIRegisters->hcCommandStatus, 0, kickBits);
			}
		}
    }
    else
    {
        newOHCIGeneralTransferDescriptor = AllocateTD();
        if (newOHCIGeneralTransferDescriptor == NULL) 
        {
            status = kIOReturnNoMemory;
        }
        else
        {
            // last in queue is dummy descriptor. Fill it in then add new dummy
            pOHCIGeneralTransferDescriptor = (AppleOHCIGeneralTransferDescriptorPtr) queue->pLogicalTailP;
    
            pOHCIGeneralTransferDescriptor->pShared->ohciFlags = HostToUSBLong(flags);
            OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->nextTD, 0, newOHCIGeneralTransferDescriptor->pPhysical);
            pOHCIGeneralTransferDescriptor->pLogicalNext = newOHCIGeneralTransferDescriptor;
            pOHCIGeneralTransferDescriptor->pEndpoint = queue;
            pOHCIGeneralTransferDescriptor->pType = type;
    
            /* for zero sized buffers */
            pOHCIGeneralTransferDescriptor->pShared->currentBufferPtr = 0;
            pOHCIGeneralTransferDescriptor->pShared->bufferEnd = 0;
            pOHCIGeneralTransferDescriptor->command = command;
            pOHCIGeneralTransferDescriptor->uimFlags |= kUIMFlagsCallbackTD;
			if (command->GetMultiTransferTransaction())
			{
				pOHCIGeneralTransferDescriptor->uimFlags |= kUIMFlagsMultiTDTransaction;
				if (command->GetFinalTransferInTransaction())
					pOHCIGeneralTransferDescriptor->uimFlags |= kUIMFlagsFinalTDinTransaction;
			}
            
            // Make new descriptor the tail
            queue->pShared->tdQueueTailPtr = pOHCIGeneralTransferDescriptor->pShared->nextTD;
            queue->pLogicalTailP = newOHCIGeneralTransferDescriptor;
            OSWriteLittleInt32(&_pOHCIRegisters->hcCommandStatus, 0, kickBits);
        }
    }

	// printTD(pOHCIGeneralTransferDescriptor, 7);
	
    if (status)
	{
        USBLog(1, "AppleUSBOHCI[%p] CreateGeneralTransfer: returning status 0x%x", this, status);
		USBTrace( kUSBTOHCI, kTPOHCICreateGeneralTransfer , (uintptr_t)this, status, 0, 0 );
	}
    return (status);
}



IOReturn 
AppleUSBOHCI::UIMCreateControlEndpoint(
                                          UInt8				functionAddress,
                                          UInt8				endpointNumber,
                                          UInt16			maxPacketSize,
                                          UInt8				speed,
                                          USBDeviceAddress    		highSpeedHub,
                                          int				highSpeedPort)
{
#pragma unused (highSpeedHub, highSpeedPort)
   USBLog(5, "AppleUSBOHCI[%p]: UIMCreateControlEndpoint w/ HS ( Addr: %d:%d, max=%d, %s) calling thru", this, functionAddress, endpointNumber, maxPacketSize, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");

    return UIMCreateControlEndpoint( functionAddress, endpointNumber, maxPacketSize, speed );
}



IOReturn 
AppleUSBOHCI::UIMCreateControlEndpoint(UInt8 functionAddress, UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed)
{
    AppleOHCIEndpointDescriptorPtr	pOHCIEndpointDescriptor, pED;

    USBLog(5, "AppleUSBOHCI[%p]: UIMCreateControlEndpoint( Addr: %d:%d, max=%d, %s)", this,
          functionAddress, endpointNumber, maxPacketSize, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");
    if (_rootHubFuncAddress == functionAddress)
    {
        if ( (endpointNumber != 0) && (speed == kUSBDeviceSpeedLow))
        {
            // Ignore High Speed for now
            USBLog(3,"AppleUSBOHCI[%p] UIMCreateControlEndpoint: Bad parameters endpoint: %d, speed: %s",this,endpointNumber, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");
            return kIOReturnBadArgument;
        }
        
        return SimulateControlEDCreate(maxPacketSize);
    }
        
    pED = _pControlHead;
    if ((speed == kUSBDeviceSpeedFull) && _OptiOn)
        pED = (AppleOHCIEndpointDescriptorPtr) _pBulkHead;

    pOHCIEndpointDescriptor = AddEmptyEndPoint(functionAddress,
                                               endpointNumber,
                                               maxPacketSize,
                                               speed,
                                               kOHCIEDDirectionTD,
                                               pED,
                                               kOHCIEDFormatGeneralTD);

    if (pOHCIEndpointDescriptor == NULL)
        return kIOReturnNoMemory;
        
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::UIMCreateControlTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor*			CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
#pragma unused (functionAddress, endpointNumber, completion, CBP, bufferRounding, bufferSize, direction)
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "AppleUSBOHCI[%p] UIMCreateControlTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
}



IOReturn 
AppleUSBOHCI::UIMCreateControlTransfer(short				functionAddress,
									   short				endpointNumber,
									   IOUSBCommand*		command,
									   IOMemoryDescriptor*	CBP,
									   bool					bufferRounding,
									   UInt32				bufferSize,
									   short				direction)
{
    UInt32								myBufferRounding = 0;
    UInt32								myDirection;
    UInt32								myToggle;
    AppleOHCIEndpointDescriptorPtr		pEDQueue, pEDDummy;
    IOReturn							status;
    IOUSBCompletion						completion = command->GetUSLCompletion();

    USBLog(7, "AppleUSBOHCI[%p]::UIMCreateControlTransfer: adr=%d:%d cbp=%p:%x br=%s cback=[%p:%p] dir=%d)", this,
          functionAddress, endpointNumber, CBP, (uint32_t)bufferSize,
          bufferRounding ? "YES":"NO",
             completion.target, completion.parameter, direction);

    if (direction == kUSBOut)
    {
        direction = kOHCIGTDPIDOut;
    }
    else if (direction == kUSBIn)
    {
        direction = kOHCIGTDPIDIn;
    }
    else
    {
        direction = kOHCIGTDPIDSetup;
    }
    // search for endpoint descriptor

    pEDQueue = FindControlEndpoint(functionAddress, endpointNumber, &pEDDummy);
    if (pEDQueue == NULL)
    {
        USBLog(3, "AppleUSBOHCI[%p] UIMCreateControlTransfer- Could not find endpoint (FN: %d, EP: %d)!", this, functionAddress, endpointNumber);
        return(kIOUSBEndpointNotFound);
    }
    if (bufferRounding)
		myBufferRounding = kOHCIGTDControl_R;
    myDirection = (UInt32) direction << kOHCIDirectionOffset;
    myToggle = kOHCIBit25;	/* Take data toggle from TD */
    if (direction != 0)
    {
        /* Setup uses Data 0, data status use Data1 */
        myToggle |= kOHCIBit24;	/* use Data1 */
    }

    status = CreateGeneralTransfer(pEDQueue, command, CBP, bufferSize, myBufferRounding | myDirection | myToggle, kOHCIControlSetupType,  kOHCIHcCommandStatus_CLF);

    return (status);
}



IOReturn 
AppleUSBOHCI::UIMCreateControlTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCompletion			completion,
            void*				CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
#pragma unused (functionAddress, endpointNumber, completion, CBP, bufferRounding, bufferSize, direction)
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "AppleUSBOHCI[%p]UIMCreateControlTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
}



IOReturn 
AppleUSBOHCI::UIMCreateControlTransfer(short				functionAddress,
									   short				endpointNumber,
									   IOUSBCommand*		command,
									   void*				CBP,
									   bool					bufferRounding,
									   UInt32				bufferSize,
									   short				direction)
{
#pragma unused (functionAddress, endpointNumber, command, CBP, bufferRounding, bufferSize, direction)
    IOMemoryDescriptor *		desc = NULL;
    IODirection					descDirection;
    IOReturn					status;
    IOUSBCompletion				completion = command->GetUSLCompletion();

    USBLog(1, "AppleUSBOHCI[%p]UIMCreateControlTransfer- calling the pointer method instead of the desc method!", this);
    return kIOReturnIPCError;
}

/* Not implemented - use UIMAbortEndpoint
IOReturn AppleUSBOHCI::UIMAbortControlEndpoint(void);
IOReturn AppleUSBOHCI::UIMEnableControlEndpoint(void);
IOReturn AppleUSBOHCI::UIMDisableControlEndpoint(void);
*/

IOReturn
AppleUSBOHCI::UIMCreateBulkEndpoint(
                                       UInt8				functionAddress,
                                       UInt8				endpointNumber,
                                       UInt8				direction,
                                       UInt8				speed,
                                       UInt16				maxPacketSize,
                                       USBDeviceAddress    		highSpeedHub,
                                       int			                highSpeedPort)
{
#pragma unused (highSpeedHub, highSpeedPort)
   USBLog(5,"AppleUSBOHCI[%p]: UIMCreateBulkEndpoint HS(Addr=%d:%d, max=%d, dir=%d, %s) calling thru", this,
           functionAddress, endpointNumber, maxPacketSize, direction, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");

    return UIMCreateBulkEndpoint( functionAddress, endpointNumber, direction, speed, maxPacketSize);
}


// Bulk
IOReturn AppleUSBOHCI::UIMCreateBulkEndpoint(
            UInt8				functionAddress,
            UInt8				endpointNumber,
            UInt8				direction,
            UInt8				speed,
            UInt8				maxPacketSize)
{
    AppleOHCIEndpointDescriptorPtr	pOHCIEndpointDescriptor, pED;


    USBLog(5,"AppleUSBOHCI[%p]: UIMCreateBulkEndpoint(Addr=%d:%d, max=%d, dir=%d, %s)", this,
          functionAddress, endpointNumber, maxPacketSize, direction, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");
    
    if (direction == kUSBOut)
            direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
            direction = kOHCIEDDirectionIn;
    else
            direction = kOHCIEDDirectionTD;

    pED = (AppleOHCIEndpointDescriptorPtr) _pBulkHead;
    pOHCIEndpointDescriptor = AddEmptyEndPoint (functionAddress,
                                                endpointNumber,
                                                maxPacketSize,
                                                speed,
                                                direction,
                                                pED,
                                                kOHCIEDFormatGeneralTD);
    if (pOHCIEndpointDescriptor == NULL)
        return(kIOReturnNoMemory);

    return (kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::UIMCreateBulkTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
#pragma unused (functionAddress, endpointNumber, completion, CBP, bufferRounding, bufferSize, direction)
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "AppleUSBOHCI[%p]UIMCreateBulkTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
}



IOReturn 
AppleUSBOHCI::UIMCreateBulkTransfer(IOUSBCommand* command)
{
    IOReturn							status = kIOReturnSuccess;
    UInt32								myBufferRounding = 0;
    UInt32								TDDirection;
    UInt32								kickBits;
    AppleOHCIEndpointDescriptorPtr		pEDQueue, pEDDummy;
    IOUSBCompletion						completion = command->GetUSLCompletion();
    short								direction = command->GetDirection();
    IOMemoryDescriptor*					buffer = command->GetBuffer();

    USBLog(7, "AppleUSBOHCI[%p]::UIMCreateBulkTransfer: adr=%d:%d cbp=%p:%qx br=%s cback=[%p:%p:%p] dir=%d)",this,
	command->GetAddress(), command->GetEndpoint(), buffer, (uint64_t)command->GetReqCount(), command->GetBufferRounding() ?"YES":"NO", 
	completion.action, completion.target, completion.parameter, direction);

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    // search for endpoint descriptor
    pEDQueue = FindBulkEndpoint(command->GetAddress(), command->GetEndpoint(), direction, &pEDDummy);

    if (!pEDQueue)
    {
        USBLog(3, "AppleUSBOHCI[%p] UIMCreateBulkTransfer- Could not find endpoint!", this);
        return (kIOUSBEndpointNotFound);
    }

    if (command->GetBufferRounding())
	myBufferRounding = kOHCIGTDControl_R;
    TDDirection = (UInt32) direction << kOHCIDirectionOffset;
    kickBits = kOHCIHcCommandStatus_BLF;
    if ( _OptiOn)
        kickBits |= kOHCIHcCommandStatus_CLF;		

    status = CreateGeneralTransfer(pEDQueue, command, buffer, command->GetReqCount(), myBufferRounding | TDDirection, kOHCIBulkTransferOutType, kickBits);

    return (status);
}



IOReturn
AppleUSBOHCI::UIMCreateInterruptEndpoint(
                                            short				functionAddress,
                                            short				endpointNumber,
                                            UInt8				direction,
                                            short				speed,
                                            UInt16				maxPacketSize,
                                            short				pollingRate,
                                            USBDeviceAddress    		highSpeedHub,
                                            int                 		highSpeedPort)
{
#pragma unused (highSpeedHub, highSpeedPort)
    USBLog(5, "AppleUSBOHCI[%p]: UIMCreateInterruptEndpoint HS ( Addr: %d:%d, max=%d, dir=%d, rate=%d, %s) calling thru", this,
           functionAddress, endpointNumber, maxPacketSize,direction,
           pollingRate, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");

    return UIMCreateInterruptEndpoint( functionAddress, endpointNumber, direction, speed, maxPacketSize, pollingRate);
}

// Interrupt
IOReturn 
AppleUSBOHCI::UIMCreateInterruptEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt8				direction,
            short				speed,
            UInt16				maxPacketSize,
            short				pollingRate)
{
    AppleOHCIEndpointDescriptorPtr		pOHCIEndpointDescriptor;
    AppleOHCIEndpointDescriptorPtr		pED, temp;
    int                                 offset;
    short								originalDirection = direction;
    UInt32								currentToggle = 0;
    
    USBLog(5, "AppleUSBOHCI[%p]: UIMCreateInterruptEndpoint ( Addr: %d:%d, max=%d, dir=%d, rate=%d, %s)", this,
           functionAddress, endpointNumber, maxPacketSize,direction,
           pollingRate, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");
    
    if (_rootHubFuncAddress == functionAddress)
    {
        if ( (endpointNumber != 1) || ( speed != kUSBDeviceSpeedFull ) || (direction != kUSBIn) )
        {
            USBLog(3, "AppleUSBOHCI[%p]: UIMCreateInterruptEndpoint bad parameters: endpNumber %d, speed: %s, direction: %d", this, endpointNumber, (speed == kUSBDeviceSpeedLow) ? "lo" : "full", direction);
            return kIOReturnBadArgument;
        }
        
        return RootHubStartTimer32(pollingRate);
    }
    
    // Modify direction to be an OHCI direction, as opposed to the USB direction.
    //
    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;
    
    // If the interrupt already exists, then we need to delete it first, as we're probably trying
    // to change the Polling interval via SetPipePolicy().
    //
    pED = FindInterruptEndpoint(functionAddress, endpointNumber, direction, &temp);
    if ( pED != NULL )
    {
        IOReturn ret;
        USBLog(3, "AppleUSBOHCI[%p]: UIMCreateInterruptEndpoint endpoint already existed -- deleting it", this);
		
		currentToggle = USBToHostLong(pED->pShared->tdQueueHeadPtr) & (kOHCIHeadPointer_C);
		if ( currentToggle != 0)
		{
			USBLog(6,"AppleUSBOHCI[%p]::UIMCreateInterruptEndpoint:  Preserving a data toggle of 1 before of the EP that we are going to delete!", this);
		}
		
        ret = UIMDeleteEndpoint(functionAddress, endpointNumber, originalDirection);
        if ( ret != kIOReturnSuccess)
        {
            USBLog(3, "AppleUSBOHCI[%p]: UIMCreateInterruptEndpoint deleting endpoint returned 0x%x", this, ret);
            return ret;
        }
    }
    else
	{
        USBLog(6, "AppleUSBOHCI[%p]: UIMCreateInterruptEndpoint endpoint does NOT exist", this);
	}
    
    
    ///ZZZZz  opti bug fix!!!!
    if (_OptiOn)
        if (speed == kUSBDeviceSpeedFull)
            if (pollingRate >= 8)
                pollingRate = 7;
    
    // Do we have room?? if so return with offset equal to location
    if (DetermineInterruptOffset(pollingRate, maxPacketSize, &offset) == false)
        return(kIOReturnNoBandwidth);
    
    USBLog(5, "AppleUSBOHCI[%p]: UIMCreateInterruptEndpoint: offset = %d", this, offset);
    
    pED = (AppleOHCIEndpointDescriptorPtr) _pInterruptHead[offset].pHead;
    pOHCIEndpointDescriptor = AddEmptyEndPoint (functionAddress, endpointNumber, 
                                                maxPacketSize, speed, direction, pED, kOHCIEDFormatGeneralTD);
    if (NULL == pOHCIEndpointDescriptor)
        return(-1);
    
    _pInterruptHead[offset].nodeBandwidth += maxPacketSize;
    
	// Write back the toggle in case we deleted the EP and recreated it
	pOHCIEndpointDescriptor->pShared->tdQueueHeadPtr |= HostToUSBLong(currentToggle);
		
   //  print_int_list(7);
    
    return (kIOReturnSuccess);
}



IOReturn
AppleUSBOHCI::UIMCreateInterruptTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCompletion			completion,
            IOMemoryDescriptor *		CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
#pragma unused (functionAddress, endpointNumber, completion, CBP, bufferRounding, bufferSize, direction)
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "AppleUSBOHCI[%p]UIMCreateInterruptTransfer- calling the wrong method!", this);
    return kIOReturnIPCError;
}



IOReturn
AppleUSBOHCI::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
    IOReturn							status = kIOReturnSuccess;
    UInt32								myBufferRounding = 0;
    UInt32								myDirection;
    UInt32								myToggle;
    AppleOHCIEndpointDescriptorPtr		pEDQueue, temp;
    IOUSBCompletion						completion = command->GetUSLCompletion();
    IOMemoryDescriptor*					buffer = command->GetBuffer();
    short								direction = command->GetDirection(); // our local copy may change

    if (_rootHubFuncAddress == command->GetAddress())
    {
		IODMACommand			*dmaCommand = command->GetDMACommand();
		IOMemoryDescriptor		*memDesc = dmaCommand ? (IOMemoryDescriptor*)dmaCommand->getMemoryDescriptor() : NULL;
		
		if (memDesc)
		{
			USBLog(3, "AppleUSBOHCI[%p]::UIMCreateInterruptTransfer - root hub interrupt transfer - clearing  unneeded memDesc (%p) from dmaCommand (%p)", this, memDesc, dmaCommand);
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

    USBLog(7, "AppleUSBOHCI[%p]::UIMCreateInterruptTransfer: adr=%d:%d cbp=%p:%qx br=%s cback=[%p:%p:%p])", this,
	    command->GetAddress(), command->GetEndpoint(), command->GetBuffer(), 
	    (uint64_t)command->GetReqCount(), command->GetBufferRounding()?"YES":"NO", 
	    completion.action, completion.target, 
	    completion.parameter);

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    pEDQueue = FindInterruptEndpoint(command->GetAddress(), command->GetEndpoint(), direction, &temp);
    if (pEDQueue != NULL)
    {
		UInt32 edFlags = USBToHostLong(pEDQueue->pShared->flags);
		if ( ((edFlags & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase) == 0 )
		{
			USBLog(2, "AppleUSBOHCI[%p]::UIMCreateInterruptTransfer - maxPacketSize is 0, returning kIOUSBNotEnoughBandwidth", this);
			status = kIOReturnNoBandwidth;
		}
		else 
		{
			if (command->GetBufferRounding())
				myBufferRounding = kOHCIGTDControl_R;
			myToggle = 0;	/* Take data toggle from Endpoint Descriptor */
			
			myDirection = (UInt32) direction << kOHCIDirectionOffset;
			
			status = CreateGeneralTransfer(pEDQueue, command, buffer, command->GetReqCount(), myBufferRounding | myDirection | myToggle, kOHCIInterruptInType, 0);
		}
    }
    else
    {
        USBLog(3, "AppleUSBOHCI[%p] UIMCreateInterruptTransfer- Could not find endpoint!", this);
        status = kIOUSBEndpointNotFound;
    }

    return (status);
}



IOReturn
AppleUSBOHCI::UIMCreateIsochEndpoint(
                                          short			functionAddress,
                                          short			endpointNumber,
                                          UInt32		maxPacketSize,
                                          UInt8			direction,
                                          USBDeviceAddress 	highSpeedHub,
                                          int      	highSpeedPort)
{
#pragma unused (highSpeedHub, highSpeedPort)
    return UIMCreateIsochEndpoint( functionAddress, endpointNumber, maxPacketSize, direction);
}

// Isoch
IOReturn 
AppleUSBOHCI::UIMCreateIsochEndpoint(
            short				functionAddress,
            short				endpointNumber,
            UInt32				maxPacketSize,
            UInt8				direction)
{
    AppleOHCIEndpointDescriptorPtr	pOHCIEndpointDescriptor, pED;
    UInt32			curMaxPacketSize;
    UInt32			xtraRequest;
    UInt32			edFlags;


    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    pED = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);
    if (pED) 
	{
        // this is the case where we have already created this endpoint, and now we are adjusting the maxPacketSize
        //
        USBLog(2,"AppleUSBOHCI[%p]::UIMCreateIsochEndpoint endpoint already exists, changing maxPacketSize to %d", this, (uint32_t)maxPacketSize);

        edFlags = USBToHostLong(pED->pShared->flags);
        curMaxPacketSize = ( edFlags & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase;
        if (maxPacketSize == curMaxPacketSize) 
		{
            USBLog(2,"AppleUSBOHCI[%p]::UIMCreateIsochEndpoint maxPacketSize (%d) the same, no change", this, (uint32_t)maxPacketSize);
            return kIOReturnSuccess;
        }
        if (maxPacketSize > curMaxPacketSize) 
		{
            // client is trying to get more bandwidth
            xtraRequest = maxPacketSize - curMaxPacketSize;
            if (xtraRequest > _isochBandwidthAvail)
            {
                USBLog(2,"AppleUSBOHCI[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %d, available: %d", this, (uint32_t)xtraRequest, (uint32_t)_isochBandwidthAvail);
                return kIOReturnNoBandwidth;
            }
            _isochBandwidthAvail -= xtraRequest;
            USBLog(2,"AppleUSBOHCI[%p]::UIMCreateIsochEndpoint grabbing additional bandwidth: %d, new available: %d", this, (uint32_t)xtraRequest, (uint32_t)_isochBandwidthAvail);
        } 
		else 
		{
            // client is trying to return some bandwidth
            xtraRequest = curMaxPacketSize - maxPacketSize;
            _isochBandwidthAvail += xtraRequest;
            USBLog(2,"AppleUSBOHCI[%p]::UIMCreateIsochEndpoint returning some bandwidth: %d, new available: %d", this, (uint32_t)xtraRequest, (uint32_t)_isochBandwidthAvail);

        }
        // update the maxPacketSize field in the endpoint
        edFlags &= ~kOHCIEDControl_MPS;					// strip out old MPS
        edFlags |= (maxPacketSize << kOHCIEDControl_MPSPhase);
        OSWriteLittleInt32(&pED->pShared->flags, 0, edFlags);
        return kIOReturnSuccess;
    }

    if (maxPacketSize > _isochBandwidthAvail) 
    {
        USBLog(3,"AppleUSBOHCI[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %d, available: %d", this, (uint32_t)maxPacketSize, (uint32_t)_isochBandwidthAvail);
        return kIOReturnNoBandwidth;
    }

    _isochBandwidthAvail -= maxPacketSize;
    pED = _pIsochHead;
    pOHCIEndpointDescriptor = AddEmptyEndPoint(functionAddress, endpointNumber,
	maxPacketSize, kUSBDeviceSpeedFull, direction, pED, kOHCIEDFormatIsochronousTD);
    if (pOHCIEndpointDescriptor == NULL) 
	{
        _isochBandwidthAvail += maxPacketSize;
        return(kIOReturnNoMemory);
    }

    USBLog(5,"AppleUSBOHCI[%p]::UIMCreateIsochEndpoint success. bandwidth used = %d, new available: %d", this, (uint32_t)maxPacketSize, (uint32_t)_isochBandwidthAvail);

    return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::UIMCreateIsochTransfer(short						functionAddress,
									 short						endpointNumber,
									 IOUSBIsocCompletion		completion,
									 UInt8						direction,
									 UInt64						frameNumberStart,
									 IOMemoryDescriptor *		pBuffer,
									 UInt32						frameCount,
									 IOUSBIsocFrame	*			pFrames)
{
#pragma unused (functionAddress, endpointNumber, completion, frameNumberStart, frameNumberStart, pBuffer, direction, frameCount, pFrames)
	USBError(1, "AppleUSBOHCI::UIMCreateIsochTransfer - old method");
	return kIOReturnIPCError;
}



IOReturn 
AppleUSBOHCI::UIMAbortEndpoint(short				functionAddress,
							   short				endpointNumber,
							   short				direction)
{
    AppleOHCIEndpointDescriptorPtr	pED;
    AppleOHCIEndpointDescriptorPtr	pEDQueueBack;
    UInt32							something, controlMask;

    USBLog(5, "AppleUSBOHCI[%p] UIMAbortEndpoint: Addr: %d, Endpoint: %d,%d", this, functionAddress,endpointNumber,direction);

    if (functionAddress == _rootHubFuncAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "AppleUSBOHCI[%p] UIMAbortEndpoint: bad params - endpNumber: %d", this, endpointNumber );
			USBTrace( kUSBTOHCI, kTPOHCIAbortEndpoint , (uintptr_t)this, functionAddress, endpointNumber, kIOReturnBadArgument );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDAbort (endpointNumber, direction) in 9
        //
        USBLog(5, "AppleUSBOHCI[%p] UIMAbortEndpoint: Attempting operation on root hub", this);
        return SimulateEDAbort(endpointNumber, direction);
    }

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;
    
    //search for endpoint descriptor
    pED = FindEndpoint (functionAddress, endpointNumber, direction, &pEDQueueBack, &controlMask);
    if (!pED)
    {
        USBLog(3, "AppleUSBOHCI[%p] UIMAbortEndpoint- Could not find endpoint!", this);
        return (kIOUSBEndpointNotFound);
    }

    pED->pShared->flags |= HostToUSBLong(kOHCIEDControl_K);	// mark the ED as skipped

    // We used to wait for a SOF interrupt here.  Now just sleep for 2 ms:  1 to finish processing
    // the frame and 1 to let the filter interrupt routine finish
    //
    IOSleep(2);

    // Process the Done Queue in case there is something there
    //
    if (_writeDoneHeadInterrupt & kOHCIHcInterrupt_WDH)
    {
        _writeDoneHeadInterrupt = 0;
        UIMProcessDoneQueue(NULL);
    }

	// Remove any TDs from the endpoint, but do NOT reset the data toggle
    RemoveTDs(pED, false);

    pED->pShared->flags &= ~HostToUSBLong(kOHCIEDControl_K);	// activate ED again
	IOSync();

    return (kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::UIMDeleteEndpoint(
            short				functionAddress,
            short				endpointNumber,
            short				direction) 
{
    AppleOHCIEndpointDescriptorPtr	pED;
    AppleOHCIEndpointDescriptorPtr	pEDQueueBack;
    UInt32			hcControl;
    UInt32			something, controlMask;
    //	UInt32			edDirection;

    USBLog(5, "AppleUSBOHCI[%p] UIMDeleteEndpoint: Addr: %d, Endpoint: %d,%d", this, functionAddress,endpointNumber,direction);
    
    if (functionAddress == _rootHubFuncAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "AppleUSBOHCI[%p] UIMDeleteEndpoint: bad params - endpNumber: %d", this, endpointNumber );
			USBTrace( kUSBTOHCI, kTPOHCIDeleteEndpoint , (uintptr_t)this, functionAddress, endpointNumber, kIOReturnBadArgument );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDDelete (endpointNumber, direction) in 9
        //
        USBLog(5, "AppleUSBOHCI[%p] UIMDeleteEndpoint: Attempting operation on root hub", this);
        return SimulateEDDelete( endpointNumber, direction);
    }

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    //search for endpoint descriptor
    pED = FindEndpoint (functionAddress,
                        endpointNumber,
                        direction,
                        &pEDQueueBack,
                        &controlMask); 
    if (!pED)
    {
        USBLog(3, "AppleUSBOHCI[%p] UIMDeleteEndpoint- Could not find endpoint!", this);
        return (kIOUSBEndpointNotFound);
    }
    
    // Remove Endpoint
    //mark sKipped
    pED->pShared->flags |= HostToUSBLong(kOHCIEDControl_K);
    //	edDirection = HostToUSBLong(pED->dWord0) & kOHCIEndpointDirectionMask;
    // remove pointer wraps
    pEDQueueBack->pShared->nextED = pED->pShared->nextED;
    pEDQueueBack->pLogicalNext = pED->pLogicalNext;

    // clear some bit in hcControl
    hcControl = USBToHostLong(_pOHCIRegisters->hcControl);	
    hcControl &= ~controlMask;
    hcControl &= OHCIBitRange(0, 10);

    _pOHCIRegisters->hcControl = HostToUSBLong(hcControl);

    // We used to wait for a SOF interrupt here.  Now just sleep for 2 ms:  1 to finish processing
    // the frame and 1 to let the filter interrupt routine finish
    //
    IOSleep(2);

    // Process the Done Queue in case there is something there
    //
    if (_writeDoneHeadInterrupt & kOHCIHcInterrupt_WDH)
    {
        _writeDoneHeadInterrupt = 0;
        UIMProcessDoneQueue(NULL);
    }
    
    // restart hcControl
    hcControl |= controlMask;
    _pOHCIRegisters->hcControl = HostToUSBLong(hcControl);

    USBLog(5, "AppleUSBOHCI[%p]::UIMDeleteEndpoint", this); 
    
    if (GetEDType(pED) == kOHCIEDFormatIsochronousTD)
    {
        UInt32 maxPacketSize = (USBToHostLong(pED->pShared->flags) & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase;
        _isochBandwidthAvail += maxPacketSize;
        USBLog(5, "AppleUSBOHCI[%p]::UIMDeleteEndpoint (Isoch) - bandwidth returned %d, new available: %d", this, (uint32_t)maxPacketSize, (uint32_t)_isochBandwidthAvail);
    }
    RemoveAllTDs(pED);

    pED->pShared->nextED = NULL;

    //deallocate ED
    DeallocateED(pED);
       
	return (kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::UIMClearEndpointStall(short functionAddress, short endpointNumber, short direction)
{
    AppleOHCIEndpointDescriptorPtr			pEDQueueBack, pED;
    AppleOHCIGeneralTransferDescriptorPtr	transaction;
    UInt32									tail, controlMask;


    USBLog(5, "+AppleUSBOHCI[%p]: clearing endpoint %d:%d stall", this, functionAddress, endpointNumber);
    
    if (_rootHubFuncAddress == functionAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "AppleUSBOHCI[%p] UIMClearEndpointStall: bad params - endpNumber: %d", this, endpointNumber );
			USBTrace( kUSBTOHCI, kTPOHCIEndpointStall , (uintptr_t)this, functionAddress, endpointNumber, kIOReturnBadArgument );
            return kIOReturnBadArgument;
        }
        
        USBLog(5, "AppleUSBOHCI[%p] UIMClearEndpointStall: Attempting operation on root hub", this);
        return SimulateEDClearStall(endpointNumber, direction);
    }

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    transaction = NULL;
    tail = NULL;
    //search for endpoint descriptor
    pED = FindEndpoint (functionAddress, endpointNumber, direction, &pEDQueueBack, &controlMask);
    if (!pED)
    {
        USBLog(3, "AppleUSBOHCI[%p] UIMClearEndpointStall- Could not find endpoint!", this);
        return kIOUSBEndpointNotFound;
    }

	if (pED->pAborting)
	{
		// 7946083 - don't allow the abort to recurse
        USBLog(1, "AppleUSBOHCI[%p]::UIMClearEndpointStall- Already aborting endpoint [%p] - not recursing!", this, pED);
		return kIOUSBClearPipeStallNotRecursive;
	}
	pED->pAborting = true;
	tail = USBToHostLong(pED->pShared->tdQueueTailPtr);
	transaction = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);
	
	// Unlink all transactions at once (this also clears the halted bit AND resets the data toggle)
	pED->pShared->tdQueueHeadPtr = pED->pShared->tdQueueTailPtr;
	pED->pLogicalHeadP = pED->pLogicalTailP;

    if (transaction != NULL)
    {
        ReturnTransactions(transaction, tail);
    }
    
    USBLog(5, "-AppleUSBOHCI[%p]: clearing endpoint %d:%d stall", this, functionAddress, endpointNumber);
	pED->pAborting = false;
    return kIOReturnSuccess;
}



AppleOHCIEndpointDescriptorPtr 
AppleUSBOHCI::AddEmptyEndPoint(
        UInt8 						functionAddress,
        UInt8						endpointNumber,
        UInt16						maxPacketSize,
        UInt8						speed,
        UInt8						direction,
        AppleOHCIEndpointDescriptorPtr			pED,
        OHCIEDFormat					format)
{
    UInt32				myFunctionAddress,
    					myEndpointNumber,
    					myEndpointDirection,
    					myMaxPacketSize,
    					mySpeed,
    					myFormat;
    AppleOHCIEndpointDescriptorPtr		pOHCIEndpointDescriptor;
    AppleOHCIGeneralTransferDescriptorPtr	pOHCIGeneralTransferDescriptor;
    AppleOHCIIsochTransferDescriptorPtr	pITD;

    
    pOHCIEndpointDescriptor = (AppleOHCIEndpointDescriptorPtr) AllocateED();
    myFunctionAddress = ((UInt32) functionAddress) << kOHCIEDControl_FAPhase;
    myEndpointNumber = ((UInt32) endpointNumber) << kOHCIEDControl_ENPhase;
    myEndpointDirection = ((UInt32) direction) << kOHCIEDControl_DPhase;
    if (speed == kUSBDeviceSpeedFull)
        mySpeed = kOHCIEDSpeedFull << kOHCIEDControl_SPhase;
    else
        mySpeed = kOHCIEDSpeedLow << kOHCIEDControl_SPhase;
    myMaxPacketSize = ((UInt32) maxPacketSize) << kOHCIEDControl_MPSPhase;
    myFormat = ((UInt32) format) << kOHCIEDControl_FPhase;
    pOHCIEndpointDescriptor->pShared->flags = HostToUSBLong(myFunctionAddress
					| myEndpointNumber
					| myEndpointDirection
					| myMaxPacketSize
					| mySpeed
					| myFormat);

    if (format == kOHCIEDFormatGeneralTD)
    {
        pOHCIGeneralTransferDescriptor = AllocateTD();
        if (pOHCIGeneralTransferDescriptor == NULL) 
        {
            return NULL;
        }

        /* These were previously nil */
        pOHCIEndpointDescriptor->pShared->tdQueueTailPtr = HostToUSBLong( pOHCIGeneralTransferDescriptor->pPhysical);
        pOHCIEndpointDescriptor->pShared->tdQueueHeadPtr = HostToUSBLong( pOHCIGeneralTransferDescriptor->pPhysical);
        pOHCIEndpointDescriptor->pLogicalTailP = pOHCIGeneralTransferDescriptor;
        pOHCIEndpointDescriptor->pLogicalHeadP = pOHCIGeneralTransferDescriptor;
    }
    else
    {
        pITD = AllocateITD();
        if (pITD == NULL) 
        {
            return NULL;
        }

        /* These were previously nil */
        pOHCIEndpointDescriptor->pShared->tdQueueTailPtr = HostToUSBLong( pITD->pPhysical);
        pOHCIEndpointDescriptor->pShared->tdQueueHeadPtr = HostToUSBLong( pITD->pPhysical);
        pOHCIEndpointDescriptor->pLogicalTailP = pITD;
        pOHCIEndpointDescriptor->pLogicalHeadP = pITD;		

    }

    pOHCIEndpointDescriptor->pShared->nextED = pED->pShared->nextED;
    pOHCIEndpointDescriptor->pLogicalNext = pED->pLogicalNext;
    pED->pLogicalNext = pOHCIEndpointDescriptor;
    pED->pShared->nextED = HostToUSBLong(pOHCIEndpointDescriptor->pPhysical);

    return (pOHCIEndpointDescriptor);
}



AppleOHCIEndpointDescriptorPtr 
AppleUSBOHCI::FindControlEndpoint (
	short 						functionNumber, 
	short						endpointNumber, 
	AppleOHCIEndpointDescriptorPtr   			*pEDBack)
{
    UInt32			unique;
    AppleOHCIEndpointDescriptorPtr	pEDQueue;
    AppleOHCIEndpointDescriptorPtr	pEDQueueBack;

	
    // search for endpoint descriptor
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEndpointNumberOffset) | ((UInt32) functionNumber));
    pEDQueueBack = _pControlHead;
    pEDQueue = pEDQueueBack->pLogicalNext;

    while (pEDQueue != _pControlTail)
    {
        if ((USBToHostLong(pEDQueue->pShared->flags) & kOHCIUniqueNumNoDirMask) == unique)
        {
            *pEDBack = pEDQueueBack;
            return (pEDQueue);
        }
        else
        {
            pEDQueueBack = pEDQueue;
            pEDQueue = (AppleOHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
        }
    }
    if (_OptiOn)
    {
        pEDQueue = FindBulkEndpoint (functionNumber, endpointNumber, kOHCIEDDirectionTD, &pEDQueueBack);
        *pEDBack = pEDQueueBack;
        return (pEDQueue);
    }
    return NULL;
}



AppleOHCIEndpointDescriptorPtr 
AppleUSBOHCI::FindBulkEndpoint (
	short									functionNumber, 
	short									endpointNumber,
	short									direction,
	AppleOHCIEndpointDescriptorPtr			*pEDBack)
{

    UInt32			unique;
    UInt32			myEndpointDirection;
    AppleOHCIEndpointDescriptorPtr	pEDQueue;
    AppleOHCIEndpointDescriptorPtr	pEDQueueBack;


    // search for endpoint descriptor
    myEndpointDirection = ((UInt32) direction) << kOHCIEndpointDirectionOffset;
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEndpointNumberOffset) | ((UInt32) functionNumber) | myEndpointDirection);
    pEDQueueBack = (AppleOHCIEndpointDescriptorPtr) _pBulkHead;
    pEDQueue = pEDQueueBack->pLogicalNext;

    while ( pEDQueue != _pBulkTail )
    {
        if ((USBToHostLong(pEDQueue->pShared->flags) & kUniqueNumMask) == unique)
        {
            *pEDBack = pEDQueueBack;
            return (pEDQueue);
        }
        else
        {
            pEDQueueBack = pEDQueue;
            pEDQueue = pEDQueue->pLogicalNext;
        }
    }
    return NULL;
}



AppleOHCIEndpointDescriptorPtr 
AppleUSBOHCI::FindEndpoint (
	short 						functionNumber, 
	short 						endpointNumber,
	short 						direction, 
	AppleOHCIEndpointDescriptorPtr 			*pEDQueueBack, 
	UInt32 						*controlMask)
{
    AppleOHCIEndpointDescriptorPtr pED, pEDBack;

    pED = FindControlEndpoint (functionNumber, endpointNumber, &pEDBack);
    if (pED != NULL)
    {
        *pEDQueueBack = pEDBack;
        *controlMask = kOHCIHcControl_CLE;
        return (pED);
    }

    pED = FindBulkEndpoint(functionNumber, endpointNumber, direction, &pEDBack);
    if (pED != NULL)
    {
        *pEDQueueBack = pEDBack;

        *controlMask = kOHCIHcControl_BLE;
        //zzzz Opti Bug
        if (_OptiOn)
            *controlMask = kOHCIHcControl_CLE;
        return (pED);
    }

    pED = FindInterruptEndpoint(functionNumber, endpointNumber, direction,
	&pEDBack);
    if (pED != NULL)
    {
        *pEDQueueBack = pEDBack;
        *controlMask = 0;
        return (pED);	
    }

    pED = FindIsochronousEndpoint(functionNumber, endpointNumber,
                                  direction, &pEDBack);
    *pEDQueueBack = pEDBack;
    *controlMask = 0;
    return (pED);	
}


AppleOHCIEndpointDescriptorPtr 
AppleUSBOHCI::FindIsochronousEndpoint(
	short 						functionNumber,
	short						endpointNumber,
	short 						direction, 
	AppleOHCIEndpointDescriptorPtr			*pEDBack)
{
    UInt32			myEndpointDirection;
    UInt32			unique;
    AppleOHCIEndpointDescriptorPtr	pEDQueue, pEDQueueBack;

    // search for endpoint descriptor
    myEndpointDirection = ((UInt32) direction) << kOHCIEndpointDirectionOffset;
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEndpointNumberOffset)
                       | ((UInt32) functionNumber) | myEndpointDirection);

    pEDQueueBack = (AppleOHCIEndpointDescriptorPtr) _pIsochHead;
    pEDQueue = pEDQueueBack->pLogicalNext;
    while (pEDQueue != _pIsochTail )
    {
        if ((USBToHostLong(pEDQueue->pShared->flags) & kUniqueNumMask) == unique)
        {
            if (pEDBack)
                *pEDBack = pEDQueueBack;
            return (pEDQueue);
        }
        else
        {
            pEDQueueBack = pEDQueue;
            pEDQueue = pEDQueue->pLogicalNext;
        }
    }
    return NULL;
}


AppleOHCIEndpointDescriptorPtr 
AppleUSBOHCI::FindInterruptEndpoint(
	short								functionNumber,
	short								endpointNumber,
    short                               direction,
	AppleOHCIEndpointDescriptorPtr		*pEDBack)
{
    UInt32								myEndpointDirection;
    UInt32								unique;
    AppleOHCIEndpointDescriptorPtr		pEDQueue;
    int									i;
    UInt32								temp;
    
    //search for endpoint descriptor
    myEndpointDirection = ((UInt32) direction) << kOHCIEndpointDirectionOffset;
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEDControl_ENPhase)
                       | (((UInt32) functionNumber) << kOHCIEDControl_FAPhase)
                       | myEndpointDirection);
    
    for (i = 0; i < 63; i++)
    {
        pEDQueue = _pInterruptHead[i].pHead;
        *pEDBack = pEDQueue;
        // BT do this first, or you find the dummy endpoint
        // all this is hanging off. It matches 0,0
        pEDQueue = pEDQueue->pLogicalNext;
        
        while (pEDQueue != _pInterruptHead[i].pTail)
        {
            if (!pEDQueue)
            {
                // see rdar://9840383 - we may want to consider a panic here for logging builds to try to debug this, as it should never happen
                USBLog(1, "AppleUSBOHCI[%p]::FindInterruptEndpoint - UNEXPECTED NULL pEDQueue at index %d - _pInterruptHead[%p] *pEDBack[%p]", this, i, _pInterruptHead[i].pHead, *pEDBack);
                return NULL;
            }
            temp = (USBToHostLong(pEDQueue->pShared->flags)) & kUniqueNumMask;

            if ( temp == unique)
            {
                return pEDQueue;
            }
            *pEDBack = pEDQueue;
            pEDQueue = pEDQueue->pLogicalNext;
        }
    }
    return NULL;
}

bool AppleUSBOHCI::DetermineInterruptOffset(
    UInt32          pollingRate,
    UInt32          /* reserveBandwidth */,
    int             *offset)
{
    int num;

    num = USBToHostLong(_pOHCIRegisters->hcFmNumber) & kOHCIFmNumberMask;
    if (pollingRate <  1)
    {
        //error condition
        USBError(1,"AppleUSBOHCI[%p]::DetermineInterruptOffset pollingRate of 0 -- that's illegal!", this);
        return(false);
    }
    else if (pollingRate < 2)
        *offset = 62;
    else if (pollingRate < 4)
        *offset = (num%2) + 60;
    else if (pollingRate < 8)
        *offset = (num%4) + 56;
    else if (pollingRate < 16)
        *offset = (num%8) + 48;
    else if (pollingRate < 32)
        *offset = (num%16) + 32;
    else
        *offset = (num%32) + 0;
    return (true);
}

#pragma mark Debug Output
void 
AppleUSBOHCI::printTD(AppleOHCIGeneralTransferDescriptorPtr pTD, int level)
{
    UInt32 w0, dir;

    if (pTD == 0)
    {
        USBLog(level, "AppleUSBOHCI[%p]::printTD  Attempt to print null TD", this);
        return;
    }
	
    w0 = USBToHostLong(pTD->pShared->ohciFlags);
    dir = (w0 & kOHCIGTDControl_DP) >> kOHCIGTDControl_DPPhase;

 	USBLog(level, "AppleUSBOHCI[%p]::printTD: ------pTD at %p (%s)", this, pTD, dir == 0 ? "SETUP" : (dir==2?"IN":"OUT"));
	USBLog(level, "AppleUSBOHCI[%p]::printTD: pEndpoint:               %p (%d:%d)", this, pTD->pEndpoint, 
		   (USBToHostLong((pTD->pEndpoint)->pShared->flags) & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase,
		   (USBToHostLong((pTD->pEndpoint)->pShared->flags) & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase
		   );
	USBLog(level, "AppleUSBOHCI[%p]::printTD: shared.ohciFlags:         0x%x", this, USBToHostLong(pTD->pShared->ohciFlags));
	USBLog(level, "AppleUSBOHCI[%p]::printTD: shared.currentBufferPtr:  0x%x", this, USBToHostLong(pTD->pShared->currentBufferPtr));
	USBLog(level, "AppleUSBOHCI[%p]::printTD: shared.nextTD:            0x%x", this, USBToHostLong(pTD->pShared->nextTD));
	USBLog(level, "AppleUSBOHCI[%p]::printTD: shared.bufferEnd:         0x%x", this, USBToHostLong(pTD->pShared->bufferEnd));
	
	USBLog(level, "AppleUSBOHCI[%p]::printTD: pType:                    0x%x", this, pTD->pType);
	USBLog(level, "AppleUSBOHCI[%p]::printTD: uimFlags:                 0x%x", this, pTD->uimFlags);
	USBLog(level, "AppleUSBOHCI[%p]::printTD: pPhysical:                0x%x", this, (uint32_t)(pTD->pPhysical));
	USBLog(level, "AppleUSBOHCI[%p]::printTD: pLogicalNext:             %p", this, (pTD->pLogicalNext));
	
	USBLog(level, "AppleUSBOHCI[%p]::printTD: lastFrame:                0x%x",  this, (uint32_t)(pTD->lastFrame));
	USBLog(level, "AppleUSBOHCI[%p]::printTD: lastRemaining:            0x%x",  this, (uint32_t)(pTD->lastRemaining));
	USBLog(level, "AppleUSBOHCI[%p]::printTD: bufferSize:               0x%x",  this, (uint32_t)(pTD->bufferSize));
	
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQTD1, (uintptr_t)this, (uintptr_t)pTD, (uintptr_t)pTD->pEndpoint, (uint32_t)USBToHostLong(pTD->pShared->ohciFlags) );
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQTD6, (uintptr_t)this, (uintptr_t)pTD->pEndpoint, (uint32_t)USBToHostLong((pTD->pEndpoint)->pShared->flags), 0);
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQTD2, (uintptr_t)this, (uint32_t)USBToHostLong(pTD->pShared->currentBufferPtr), (uint32_t)USBToHostLong(pTD->pShared->nextTD), (uint32_t)USBToHostLong(pTD->pShared->nextTD));
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQTD3, (uintptr_t)this, (uint32_t)USBToHostLong(pTD->pShared->bufferEnd), (uint32_t)pTD->pType, (uint32_t)pTD->uimFlags);
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQTD4, (uintptr_t)this, (uint32_t)pTD->pPhysical, (uintptr_t)pTD->pLogicalNext, (uint32_t)pTD->lastFrame);
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQTD5, (uintptr_t)this, (uint32_t)pTD->lastRemaining, (uintptr_t)pTD->bufferSize, 0);
}

void 
AppleUSBOHCI::printED(AppleOHCIEndpointDescriptorPtr pED, int level)
{
    UInt32 w0, dir;

    if (pED == 0)
    {
        USBLog(level, "AppleUSBOHCI[%p]::printED  Attempt to print null ED", this);
        return;
    }

    w0 = USBToHostLong(pED->pShared->flags);
    dir = (w0 & kOHCIGTDControl_DP) >> kOHCIGTDControl_DPPhase;

	
 	USBLog(level, "AppleUSBOHCI[%p]::printED: ------ pED at %p (%d:%d)", this, pED, 
		   (uint32_t)(w0 & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase,
		   (uint32_t)(w0 & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase
		   );
	USBLog(level, "AppleUSBOHCI[%p]::printED: shared.flags:             0x%x %s", this, USBToHostLong(pED->pShared->flags),
		   w0 & kOHCIEDControl_K?"( SKIP )":""
		   );
	USBLog(level, "AppleUSBOHCI[%p]::printED: shared.tdQueueTailPtr:    0x%x", this, USBToHostLong(pED->pShared->tdQueueTailPtr));
	USBLog(level, "AppleUSBOHCI[%p]::printED: shared.tdQueueHeadPtr:    0x%x %s", this, USBToHostLong(pED->pShared->tdQueueHeadPtr),
		   USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPointer_H?"( HALTED )":""
		   );
	USBLog(level, "AppleUSBOHCI[%p]::printED: shared.nextED:            0x%x", this, USBToHostLong(pED->pShared->nextED));
	
	USBLog(level, "AppleUSBOHCI[%p]::printED: pPhysical:                0x%x", this, (uint32_t)(pED->pPhysical));
	USBLog(level, "AppleUSBOHCI[%p]::printED: pLogicalNext:             %p", this, (pED->pLogicalNext));
	
	USBLog(level, "AppleUSBOHCI[%p]::printED: pLogicalTailP:            %p",  this, (pED->pLogicalTailP));
	USBLog(level, "AppleUSBOHCI[%p]::printED: pLogicalHeadP:            %p",  this, (pED->pLogicalHeadP));
	USBLog(level, "AppleUSBOHCI[%p]::printED: pAborting:                %s",  this, (pED->pAborting ? "YES" : "NO"));
	
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQED1, (uintptr_t)this, (uintptr_t)pED, (uint32_t)USBToHostLong(pED->pShared->flags), (uint32_t)USBToHostLong(pED->pShared->tdQueueTailPtr) );
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQED2, (uintptr_t)this, (uint32_t)USBToHostLong(pED->pShared->tdQueueHeadPtr), (uint32_t)USBToHostLong(pED->pShared->nextED), (uint32_t)pED->pPhysical );
	USBTrace( kUSBTOHCIDumpQueues, kTPOHCIDumpQED3, (uintptr_t)this, (uintptr_t)pED->pLogicalNext, (uintptr_t)(pED->pLogicalTailP), (uintptr_t)pED->pLogicalHeadP );

}

static const char *cc_errors[] = {
    "NO ERROR",			/* 0 */
    "CRC",			/* 1 */
    "BIT STUFFING",		/* 2 */
    "DATA TOGGLE MISMATCH",	/* 3 */
    "STALL",			/* 4 */
    "DEVICE NOT RESPONDING",	/* 5 */
    "PID CHECK FAILURE",	/* 6 */
    "UNEXPECTED PID",		/* 7 */
    "DATA OVERRUN",		/* 8 */
    "DATA UNDERRUN",		/* 9 */
    "??",			/* reserved */
    "??",			/* reserved */
    "BUFFER OVERRUN",		/* 12 */
    "BUFFER UNDERRUN",		/* 13 */
    "NOT ACCESSED A",		/* not processed yet */
    "NOT ACCESSED B"		/* not processed yet */
};



void 
AppleUSBOHCI::print_td(AppleOHCIGeneralTransferDescriptorPtr pTD, int level)
{
    UInt32 w0, dir, err;
	
    if (pTD == 0) return;
	
    w0 = USBToHostLong(pTD->pShared->ohciFlags);
    dir = (w0 & kOHCIGTDControl_DP) >> kOHCIGTDControl_DPPhase;
    err = (w0 & kOHCIGTDControl_CC) >> kOHCIGTDControl_CCPhase;
    USBLog(level, "AppleUSBOHCI[%p]\tTD(%p->%p) dir=%s cc=%s errc=%d t=%d rd=%s: c=0x%08x cbp=0x%08x, next=0x%08x, bend=0x%08x",
		   this,
		   pTD, (void*)pTD->pPhysical,
		   dir == 0 ? "SETUP" : (dir==2?"IN":"OUT"),
		   cc_errors[err],
		   (uint32_t)(w0 & kOHCIGTDControl_EC) >> kOHCIGTDControl_ECPhase,
		   (uint32_t)(w0 & kOHCIGTDControl_T)  >> kOHCIGTDControl_TPhase,
		   (w0 & kOHCIGTDControl_R)?"yes":"no",
		   USBToHostLong(pTD->pShared->ohciFlags),
		   USBToHostLong(pTD->pShared->currentBufferPtr),
		   USBToHostLong(pTD->pShared->nextTD),
		   USBToHostLong(pTD->pShared->bufferEnd));
}



void 
AppleUSBOHCI::print_itd(AppleOHCIIsochTransferDescriptorPtr pTD, int level)
{
    UInt32 w0, err;
    int i;
    if (pTD == 0) return;

    w0 = USBToHostLong(pTD->pShared->flags);
    err = (w0 & kOHCIITDControl_CC) >> kOHCIITDControl_CCPhase;
    USBLog(level, "AppleUSBOHCI[%p]\tTD(%p->%p) cc=%s fc=%d sf=0x%x c=0x%08x bp0=0x%08x, bend=0x%08x, next=0x%08x",
           this,
           pTD, (void *)pTD->pPhysical,
           cc_errors[err],
           (uint32_t)(w0 & kOHCIITDControl_FC) >> kOHCIITDControl_FCPhase,
           (uint32_t)(w0 & kOHCIITDControl_SF)  >> kOHCIITDControl_SFPhase,
           (uint32_t)w0,
           USBToHostLong(pTD->pShared->bufferPage0),
           USBToHostLong(pTD->pShared->bufferEnd),
           USBToHostLong(pTD->pShared->nextTD)
           );
    for(i=0; i<8; i++)
    {
        USBLog(level, "Offset/PSW %d = 0x%x", i, USBToHostWord(pTD->pShared->offset[i]));
    }
    USBLog(level, "frames =%p, FrameNumber %d", (void *)pTD->pIsocFrame, (uint32_t)pTD->frameNum);
}



void 
AppleUSBOHCI::print_ed(AppleOHCIEndpointDescriptorPtr pED, int level)
{
    AppleOHCIGeneralTransferDescriptorPtr	pTD;
    UInt32 w0;


    if (pED == 0) 
	{
		//kprintf("Null ED\n");
		return;
    }
    w0 = USBToHostLong(pED->pShared->flags);

    if ((w0 & kOHCIEDControl_K) == 0 /*noskip*/)
    {
        USBLog(level, "AppleUSBOHCI[%p] ED(%p->%p) %d:%d d=%d s=%s sk=%s i=%s max=%d : c=0x%08x tail=0x%08x, head=0x%08x, next=0x%08x",
              this, 
              pED, (void *)pED->pPhysical,
              (uint32_t)(w0 & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase,
              (uint32_t)(w0 & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase,
              (uint32_t)(w0 & kOHCIEDControl_D)  >> kOHCIEDControl_DPhase,
              w0 & kOHCIEDControl_S?"low":"full",
              w0 & kOHCIEDControl_K?"yes":"no",
              w0 & kOHCIEDControl_F?"yes":"no",
              (uint32_t)(w0 & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase,
              USBToHostLong(pED->pShared->flags),
              USBToHostLong(pED->pShared->tdQueueTailPtr),
              USBToHostLong(pED->pShared->tdQueueHeadPtr),
              USBToHostLong(pED->pShared->nextED));

        //pTD = (AppleOHCIGeneralTransferDescriptorPtr) pED->pVirtualHeadP;
       pTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCINextEndpointDescriptor_nextED);
       while (pTD != 0)
        {
            // DEBUGLOG("\t");
            printTD(pTD, level);
            pTD = pTD->pLogicalNext;
        }
    }
	else 
	{
		USBLog(level,"AppleUSBOHCI[%p]::print_ed  skipping %p because it K bit set", this, pED);
	}

}



void 
AppleUSBOHCI::print_isoc_ed(AppleOHCIEndpointDescriptorPtr pED, int level, bool printSkipped, bool printTDs)
{
#pragma unused (printSkipped, printTDs)
   AppleOHCIIsochTransferDescriptorPtr	pTD;
    UInt32 w0;


    if (pED == 0) {
        kprintf("Null ED\n");
        return;
    }
    w0 = USBToHostLong(pED->pShared->flags);

    if ((w0 & kOHCIEDControl_K) == 0 /*noskip*/)
    {
        USBLog(level, "AppleUSBOHCI[%p] ED(0x%p->%p) %d:%d d=%d s=%s sk=%s i=%s max=%d : c=0x%08x tail=0x%08x, head=0x%08x, next=0x%08x",
              this,
              pED, (void *)pED->pPhysical,
              (uint32_t)(w0 & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase,
              (uint32_t)(w0 & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase,
              (uint32_t)(w0 & kOHCIEDControl_D)  >> kOHCIEDControl_DPhase,
              w0 & kOHCIEDControl_S?"low":"hi",
              w0 & kOHCIEDControl_K?"yes":"no",
              w0 & kOHCIEDControl_F?"yes":"no",
              (uint32_t)(w0 & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase,
              USBToHostLong(pED->pShared->flags),
              USBToHostLong(pED->pShared->tdQueueTailPtr),
              USBToHostLong(pED->pShared->tdQueueHeadPtr),
              USBToHostLong(pED->pShared->nextED));

        pTD = (AppleOHCIIsochTransferDescriptorPtr) pED->pLogicalHeadP;
        while (pTD != 0)
        {
           //  DEBUGLOG("\t");
            print_itd(pTD, level);
            pTD = pTD->pLogicalNext;
        }
    }
}



void 
AppleUSBOHCI::print_list(AppleOHCIEndpointDescriptorPtr pListHead, AppleOHCIEndpointDescriptorPtr pListTail, int level, bool printSkipped, bool printTDs)
{
    AppleOHCIEndpointDescriptorPtr		pED, pEDTail;

    USBLog(level, "AppleUSBOHCI[%p]:print_list  Head: %p, Tail: %p", this, pListHead, pListTail);

    pED = (AppleOHCIEndpointDescriptorPtr) pListHead;
    pEDTail = (AppleOHCIEndpointDescriptorPtr) pListTail;

    while (pED != pEDTail)
    {
		AppleOHCIGeneralTransferDescriptorPtr	pTD;

		if ((USBToHostLong(pED->pShared->flags) & kOHCIEDControl_K) == 0 || printSkipped)
			printED(pED, level);
 
		if ( printTDs )
		{
			// get the top TD
			pTD = (AppleOHCIGeneralTransferDescriptorPtr) (USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);
			// convert physical to logical
			pTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical((IOPhysicalAddress)pTD);
			if ( pTD && !(pTD == pED->pLogicalTailP) )
			{
				printTD(pTD, level);
				while ( pTD && pTD->pLogicalNext != NULL )
				{
					printTD(pTD->pLogicalNext, level);
					pTD = pTD->pLogicalNext;
				}
			}
		}
		
		pED = pED->pLogicalNext;
    }
	if ((USBToHostLong(pED->pShared->flags) & kOHCIEDControl_K) == 0 || printSkipped)
		printED(pEDTail, level);
}



void 
AppleUSBOHCI::print_control_list(int level, bool printSkipped, bool printTDs)
{
    USBLog(level, "AppleUSBOHCI[%p] Control List: h/w head = 0x%x", this, USBToHostLong(_pOHCIRegisters->hcControlHeadED));
    print_list(_pControlHead, _pControlTail, level, printSkipped, printTDs);
}



void 
AppleUSBOHCI::print_bulk_list(int level, bool printSkipped, bool printTDs)
{
    USBLog(level, "AppleUSBOHCI[%p] Bulk List: h/w head = 0x%x", this, USBToHostLong(_pOHCIRegisters->hcBulkHeadED));
    print_list((AppleOHCIEndpointDescriptorPtr) _pBulkHead, (AppleOHCIEndpointDescriptorPtr) _pBulkTail,level, printSkipped, printTDs);
}



void 
AppleUSBOHCI::print_int_list(int level, bool printSkipped, bool printTDs)
{
    int				i;
    UInt32			w0;
    AppleOHCIEndpointDescriptorPtr 	pED;


    for (i = 0; i <= 63; i++)
    {
        pED = _pInterruptHead[i].pHead->pLogicalNext;

		w0 = USBToHostLong(pED->pShared->flags);
		
		if ((USBToHostLong(pED->pShared->flags) & kOHCIEDControl_K) == 0 || printSkipped)
		{
			USBLog(level, "AppleUSBOHCI[%p]Interrupt List  level[%d]:", this, i);
			printED(pED, level);
			
			if ( printTDs )
			{
				AppleOHCIGeneralTransferDescriptorPtr	pTD;

				// get the top TD
				pTD = (AppleOHCIGeneralTransferDescriptorPtr) (USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);
				// convert physical to logical
				pTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical((IOPhysicalAddress)pTD);
				if ( pTD && !(pTD == pED->pLogicalTailP) )
				{
					printTD(pTD, level);
					while ( pTD && pTD->pLogicalNext != NULL )
					{
						printTD(pTD->pLogicalNext, level);
						pTD = pTD->pLogicalNext;
					}
				}
			}
			
		}
    }
}

#pragma mark Timeout Checks
#define	kOHCIUIMScratchFirstActiveFrame	0

void
AppleUSBOHCI::CheckEDListForTimeouts(AppleOHCIEndpointDescriptorPtr head, AppleOHCIEndpointDescriptorPtr tail)
{
    AppleOHCIEndpointDescriptorPtr		pED = head;
    AppleOHCIGeneralTransferDescriptorPtr	pTD;

    UInt32 				noDataTimeout;
    UInt32				completionTimeout;
    UInt32				rem;
    UInt32				curFrame = GetFrameNumber32();

	if (curFrame == 0)
		return;
	
    for (pED = pED->pLogicalNext; pED != tail; pED = pED->pLogicalNext)
    {
        // get the top TD
        pTD = (AppleOHCIGeneralTransferDescriptorPtr) (USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);
        // convert physical to logical
        pTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical((IOPhysicalAddress)pTD);
        if (!pTD)
            continue;
        if (pTD == pED->pLogicalTailP)
            continue;
        if (!pTD->command)
            continue;

        noDataTimeout = pTD->command->GetNoDataTimeout();
        completionTimeout = pTD->command->GetCompletionTimeout();

        if (completionTimeout)
        {
            UInt32	firstActiveFrame = pTD->command->GetUIMScratch(kOHCIUIMScratchFirstActiveFrame);
            if (!firstActiveFrame)
            {
                pTD->command->SetUIMScratch(kOHCIUIMScratchFirstActiveFrame, curFrame);
                continue;
            }
            if ((curFrame - firstActiveFrame) >= completionTimeout)
            {
				uint32_t	myFlags = USBToHostLong( pED->pShared->flags);
                USBLog(2, "AppleUSBOHCI[%p]::Found a transaction past the completion deadline, timing out! (%p, 0x%x - 0x%x)", this, pTD, (uint32_t)curFrame, (uint32_t)firstActiveFrame);
				USBError(1, "AppleUSBOHCI[%p]::Found a transaction past the completion deadline on bus 0x%x, timing out! (Addr: %d, EP: %d)", this, (uint32_t) _busNumber, ((myFlags & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase), ((myFlags & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase) );
               
				ReturnOneTransaction(pTD, pED, kIOUSBTransactionTimeout);
                continue;
            }
        }

        if (!noDataTimeout)
            continue;

        if (!pTD->lastFrame || (pTD->lastFrame > curFrame))
        {
            // this pTD is not a candidate yet, remember the frame number and go on
            pTD->lastFrame = curFrame;
            pTD->lastRemaining = findBufferRemaining(pTD);
            continue;
        }
        rem = findBufferRemaining(pTD);
        if (pTD->lastRemaining != rem)
        {
            // there has been some activity on this TD. update and move on
            pTD->lastRemaining = rem;
            continue;
        }
        if ((curFrame - pTD->lastFrame) >= noDataTimeout)
        {
			uint32_t	myFlags = USBToHostLong( pED->pShared->flags); 
            USBLog(2, "AppleUSBOHCI[%p]::Found a transaction which hasn't moved in 5 seconds, timing out! (%p, 0x%x - 0x%x)", this, pTD, (uint32_t)curFrame, (uint32_t)pTD->lastFrame);
			USBError(1, "AppleUSBOHCI[%p]::Found a transaction which hasn't moved in 5 seconds on bus 0x%x, timing out! (Addr: %d, EP: %d)", this, (uint32_t) _busNumber, ((myFlags & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase), ((myFlags & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase) );
			
            ReturnOneTransaction(pTD, pED, kIOUSBTransactionTimeout);
            continue;
        }
    }
}

void
AppleUSBOHCI::ReturnAllTransactionsInEndpoint(AppleOHCIEndpointDescriptorPtr head, AppleOHCIEndpointDescriptorPtr tail)
{
    AppleOHCIEndpointDescriptorPtr		pED = head;
    AppleOHCIGeneralTransferDescriptorPtr	pTD;

    UInt32				rem;

    if (head == NULL)
         return;
         
    for (pED = pED->pLogicalNext; pED != tail; pED = pED->pLogicalNext)
    {
        // get the top TD
        pTD = (AppleOHCIGeneralTransferDescriptorPtr) (USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);
        // convert physical to logical
        pTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical((IOPhysicalAddress)pTD);
        if (!pTD)
            continue;
        if (pTD == pED->pLogicalTailP)
            continue;
        if (!pTD->command)
            continue;

        ReturnOneTransaction(pTD, pED, kIOReturnNotResponding);
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
AppleUSBOHCI::UIMCheckForTimeouts(void)
{
    AbsoluteTime	currentTime;
    AbsoluteTime	lastRootHubChangeTime;
    UInt64			elapsedTime = 0;
    bool			allPortsDisconnected = false;
	
    // If we are not active anymore or if we're in ohciBusStateOff, then don't check for timeouts 
    //
    if ( isInactive() || !_controllerAvailable || (_myBusState != kUSBBusStateRunning))
	{
		USBLog(7,"AppleUSBOHCI[%p]  UIMCheckForTimeouts for bus %d -- not appropriate", this, (uint32_t)_busNumber);
        return;
	}
    
	
    // Check to see if our control or bulk lists have a TD that has timed out
    //
    CheckEDListForTimeouts(_pControlHead, _pControlTail);
    CheckEDListForTimeouts(_pBulkHead, _pBulkTail);

     // From OS9:  Ferg 1-29-01
    // some controllers can be swamped by PCI traffic and essentially go dead.  
    // here we attempt to detect this condition and recover from it.
    //
    if ( _errataBits & kErrataNeedsWatchdogTimer ) 
    {
        UInt16 			hccaFrameNumber, hcFrameNumber;
        UInt32			fmInterval, hcca, bulkHead, controlHead, periodicStart, intEnable, fmNumber;
        
		// this should be done by the new power manager code
		
        hcFrameNumber = (UInt16) USBToHostLong(_pOHCIRegisters->hcFmNumber);  // check this first in case an interrupt delays the second read
        hccaFrameNumber = (UInt16) USBToHostLong(*(UInt32 *)(_pHCCA + 0x80));
        
        if ( (hcFrameNumber > 5) && (hcFrameNumber > (hccaFrameNumber+5)) )
        {
            USBError(1,"AppleUSBOHCI[%p] Watchdog detected dead controller (hcca #: %d, hc #: %d)", this,  (uint32_t) hccaFrameNumber, (uint32_t) hcFrameNumber);
                    
            // Save registers
            //
            fmInterval = _pOHCIRegisters->hcFmInterval;
            hcca = _pOHCIRegisters->hcHCCA;
            bulkHead = _pOHCIRegisters->hcBulkHeadED;
            controlHead = _pOHCIRegisters->hcControlHeadED;
            periodicStart = _pOHCIRegisters->hcPeriodicStart;
            intEnable = _pOHCIRegisters->hcInterruptEnable;
            fmNumber = _pOHCIRegisters->hcFmNumber;
            
            _pOHCIRegisters->hcCommandStatus = HostToUSBLong(kOHCIHcCommandStatus_HCR);  // Reset OHCI 
            IOSleep(3);
            
            // Restore registers
            //
            _pOHCIRegisters->hcFmNumber = fmNumber;
            _pOHCIRegisters->hcInterruptEnable = intEnable;
            _pOHCIRegisters->hcPeriodicStart = periodicStart;
            _pOHCIRegisters->hcBulkHeadED = bulkHead;
            _pOHCIRegisters->hcControlHeadED = controlHead;
            _pOHCIRegisters->hcHCCA = hcca;
            _pOHCIRegisters->hcFmInterval = fmInterval;
            IOSync();
			// i did not put the NEC incomplete write check in here
			// because this is the _needsWatchdog errata and they are
			// two different errata bits
						
            _pOHCIRegisters->hcControl = HostToUSBLong(kOHCIFunctionalState_Resume << kOHCIHcControl_HCFSPhase);
            
            if (_errataBits & kErrataLucentSuspendResume)
            {
                // JRH 08-27-99
                // this is a very simple yet clever hack for working around a bug in the Lucent controller
                // By using 35 instead of 20, we overflow an internal 5 bit counter by exactly 3ms, which 
                // stops an errant 3ms suspend from appearing on the bus
                //
                IOSleep(35);
            }
            else
            {
                IOSleep(20);
            }
            
            // Turn back on all the processing
            //
            _pOHCIRegisters->hcControl = HostToUSBLong(kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase);
            
            // Wait the required 3 ms before turning on the lists
            //
            IOSleep(3);			
            
            _pOHCIRegisters->hcControl =  HostToUSBLong((kOHCIFunctionalState_Operational << kOHCIHcControl_HCFSPhase)
                                                | kOHCIHcControl_CLE | (_OptiOn ? kOHCIHcControl_Zero : kOHCIHcControl_BLE) 
                                                | kOHCIHcControl_PLE | kOHCIHcControl_IE);
        }
    }
}



IOReturn 
AppleUSBOHCI::UIMCreateIsochTransfer(short						functionAddress,
									 short						endpointNumber,
									 IOUSBIsocCompletion		completion,
									 UInt8						direction,
									 UInt64						frameNumberStart,
									 IOMemoryDescriptor *		pBuffer,
									 UInt32						frameCount,
									 IOUSBLowLatencyIsocFrame *	pFrames,
									 UInt32						updateFrequency)
{
#pragma unused (functionAddress, endpointNumber, completion, frameNumberStart, frameNumberStart, pBuffer, direction, frameCount, pFrames, updateFrequency)
	USBError(1, "AppleUSBOHCI::UIMCreateIsochTransfer(LL) - old method");
	return kIOReturnIPCError;
}



IOReturn
AppleUSBOHCI::UIMCreateIsochTransfer(IOUSBIsocCommand *command)
{
	UInt8										direction = command->GetDirection();
	USBDeviceAddress							functionAddress = command->GetAddress();
	UInt8										endpointNumber = command->GetEndpoint();
	IOUSBIsocCompletion							completion = command->GetUSLCompletion();
	UInt64										frameNumberStart = command->GetStartFrame();
	IOMemoryDescriptor *						pBuffer = command->GetBuffer();
	UInt32										frameCount = command->GetNumFrames();
	IOUSBIsocFrame *							pFrames = command->GetFrameList();
	IOUSBLowLatencyIsocFrame *					pLLFrames = (IOUSBLowLatencyIsocFrame *)pFrames;
	UInt32										updateFrequency = command->GetUpdateFrequency();
	bool										requestFromRosettaClient = command->GetIsRosettaClient();
	bool										lowLatency = command->GetLowLatency();
	IODMACommand *								dmaCommand = command->GetDMACommand();
    IOReturn									status = kIOReturnSuccess;
    AppleOHCIIsochTransferDescriptorPtr			pTailITD = NULL;
    AppleOHCIIsochTransferDescriptorPtr			pNewITD = NULL;
    AppleOHCIIsochTransferDescriptorPtr			pTempITD = NULL;
    UInt32										i;
    UInt32										curFrameInRequest = 0;
    UInt32										bufferSize = 0;
    UInt32										pageOffset = 0;
    UInt32										segmentEnd = 0;
    UInt32										lastPhysical = 0;
    AppleOHCIEndpointDescriptorPtr				pED;
    UInt32										curFrameInTD = 0;
    UInt16										frameNumber = (UInt16) frameNumberStart;
    UInt64										curFrameNumber = GetFrameNumber();
    UInt64										frameDiff;
    UInt64										maxOffset = (UInt64)(0x00007FF0);
    UInt32										diff32;
	
    UInt32										itdFlags = 0;
    UInt32										numSegs = 0;
    UInt32										physPageStart = 0;
    UInt32										prevFramesPage = 0;
    UInt32										physPageEnd = 0;
    UInt32										pageSelectMask = 0;
    bool										needNewITD;
    bool										multiPageSegment = false;
    UInt32										tdType;
    IOByteCount									transferOffset;
    bool										useUpdateFrequency = true;
	UInt64										offset;
	IODMACommand::Segment64						segments64[2];
	IODMACommand::Segment32						segments32[2];
	UInt32										edFlags;
	UInt32										maxPacketSize;
	
    if ( (frameCount == 0) || (frameCount > 1000) )
    {
        USBLog(3,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer bad frameCount: %d", this, (uint32_t)frameCount);
        return kIOReturnBadArgument;
    }
	
	if (curFrameNumber == 0)
		return kIOReturnInternalError;
	
    if (direction == kUSBOut) 
	{
        direction = kOHCIEDDirectionOut;
        tdType = lowLatency ? kOHCIIsochronousOutLowLatencyType : kOHCIIsochronousOutType;
    }
    else if (direction == kUSBIn) 
	{
        direction = kOHCIEDDirectionIn;
        tdType = lowLatency ? kOHCIIsochronousInLowLatencyType : kOHCIIsochronousInType;
    }
    else
        return kIOReturnInternalError;
	
	if (!dmaCommand)
	{
        USBError(1,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer no dmaCommand", this);
        return kIOReturnInternalError;
	}
	
	if (dmaCommand->getMemoryDescriptor() != pBuffer)
	{
        USBError(1,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer - memory desc in dmaCommand (%p) different than IOMD (%p)", this, dmaCommand->getMemoryDescriptor(), pBuffer);
        return kIOReturnInternalError;
	}
	
    pED = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);
	
    if (!pED)
    {
        USBLog(3,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer endpoint (%d) not found. Returning 0x%x", this, endpointNumber, kIOUSBEndpointNotFound);
        return kIOUSBEndpointNotFound;
    }
	
	edFlags = USBToHostLong(pED->pShared->flags);
	maxPacketSize = ( edFlags & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase;
    
    if ( lowLatency && (updateFrequency == 0))
        useUpdateFrequency = false;
	
    if (frameNumberStart <= curFrameNumber)
    {
        if (frameNumberStart < (curFrameNumber - maxOffset))
        {
            USBLog(3,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer request frame WAY too old.  frameNumberStart: %d, curFrameNumber: %d.  Returning 0x%x", this, (uint32_t) frameNumberStart, (uint32_t) curFrameNumber, kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
		USBLog(6,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %d, curFrameNumber: %d.  USBIsocFrame Ptr: %p, First ITD: %p", this, (uint32_t) frameNumberStart, (uint32_t) curFrameNumber, pFrames, pED->pLogicalTailP);
    } else 
    {	
        // frameNumberStart > curFrameNumber
        //
        if (frameNumberStart > (curFrameNumber + maxOffset))
        {
            USBLog(3,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer request frame too far ahead!  frameNumberStart: %d, curFrameNumber: %d, Returning 0x%x", this, (uint32_t) frameNumberStart, (uint32_t) curFrameNumber, kIOReturnIsoTooNew);
            return kIOReturnIsoTooNew;
        }
        
        // Check to see how far in advance the frame is scheduled
        frameDiff = frameNumberStart - curFrameNumber;
        diff32 = (UInt32)frameDiff;
        if (diff32 < 2)
        {
            USBLog(5,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer WARNING! - frameNumberStart less than 2 ms (is %d)!  frameNumberStart: %d, curFrameNumber: %d", this, (uint32_t) diff32, (uint32_t) frameNumberStart, (uint32_t) curFrameNumber);
        }
    }
	
    //
    //  Get the total size of buffer
    //
    for ( i = 0; i< frameCount; i++)
    {
        if ((lowLatency ? pLLFrames[i].frReqCount : pFrames[i].frReqCount) > maxPacketSize)
        {
            USBLog(3,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer Isoch frame (%d) too big %d", this, (uint32_t)(i + 1), (lowLatency ? pLLFrames[i].frReqCount : pFrames[i].frReqCount));
            return kIOReturnBadArgument;
        }
        bufferSize += (lowLatency ? pLLFrames[i].frReqCount : pFrames[i].frReqCount);
        
		// Make sure our frStatus field has a known value.  This is used by the client to know whether the transfer has been completed or not
		//
		if (lowLatency)
			pLLFrames[i].frStatus = requestFromRosettaClient ? (IOReturn) OSSwapInt32(kUSBLowLatencyIsochTransferKey) : (IOReturn) kUSBLowLatencyIsochTransferKey;
		else
			pFrames[i].frStatus = requestFromRosettaClient ? (IOReturn) OSSwapInt32(kUSBLowLatencyIsochTransferKey) : (IOReturn) kUSBLowLatencyIsochTransferKey;
    }
	
    USBLog(7,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer transfer %s, buffer: %p, length: %d frames: %d, updateFreq: %d", this, (direction == kOHCIEDDirectionIn) ? "in" : "out", pBuffer, (uint32_t)bufferSize, (uint32_t)frameCount, (uint32_t)updateFrequency);
	
    //
    // go ahead and make sure we can grab at least ONE TD, before we lock the buffer	
    //
    pNewITD = AllocateITD();
    USBLog(7, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - new iTD %p", this, pNewITD);
    if (pNewITD == NULL)
    {
        USBLog(1,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer Could not allocate a new iTD", this);
		USBTrace( kUSBTOHCI, kTPOHCICreateIsochTransfer, (uint32_t)bufferSize, (uint32_t)frameCount, (uint32_t)updateFrequency, kIOReturnNoMemory );
        return kIOReturnNoMemory;
    }
	
    if (!bufferSize) 
    {
		// Set up suitable dummy info
        numSegs = 1;
        segments32[0].fIOVMAddr = segments32[0].fLength = 0;
		pageOffset = 0;
    }
    
    pTailITD = (AppleOHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;	// start with the unused TD on the tail of the list
    OSWriteLittleInt32(&pTailITD->pShared->nextTD, 0, pNewITD->pPhysical);	// link in the new ITD
    pTailITD->pLogicalNext = pNewITD;
	
    needNewITD = false;
    transferOffset = 0;
    while (curFrameInRequest < frameCount) 
    {
		UInt16		thisFrameRequest = (lowLatency ? pLLFrames[curFrameInRequest].frReqCount : pFrames[curFrameInRequest].frReqCount);
		
        // Get physical segments for next frame
        if (!needNewITD && bufferSize && (thisFrameRequest != 0) ) 
		{
			numSegs = 2;
			offset = transferOffset;
			
			USBLog(7, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - calling gen64IOVMSegments - transferOffset (%d) offset (%qd) thisFrameRequest (%d)", this, (int)transferOffset, offset, thisFrameRequest);
			status = dmaCommand->gen64IOVMSegments(&offset, segments64, &numSegs);

			if (status)
			{
				USBError(1, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - curFrameInRequest[%d] frameCount[%d] - got status (%p) from gen64IOVMSegments", this, (int)curFrameInRequest, (int)frameCount, (void*)status);
				return status;
			}
			
			if (numSegs == 2)
			{
				USBLog(7, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - curFrameInRequest[%d] frameCount[%d] - after gen64IOVMSegments, offset (%qd) numSegs (%d) segments64[0].fIOVMAddr (0x%qx) segments64[0].fLength (0x%qx) segments64[1].fIOVMAddr (0x%qx) segments64[1].fLength (0x%qx)", this, (int)curFrameInRequest, (int)frameCount, offset, (int)numSegs, segments64[0].fIOVMAddr, segments64[0].fLength, segments64[1].fIOVMAddr, segments64[1].fLength);
			}
			else
			{
				USBLog(7, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - curFrameInRequest[%d] frameCount[%d] - after gen64IOVMSegments, offset (%qd) numSegs (%d) segments64[0].fIOVMAddr (0x%qx) segments64[0].fLength (0x%qx)", this, (int)curFrameInRequest, (int)frameCount, offset, (int)numSegs, segments64[0].fIOVMAddr, segments64[0].fLength);
			}

			for (i=0; i< numSegs; i++)
			{
				if (((UInt32)(segments64[i].fIOVMAddr >> 32) > 0) || ((UInt32)(segments64[i].fLength >> 32) > 0))
				{
					USBError(1, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - generated segments (%d) not 32 bit -  offset (0x%qx) length (0x%qx) ", this, (int)i, segments64[0].fIOVMAddr, segments64[0].fLength);
					return kIOReturnInternalError;
				}
				// OK to convert to 32 bit (which it should have been already)
				segments32[i].fIOVMAddr = (UInt32)segments64[i].fIOVMAddr;
				segments32[i].fLength = (UInt32)segments64[i].fLength;
			}

			if (segments32[0].fLength >= thisFrameRequest)
			{
				segments32[0].fLength = thisFrameRequest;
				numSegs = 1;
			}
			else if ((numSegs == 2) && ((thisFrameRequest - segments32[0].fLength) < segments32[1].fLength))
			{
				segments32[1].fLength = thisFrameRequest - segments32[0].fLength;
			}

            pageOffset = segments32[0].fIOVMAddr & kOHCIPageOffsetMask;
			USBLog(7, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - adding segment 0 length (%d) to transferOffset", this, (int)segments32[0].fLength);
            transferOffset += segments32[0].fLength;
            segmentEnd = (segments32[0].fIOVMAddr + segments32[0].fLength )  & kOHCIPageOffsetMask;
			
            USBLog(8,"curFrameInRequest: %d, curFrameInTD: %d, pageOffset: 0x%x, numSegs: %d, seg[0].location: 0x%x, seg[0].length: %d", (uint32_t)curFrameInRequest, (uint32_t)curFrameInTD, (uint32_t)pageOffset, (uint32_t)numSegs, (uint32_t)segments32[0].fIOVMAddr, (uint32_t)segments32[0].fLength);
			
            if (numSegs == 2)
            {
				USBLog(7, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - adding segment 1 length (%d) to transferOffset", this, (int)segments32[1].fLength);
                transferOffset += segments32[1].fLength;
                USBLog(8 ,"seg[1].location: 0x%x, seg[1].length %d", (uint32_t)segments32[1].fIOVMAddr, (uint32_t)segments32[1].fLength);
                
                // If we are wrapping around the same physical page and we are on an NEC controller, then we need to discard the 2nd segment.  It will click
                // but at least we won't hang the controller
                //
                if ( (_errataBits & kErrataNECOHCIIsochWraparound) && ((segments32[0].fIOVMAddr & kOHCIPageMask) == (segments32[1].fIOVMAddr & kOHCIPageMask)) )
                {
                    USBLog(3,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer On an NEC controller and frame data wraps from end of buffer to beginning.  Dropping data to avoid controller hang", this);
                    numSegs = 1;
                }
            }
			
            if ( (segments32[numSegs-1].fIOVMAddr & kOHCIPageMask) != ((segments32[numSegs-1].fIOVMAddr + segments32[numSegs-1].fLength) & kOHCIPageMask))
            {
                multiPageSegment = true;
                USBLog(8,"We have a segment that crosses a page boundary:  start: %p, length: %p, end: %p, curFrameinTD: %d", (void*)segments32[numSegs-1].fIOVMAddr, (void*)segments32[numSegs-1].fLength, (void*)(segments32[numSegs-1].fIOVMAddr + segments32[numSegs-1].fLength), (int)curFrameInTD);
            }
            else
                multiPageSegment = false;
        }
		
        if (curFrameInTD == 0) 
		{
            // set up counters which get reinitialized with each TD
            physPageStart = segments32[0].fIOVMAddr & kOHCIPageMask;	// for calculating real 13 bit offsets
            pageSelectMask = 0;					// First frame always starts on first page
            needNewITD = false;
			
            // set up the header of the TD - itdFlags will be stored into flags later
            itdFlags = (UInt16)(curFrameInRequest + frameNumber);
            pTailITD->pIsocFrame = (IOUSBIsocFrame *) pFrames;		// so we can get back to our info later
            pTailITD->frameNum = curFrameInRequest;	// our own index into the above array
            pTailITD->pType = tdType;			// So interrupt handler knows TD type.
            OSWriteLittleInt32(&pTailITD->pShared->bufferPage0, 0,  physPageStart);
        }
        else if ((segments32[0].fIOVMAddr & kOHCIPageMask) != physPageStart) 
		{
            // pageSelectMask is set if we've already used our one allowed page cross.
            //
            if ( (pageSelectMask && (((segments32[0].fIOVMAddr & kOHCIPageMask) != physPageEnd) || numSegs == 2)) )
			{
                // Need new ITD for this condition
                needNewITD = true;
				
                USBLog(8, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - got it! (%d, 0x%x, 0x%x, %d)", this, (uint32_t)pageSelectMask, ((uint32_t)segments32[0].fIOVMAddr) & kOHCIPageMask, (uint32_t)physPageEnd, (uint32_t)numSegs);
                
            }
            else if ( pageSelectMask && multiPageSegment )
            {
                // We have already crossed one page and we have a segment that spans 2 or more pages
                //
                needNewITD = true;
                USBLog(8,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer This frame spans 2 or more pages and we already used our page crossing ", this);
            }
            else if ( (prevFramesPage != (segments32[0].fIOVMAddr & kOHCIPageMask)) && (segmentEnd != 0) )
            {
                // We have a segment that starts in a new page but the previous one did not end
                // on a page boundary.  Need a new ITD for this condition. 
                // Need new ITD for this condition
                needNewITD = true;
                
                USBLog(8,"AppleUSBOHCI[%p]::UIMCreateIsochTransfer This frame starts on a new page and the previous one did NOT end on a page boundary (%d)", this, (uint32_t)segmentEnd);
            }
            else
			{
                if (pageSelectMask == 0 )
				{
                    USBLog(8,"Using our page crossing for this TD (0x%x)", (uint32_t)(segments32[numSegs-1].fIOVMAddr + segments32[numSegs-1].fIOVMAddr -1 ) & kOHCIPageMask);
				}
				
                pageSelectMask = kOHCIPageSize;	// ie. set bit 13
                physPageEnd = (segments32[numSegs-1].fIOVMAddr  + segments32[numSegs-1].fLength) & kOHCIPageMask;
            }
        }
        
        // Save this frame's Page so that we can use it when the next frame is process to compare and see
        // if they are different
        //
        prevFramesPage = (segments32[numSegs-1].fIOVMAddr  + segments32[numSegs-1].fLength) & kOHCIPageMask;
        
        if ( (curFrameInTD > 7) || needNewITD || (lowLatency && useUpdateFrequency && (curFrameInTD >= updateFrequency)) ) 
		{
            // Need to start a new TD
            //
            needNewITD = true;	// To simplify test at top of loop.
            itdFlags |= (curFrameInTD-1) << kOHCIITDControl_FCPhase;
            OSWriteLittleInt32(&pTailITD->pShared->bufferEnd, 0, lastPhysical);
            curFrameInTD = 0;
            pNewITD = AllocateITD();
            USBLog(7, "AppleUSBOHCI[%p]::UIMCreateIsochTransfer - new iTD %p (curFrameInRequest: %d, curFrameInTD: %d, needNewITD: %d, updateFrequency: %d", this, pNewITD, (uint32_t)curFrameInRequest, (uint32_t)curFrameInTD, needNewITD, (uint32_t)updateFrequency);
            if (pNewITD == NULL) 
			{
                status = kIOReturnNoMemory;
				break;
            }
            // Handy for debugging transfer lists
            itdFlags |= (kOHCIGTDConditionNotAccessed << kOHCIGTDControl_CCPhase);
            
            // In the past, we set the DI bits to 111b at this point, so that we only interrupted in the last transfer.  However, this presents a problem
            // for low latency in the case of a transfer having multiple TDs:  When we abort a pipe, there might be a partial transfer 
            // in the done queue that has not caused an interrupt.  If we then unmap the frame list (because our user client died), then
            // when another TD completes, we will process this orphaned low latency TD and hang/panic 'cause we'll try to access the unmapped memory.
            // Hence, we need to make sure that we interrupt after every TD.
			
			
            OSWriteLittleInt32(&pTailITD->pShared->flags, 0, itdFlags);
			
            pTailITD->completion.action = NULL;
			pTailITD->requestFromRosettaClient = requestFromRosettaClient;
			
            //print_itd(pTailITD);
            pTailITD = pTailITD->pLogicalNext;		// this is the "old" pNewTD
			if (pTailITD == NULL )
				break;
			
            OSWriteLittleInt32(&pTailITD->pShared->nextTD, 0, pNewITD->pPhysical);	// link to the "new" pNewTD
            pTailITD->pLogicalNext = pNewITD;
            continue;		// start over
        }
		
        // At this point we know we have a frame which will fit into the current TD.
        // calculate the buffer offset for the beginning of this frame
        //
        OSWriteLittleInt16(&pTailITD->pShared->offset[curFrameInTD], 0,
						   pageOffset |							// offset
						   pageSelectMask |						// offset from BP0 or BufferEnd
						   (kOHCIITDOffsetConditionNotAccessed << kOHCIITDOffset_CCPhase)); 	// mark as unused
						   	
		
        // adjust counters and calculate the physical offset of the end of the frame for the next time around the loop
        //
        curFrameInRequest++;
        curFrameInTD++;
        lastPhysical = segments32[numSegs-1].fIOVMAddr + segments32[numSegs-1].fLength - 1;
    }			
	
    if (status != kIOReturnSuccess)
    {
        // unlink the TDs, unlock the buffer, and return the status
        pNewITD = pTailITD->pLogicalNext;	// point to the "old" pNewTD, which will also get deallocated
        pTempITD = (AppleOHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;
        pTailITD = pTempITD->pLogicalNext;	// don't deallocate the real tail!
        pTempITD->pLogicalNext = NULL;		// just to make sure
        pTempITD->pShared->nextTD = NULL;	// just to make sure
        while (pTailITD != pNewITD)
        {
            pTempITD = pTailITD;
            pTailITD = pTailITD->pLogicalNext;
            DeallocateITD(pTempITD);
        }
    }
    else
    {
        // we have good status, so let's kick off the machine
        // we need to tidy up the last TD, which is not yet complete
        itdFlags |= (curFrameInTD-1) << kOHCIITDControl_FCPhase;
		
        OSWriteLittleInt32(&pTailITD->pShared->flags, 0, itdFlags);
        OSWriteLittleInt32(&pTailITD->pShared->bufferEnd, 0, lastPhysical);
        pTailITD->completion = completion;
		pTailITD->requestFromRosettaClient = requestFromRosettaClient;
        
        //print_itd(pTailITD);
        // Make new descriptor the tail
        pED->pLogicalTailP = pNewITD;
        OSWriteLittleInt32(&pED->pShared->tdQueueTailPtr, 0, pNewITD->pPhysical);
    }
	
    return status;
}



// this call is not gated, so we need to gate it ourselves
IOReturn
AppleUSBOHCI::GetFrameNumberWithTime(UInt64* frameNumber, AbsoluteTime *theTime)
{
	if (!_commandGate)
		return kIOReturnUnsupported;
		
	return _commandGate->runAction(GatedGetFrameNumberWithTime, frameNumber, theTime);
}



// here is the gated version
IOReturn
AppleUSBOHCI::GatedGetFrameNumberWithTime(OSObject *owner, void* arg0, void* arg1, void* arg2, void* arg3)
{
#pragma unused (arg2, arg3)
	AppleUSBOHCI		*me = (AppleUSBOHCI*)owner;
	UInt64				*frameNumber = (UInt64*)arg0;
	AbsoluteTime		*theTime = (AbsoluteTime*)arg1;
	
	*frameNumber = me->_anchorFrame;
	*theTime = me->_anchorTime;
	return kIOReturnSuccess;
}


IOReturn
AppleUSBOHCI::UIMEnableAddressEndpoints(USBDeviceAddress address, bool enable)
{
    AppleOHCIEndpointDescriptorPtr	pEDQueue;
	UInt32							edFlags;
	int								i;

	USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints(%d, %s)", this, (int)address, enable ? "true" : "false");
	// look through the lists one at a time - Control first
    pEDQueue = _pControlHead;
    while (pEDQueue != _pControlTail)
    {
		edFlags = USBToHostLong(pEDQueue->pShared->flags);
        if ((edFlags & kOHCIEDControl_FA) == address)
		{
			USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - found control ED[%p] which matches - %s", this, pEDQueue, enable ? "enabling" : "disabling");
			if (enable)
			{
				if (!(edFlags & kOHCISkipped))
				{
					USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - HMMM - it was NOT marked as skipped..", this);
				}
				edFlags &= ~kOHCISkipped;
			}
			else
			{
				if (edFlags & kOHCISkipped)
				{
					USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - HMMM - it was already marked as skipped..", this);
				}
				edFlags |= kOHCISkipped;
			}
			pEDQueue->pShared->flags = HostToUSBLong(edFlags);
			IOSync();
		}
		pEDQueue = (AppleOHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
	}
	
	// now bulk
    pEDQueue = _pBulkHead;
    while (pEDQueue != _pBulkTail)
    {
		edFlags = USBToHostLong(pEDQueue->pShared->flags);
        if ((edFlags & kOHCIEDControl_FA) == address)
		{
			USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - found Bulk ED[%p] which matches - %s", this, pEDQueue, enable ? "enabling" : "disabling");
			if (enable)
			{
				if (!(edFlags & kOHCISkipped))
				{
					USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - HMMM - it was NOT marked as skipped..", this);
				}
				edFlags &= ~kOHCISkipped;
			}
			else
			{
				if (edFlags & kOHCISkipped)
				{
					USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - HMMM - it was already marked as skipped..", this);
				}
				edFlags |= kOHCISkipped;
			}
			pEDQueue->pShared->flags = HostToUSBLong(edFlags);
			IOSync();
		}
		pEDQueue = (AppleOHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
	}
	
	// now interrupt
    for (i = 0; i < 63; i++)
    {
        pEDQueue = _pInterruptHead[i].pHead;
        
        while (pEDQueue != _pInterruptHead[i].pTail)
        {
			edFlags = USBToHostLong(pEDQueue->pShared->flags);

		   if ((edFlags & kOHCIEDControl_FA) == address)
			{
				USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - found Interrupt ED[%p] which matches - %s", this, pEDQueue, enable ? "enabling" : "disabling");
				if (enable)
				{
					if (!(edFlags & kOHCISkipped))
					{
						USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - HMMM - it was NOT marked as skipped..", this);
					}
					edFlags &= ~kOHCISkipped;
				}
				else
				{
					if (edFlags & kOHCISkipped)
					{
						USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAddressEndpoints - HMMM - it was already marked as skipped..", this);
					}
					edFlags |= kOHCISkipped;
				}
				pEDQueue->pShared->flags = HostToUSBLong(edFlags);
				IOSync();
			}
            pEDQueue = pEDQueue->pLogicalNext;
        }
    }
	return kIOReturnSuccess;
}



IOReturn
AppleUSBOHCI::UIMEnableAllEndpoints(bool enable)
{
    AppleOHCIEndpointDescriptorPtr	pEDQueue;
	UInt32							edFlags;
	int								i;

	USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAllEndpoints(%s)", this, enable ? "true" : "false");
	// look through the lists one at a time - Control first
    pEDQueue = _pControlHead;
    while (pEDQueue != _pControlTail)
    {
		edFlags = USBToHostLong(pEDQueue->pShared->flags);
		if ((edFlags & kOHCISkipped) && (edFlags & kOHCIEDControl_FA))
		{
			USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAllEndpoints - found skipped Control ED[%p] for ADDR[%d] ", this, pEDQueue, (int)(edFlags & kOHCIEDControl_FA));
			edFlags &= ~kOHCISkipped;
			pEDQueue->pShared->flags = HostToUSBLong(edFlags);
			IOSync();
		}
		
		pEDQueue = (AppleOHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
	}
    pEDQueue = _pBulkHead;
    while (pEDQueue != _pBulkTail)
    {
		edFlags = USBToHostLong(pEDQueue->pShared->flags);
		if ((edFlags & kOHCISkipped) && (edFlags & kOHCIEDControl_FA))
		{
			USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAllEndpoints - found skipped Bulk ED[%p] for ADDR[%d] ", this, pEDQueue, (int)(edFlags & kOHCIEDControl_FA));
			edFlags &= ~kOHCISkipped;
			pEDQueue->pShared->flags = HostToUSBLong(edFlags);
			IOSync();
		}
		
		pEDQueue = (AppleOHCIEndpointDescriptorPtr) pEDQueue->pLogicalNext;
	}
    for (i = 0; i < 63; i++)
    {
        pEDQueue = _pInterruptHead[i].pHead;
        
        while (pEDQueue != _pInterruptHead[i].pTail)
        {
			edFlags = USBToHostLong(pEDQueue->pShared->flags);
			if ((edFlags & kOHCISkipped) && (edFlags & kOHCIEDControl_FA))
			{
				USBLog(2, "AppleUSBOHCI[%p]::UIMEnableAllEndpoints - found skipped Interrupt ED[%p] for ADDR[%d] ", this, pEDQueue, (int)(edFlags & kOHCIEDControl_FA));
				edFlags &= ~kOHCISkipped;
				pEDQueue->pShared->flags = HostToUSBLong(edFlags);
				IOSync();
			}
            pEDQueue = pEDQueue->pLogicalNext;
        }
    }
	return kIOReturnSuccess;
}	
