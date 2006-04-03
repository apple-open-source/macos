/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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

#define DEBUGGING_LEVEL 0	// 1 = low; 2 = high; 3 = extreme

#define super IOUSBController

static inline OHCIEDFormat
GetEDType(AppleOHCIEndpointDescriptorPtr pED)
{
    return ((USBToHostLong(pED->pShared->flags) & kOHCIEDControl_F) >> kOHCIEDControl_FPhase);
}



IOReturn 
AppleUSBOHCI::CreateGeneralTransfer(AppleOHCIEndpointDescriptorPtr queue, IOUSBCommand* command, IOMemoryDescriptor* CBP, UInt32 bufferSize, UInt32 flags, UInt32 type, UInt32 kickBits)
{
    AppleOHCIGeneralTransferDescriptorPtr	pOHCIGeneralTransferDescriptor,
											newOHCIGeneralTransferDescriptor;
    IOReturn								status = kIOReturnSuccess;
    IOPhysicalSegment						physicalAddresses[2];	
    IOByteCount								transferOffset;
    UInt32									pageSize;
    UInt32									pageCount;
    UInt32									altFlags;		// for all but the final TD
    IOUSBCompletion							completion = command->GetUSLCompletion();

    pageSize = _pageSize;

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
        
        USBError(1, "%s[%p]::CreateGeneralTransfer - trying to queue to a stalled pipe", getName(), this);
        status = kIOUSBPipeStalled;
    }
    else if (bufferSize != 0)
    {
        transferOffset = 0;
        while (transferOffset < bufferSize)
        {
            if(_errataBits & kErrataOnlySinglePageTransfers)
                pageCount = _genCursor->getPhysicalSegments(CBP, transferOffset, physicalAddresses, 1);
            else
                pageCount = _genCursor->getPhysicalSegments(CBP, transferOffset, physicalAddresses, 2);
            newOHCIGeneralTransferDescriptor = AllocateTD();
            if (newOHCIGeneralTransferDescriptor == NULL) 
			{
                status = kIOReturnNoMemory;
                break;
            }
 
			// 3973735 - check to see if we have 2 pages, but we only need 1 to get to bufferSize
			if ((pageCount == 2) && (transferOffset + physicalAddresses[0].length >= bufferSize))
			{
				USBLog(4, "%s[%p]::CreateGeneralTransfer - bufferSize < Descriptor size - adjusting pageCount", getName(), this);
				pageCount = 1;
			}
			
			// if the first segment doesn't end on a page boundary, we will just do that much.
            if ((pageCount == 2) && ((((physicalAddresses[0].location + physicalAddresses[0].length) & (pageSize-1)) != 0) || ((physicalAddresses[1].location & (pageSize-1)) != 0)))
            {
            	pageCount = 1; // we can only do one page here
            	// must be a multiple of max packet size to avoid short packets
            	if (physicalAddresses[0].length % ((USBToHostLong(queue->pShared->flags) & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase) != 0)
            	{
					USBError(1, "%s[%p] CreateGeneralTransfer: non-multiple MPS transfer required -- giving up!", getName(), this);
	                status = kIOReturnNoMemory;
	                break;
            	}
            }
            pOHCIGeneralTransferDescriptor = (AppleOHCIGeneralTransferDescriptorPtr)queue->pLogicalTailP;
            OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->currentBufferPtr, 0, physicalAddresses[0].location);
            OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->nextTD, 0, newOHCIGeneralTransferDescriptor->pPhysical);
            if (pageCount == 2) 
			{
				// check to see if we need to use only part of the 2nd page
				if ((transferOffset + physicalAddresses[0].length + physicalAddresses[1].length) > bufferSize)
				{
					USBLog(4, "%s[%p]::CreateGeneralTransfer - bufferSize < Descriptor size - adjusting physical segment 1", getName(), this);
					physicalAddresses[1].length = bufferSize - (transferOffset + physicalAddresses[0].length);
				}
				OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->bufferEnd, 0, physicalAddresses[1].location + physicalAddresses[1].length - 1);
                transferOffset += physicalAddresses[1].length;
            }
            else
			{
				// need to check to make sure we need all of the 1st (and only) segment
				if ((transferOffset + physicalAddresses[0].length) > bufferSize)
				{
					USBLog(4, "%s[%p]::CreateGeneralTransfer - bufferSize < Descriptor size - adjusting physical segment 0", getName(), this);
					physicalAddresses[0].length = bufferSize - transferOffset;
				}
                OSWriteLittleInt32(&pOHCIGeneralTransferDescriptor->pShared->bufferEnd, 0, physicalAddresses[0].location + physicalAddresses[0].length - 1);
			}
			
            pOHCIGeneralTransferDescriptor->pLogicalNext = newOHCIGeneralTransferDescriptor;
            pOHCIGeneralTransferDescriptor->pEndpoint = queue;
            pOHCIGeneralTransferDescriptor->pType = type;
            pOHCIGeneralTransferDescriptor->command = command;
            transferOffset += physicalAddresses[0].length;

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

#if (DEBUGGING_LEVEL > 2)
    print_td(pOHCIGeneralTransferDescriptor);
#endif
    if (status)
        USBLog(5, "%s[%p] CreateGeneralTransfer: returning status 0x%x", getName(), this, status);
    return (status);
}



IOReturn AppleUSBOHCI::UIMCreateControlEndpoint(
                                          UInt8				functionAddress,
                                          UInt8				endpointNumber,
                                          UInt16			maxPacketSize,
                                          UInt8				speed,
                                          USBDeviceAddress    		highSpeedHub,
                                          int				highSpeedPort)
{
    USBLog(5, "%s[%p]: UIMCreateControlEndpoint w/ HS ( Addr: %d:%d, max=%d, %s) calling thru", getName(), this, functionAddress, endpointNumber, maxPacketSize, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");

    return UIMCreateControlEndpoint( functionAddress, endpointNumber, maxPacketSize, speed );
}

IOReturn 
AppleUSBOHCI::UIMCreateControlEndpoint(UInt8 functionAddress, UInt8 endpointNumber, UInt16 maxPacketSize, UInt8 speed)
{
    AppleOHCIEndpointDescriptorPtr	pOHCIEndpointDescriptor, pED;

    USBLog(5, "%s[%p]: UIMCreateControlEndpoint( Addr: %d:%d, max=%d, %s)", getName(), this,
          functionAddress, endpointNumber, maxPacketSize, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");
    if (_rootHubFuncAddress == functionAddress)
    {
        if ( (endpointNumber != 0) && (speed == kUSBDeviceSpeedLow))
        {
            // Ignore High Speed for now
            USBLog(3,"%s[%p] UIMCreateControlEndpoint: Bad parameters endpoint: %d, speed: %s",getName(),this,endpointNumber, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");
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

#if (DEBUGGING_LEVEL > 2)
    if ((speed == kUSBDeviceSpeedFull) && _OptiOn)
        print_bulk_list();
    else
	print_control_list();
#endif

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
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "%s[%p] UIMCreateControlTransfer- calling the wrong method!", getName(), this);
    return kIOReturnIPCError;
}



IOReturn 
AppleUSBOHCI::UIMCreateControlTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBCommand*		command,
            IOMemoryDescriptor*	CBP,
            bool				bufferRounding,
            UInt32				bufferSize,
            short				direction)
{
    UInt32				myBufferRounding = 0;
    UInt32				myDirection;
    UInt32				myToggle;
    AppleOHCIEndpointDescriptorPtr		pEDQueue, pEDDummy;
    IOReturn				status;
    IOUSBCompletion			completion = command->GetUSLCompletion();

    USBLog(7, "%s[%p]\tCrntlTx: adr=%d:%d cbp=%lx:%lx br=%s cback=[%lx:%lx] dir=%d)",getName(), this,
          functionAddress, endpointNumber, (UInt32)CBP, bufferSize,
          bufferRounding ? "YES":"NO",
             (UInt32)completion.target, (UInt32)completion.parameter, direction);

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
        USBLog(3, "%s[%p] UIMCreateControlTransfer- Could not find endpoint (FN: %d, EP: %d)!", getName(), this, functionAddress, endpointNumber);
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
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "%s[%p]UIMCreateControlTransfer- calling the wrong method!", getName(), this);
    return kIOReturnIPCError;
}



IOReturn 
AppleUSBOHCI::UIMCreateControlTransfer(
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

    USBLog(1, "%s[%p]UIMCreateControlTransfer- calling the pointer method instead of the desc method!", getName(), this);
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
    USBLog(5,"%s[%p]: UIMCreateBulkEndpoint HS(Addr=%d:%d, max=%d, dir=%d, %s) calling thru", getName(), this,
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


    USBLog(5,"%s[%p]: UIMCreateBulkEndpoint(Addr=%d:%d, max=%d, dir=%d, %s)", getName(), this,
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
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "%s[%p]UIMCreateBulkTransfer- calling the wrong method!", getName(), this);
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

    USBLog(7, "%s[%p]\tBulkTx: adr=%d:%d cbp=%lx:%x br=%s cback=[%lx:%lx:%lx] dir=%d)",getName(),this,
	command->GetAddress(), command->GetEndpoint(), (UInt32)buffer, command->GetReqCount(), command->GetBufferRounding() ?"YES":"NO", 
	(UInt32)completion.action, (UInt32)completion.target, (UInt32)completion.parameter, direction);

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
        USBLog(3, "%s[%p] UIMCreateBulkTransfer- Could not find endpoint!", getName(), this);
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
    USBLog(5, "%s[%p]: UIMCreateInterruptEndpoint HS ( Addr: %d:%d, max=%d, dir=%d, rate=%d, %s) calling thru", getName(), this,
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
    int                                         offset;
    short                                       originalDirection = direction;
    
    
    USBLog(5, "%s[%p]: UIMCreateInterruptEndpoint ( Addr: %d:%d, max=%d, dir=%d, rate=%d, %s)", getName(), this,
           functionAddress, endpointNumber, maxPacketSize,direction,
           pollingRate, (speed == kUSBDeviceSpeedLow) ? "lo" : "full");
    
    if (_rootHubFuncAddress == functionAddress)
    {
        if ( (endpointNumber != 1) || ( speed != kUSBDeviceSpeedFull ) || (direction != kUSBIn) )
        {
            USBLog(3, "%s[%p]: UIMCreateInterruptEndpoint bad parameters: endpNumber %d, speed: %s, direction: %d",getName(), this, endpointNumber, (speed == kUSBDeviceSpeedLow) ? "lo" : "full", direction);
            return kIOReturnBadArgument;
        }
        
        return SimulateInterruptEDCreate(maxPacketSize, pollingRate);
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
        USBLog(3, "%s[%p]: UIMCreateInterruptEndpoint endpoint already existed -- deleting it",getName(), this);
        ret = UIMDeleteEndpoint(functionAddress, endpointNumber, originalDirection);
        if ( ret != kIOReturnSuccess)
        {
            USBLog(3, "%s[%p]: UIMCreateInterruptEndpoint deleting endpoint returned %p",getName(), this, ret);
            return ret;
        }
    }
    else
        USBLog(3, "%s[%p]: UIMCreateInterruptEndpoint endpoint does NOT exist",getName(), this);
    
    
    ///ZZZZz  opti bug fix!!!!
    if (_OptiOn)
        if (speed == kUSBDeviceSpeedFull)
            if (pollingRate >= 8)
                pollingRate = 7;
    
    // Do we have room?? if so return with offset equal to location
    if (DetermineInterruptOffset(pollingRate, maxPacketSize, &offset) == false)
        return(kIOReturnNoBandwidth);
    
    USBLog(5, "%s[%p]: UIMCreateInterruptEndpoint: offset = %d", getName(), this, offset);
    
    pED = (AppleOHCIEndpointDescriptorPtr) _pInterruptHead[offset].pHead;
    pOHCIEndpointDescriptor = AddEmptyEndPoint (functionAddress, endpointNumber, 
                                                maxPacketSize, speed, direction, pED, kOHCIEDFormatGeneralTD);
    if (NULL == pOHCIEndpointDescriptor)
        return(-1);
    
    _pInterruptHead[offset].nodeBandwidth += maxPacketSize;
    
#if (DEBUGGING_LEVEL > 2)
    print_int_list();
#endif
    
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
    // this is the old 1.8/1.8.1 method. It should not be used any more.
    USBLog(1, "%s[%p]UIMCreateInterruptTransfer- calling the wrong method!", getName(), this);
    return kIOReturnIPCError;
}



IOReturn
AppleUSBOHCI::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
    IOReturn				status = kIOReturnSuccess;
    UInt32				myBufferRounding = 0;
    UInt32				myDirection;
    UInt32				myToggle;
    AppleOHCIEndpointDescriptorPtr		pEDQueue, temp;
    IOUSBCompletion			completion = command->GetUSLCompletion();
    IOMemoryDescriptor*			buffer = command->GetBuffer();
    short				direction = command->GetDirection(); // our local copy may change

    USBLog(7, "%s[%p]\tIntTx: adr=%d:%d cbp=%p:%lx br=%s cback=[%lx:%lx:%lx])", getName(), this,
	    command->GetAddress(), command->GetEndpoint(), command->GetBuffer(), 
	    command->GetReqCount(), command->GetBufferRounding()?"YES":"NO", 
	    (UInt32)completion.action, (UInt32)completion.target, 
	    (UInt32)completion.parameter);

    if (_rootHubFuncAddress == command->GetAddress())
    {
        SimulateRootHubInt(command->GetEndpoint(), buffer, command->GetReqCount(), completion);
        return(kIOReturnSuccess);
    }

    if (direction == kUSBOut)
        direction = kOHCIEDDirectionOut;
    else if (direction == kUSBIn)
        direction = kOHCIEDDirectionIn;
    else
        direction = kOHCIEDDirectionTD;

    pEDQueue = FindInterruptEndpoint(command->GetAddress(), command->GetEndpoint(), direction, &temp);
    if (pEDQueue != NULL)
    {
	if (command->GetBufferRounding())
	    myBufferRounding = kOHCIGTDControl_R;
        myToggle = 0;	/* Take data toggle from Endpoint Descriptor */

        myDirection = (UInt32) direction << kOHCIDirectionOffset;

	status = CreateGeneralTransfer(pEDQueue, command, buffer, command->GetReqCount(), myBufferRounding | myDirection | myToggle, kOHCIInterruptInType, 0);

    }
    else
    {
        USBLog(3, "%s[%p] UIMCreateInterruptTransfer- Could not find endpoint!", getName(), this);
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
    if (pED) {
        // this is the case where we have already created this endpoint, and now we are adjusting the maxPacketSize
        //
        USBLog(2,"%s[%p]::UIMCreateIsochEndpoint endpoint already exists, changing maxPacketSize to %d",getName(), this, maxPacketSize);

        edFlags = USBToHostLong(pED->pShared->flags);
        curMaxPacketSize = ( edFlags & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase;
        if (maxPacketSize == curMaxPacketSize) 
	{
            USBLog(2,"%s[%p]::UIMCreateIsochEndpoint maxPacketSize (%d) the same, no change",getName(), this, maxPacketSize);
            return kIOReturnSuccess;
        }
        if (maxPacketSize > curMaxPacketSize) 
	{
            // client is trying to get more bandwidth
            xtraRequest = maxPacketSize - curMaxPacketSize;
            if (xtraRequest > _isochBandwidthAvail)
            {
                USBLog(2,"%s[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %d, available: %d",getName(), this, xtraRequest, _isochBandwidthAvail);
                return kIOReturnNoBandwidth;
            }
            _isochBandwidthAvail -= xtraRequest;
            USBLog(2,"%s[%p]::UIMCreateIsochEndpoint grabbing additional bandwidth: %d, new available: %d",getName(), this, xtraRequest, _isochBandwidthAvail);
        } else 
	{
            // client is trying to return some bandwidth
            xtraRequest = curMaxPacketSize - maxPacketSize;
            _isochBandwidthAvail += xtraRequest;
            USBLog(2,"%s[%p]::UIMCreateIsochEndpoint returning some bandwidth: %d, new available: %d",getName(), this, xtraRequest, _isochBandwidthAvail);

        }
        // update the maxPacketSize field in the endpoint
        edFlags &= ~kOHCIEDControl_MPS;					// strip out old MPS
        edFlags |= (maxPacketSize << kOHCIEDControl_MPSPhase);
        OSWriteLittleInt32(&pED->pShared->flags, 0, edFlags);
        return kIOReturnSuccess;
    }

    if (maxPacketSize > _isochBandwidthAvail) 
    {
        USBLog(3,"%s[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %d, available: %d",getName(), this, maxPacketSize, _isochBandwidthAvail);
        return kIOReturnNoBandwidth;
    }

    _isochBandwidthAvail -= maxPacketSize;
    pED = _pIsochHead;
    pOHCIEndpointDescriptor = AddEmptyEndPoint(functionAddress, endpointNumber,
	maxPacketSize, kUSBDeviceSpeedFull, direction, pED, kOHCIEDFormatIsochronousTD);
    if (pOHCIEndpointDescriptor == NULL) {
        _isochBandwidthAvail += maxPacketSize;
        return(kIOReturnNoMemory);
    }

    USBLog(5,"%s[%p]::UIMCreateIsochEndpoint success. bandwidth used = %d, new available: %d",getName(), this, maxPacketSize, _isochBandwidthAvail);

    return kIOReturnSuccess;
}



IOReturn 
AppleUSBOHCI::UIMCreateIsochTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBIsocCompletion			completion,
            UInt8				direction,
            UInt64				frameNumberStart,
            IOMemoryDescriptor *		pBuffer,
            UInt32				frameCount,
            IOUSBIsocFrame			*pFrames)
{
    IOReturn 				status = kIOReturnSuccess;
    AppleOHCIIsochTransferDescriptorPtr	pTailITD = NULL;
    AppleOHCIIsochTransferDescriptorPtr	pNewITD = NULL;
    AppleOHCIIsochTransferDescriptorPtr	pTempITD = NULL;
    UInt32				i;
    UInt32				curFrameInRequest = 0;
    UInt32				bufferSize = 0;
    UInt32				pageOffset = 0;
    UInt32				prevFramesPage = 0;
    UInt32				lastPhysical = 0;
    UInt32				segmentEnd = 0;
    AppleOHCIEndpointDescriptorPtr		pED;
    UInt32				curFrameInTD = 0;
    UInt16				frameNumber = (UInt16) frameNumberStart;
    UInt64				curFrameNumber = GetFrameNumber();
    UInt64				frameDiff;
    UInt64				maxOffset = (UInt64)(0x00007FF0);
    UInt32				diff32;

    UInt32				itdFlags = 0;
    UInt32				numSegs = 0;
    UInt32				physPageStart = 0;
    UInt32				physPageEnd = 0;
    UInt32				pageSelectMask = 0;
    bool				needNewITD;
    bool				multiPageSegment = false;
    IOPhysicalSegment			segs[2];
    UInt32				tdType;
    IOByteCount				transferOffset;

    if ( (frameCount == 0) || (frameCount > 1000) )
    {
        USBLog(3,"%s[%p]::UIMCreateIsochTransfer bad frameCount: %d",getName(), this, frameCount);
        return kIOReturnBadArgument;
    }

    if (direction == kUSBOut) {
        direction = kOHCIEDDirectionOut;
        tdType = kOHCIIsochronousOutType;
    }
    else if (direction == kUSBIn) {
        direction = kOHCIEDDirectionIn;
        tdType = kOHCIIsochronousInType;
    }
    else
        return kIOReturnInternalError;

    pED = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);

    if (!pED)
    {
        USBLog(3,"%s[%p]::UIMCreateIsochTransfer endpoint (%d) not found: 0x%x",getName(), this, endpointNumber, kIOUSBEndpointNotFound);
        return kIOUSBEndpointNotFound;
    }

    if (frameNumberStart <= curFrameNumber)
    {
        if (frameNumberStart < (curFrameNumber - maxOffset))
        {
            USBLog(3,"%s[%p]::UIMCreateIsochTransfer request frame WAY too old.  frameNumberStart: %ld, curFrameNumber: %ld.  Returning 0x%x",getName(), this, (UInt32) frameNumberStart, (UInt32) curFrameNumber, kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
        USBLog(5,"%s[%p]::UIMCreateIsochTransfer WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %ld, curFrameNumber: %ld.  USBIsocFrame Ptr: %p, First ITD: %p",getName(), this, (UInt32) frameNumberStart, (UInt32) curFrameNumber, pFrames, pED->pLogicalTailP);
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

    //
    //  Get the total size of buffer
    //
    for ( i = 0; i< frameCount; i++)
    {
        if (pFrames[i].frReqCount > kUSBMaxFSIsocEndpointReqCount)
        {
            USBLog(3,"%s[%p]::UIMCreateIsochTransfer Isoch frame too big %d",getName(), this, pFrames[i].frReqCount);
            return kIOReturnBadArgument;
        }
        bufferSize += pFrames[i].frReqCount;        
    }

    USBLog(7,"%s[%p]::UIMCreateIsochTransfer transfer %s, buffer: %p, length: %d",getName(), this, (direction == kOHCIEDDirectionIn) ? "in" : "out", pBuffer, bufferSize);

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

    if (!bufferSize) {
	// Set up suitable dummy info
        numSegs = 1;
        segs[0].location = segs[0].length = 0;
	pageOffset = 0;
    }
    pTailITD = (AppleOHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;	// start with the unused TD on the tail of the list
    OSWriteLittleInt32(&pTailITD->pShared->nextTD, 0, pNewITD->pPhysical);	// link in the new ITD
    pTailITD->pLogicalNext = pNewITD;

    needNewITD = false;
    transferOffset = 0;
    while (curFrameInRequest < frameCount) 
    {
        // Get physical segments for next frame
        if (!needNewITD && bufferSize && (pFrames[curFrameInRequest].frReqCount != 0) ) 
	{
            numSegs = _isoCursor->getPhysicalSegments(pBuffer, transferOffset, segs, 2, pFrames[curFrameInRequest].frReqCount);
            pageOffset = segs[0].location & kOHCIPageOffsetMask;
            transferOffset += segs[0].length;
            segmentEnd = (segs[0].location + segs[0].length )  & kOHCIPageOffsetMask;
            
            USBLog(8,"curFrameInRequest: %d, curFrameInTD: %d, pageOffset: %x, numSegs: %d, seg[0].location: %p, seg[0].length: %d", curFrameInRequest, curFrameInTD, pageOffset, numSegs, segs[0].location, segs[0].length);
            
            if(numSegs == 2)
            {
                transferOffset += segs[1].length;
                USBLog(8 ,"seg[1].location: %p, seg[1].length %d",segs[1].location, segs[1].length);
                
                // If we are wrapping around the same physical page and we are on an NEC controller, then we need to discard the 2nd segment.  It will click
                // but at least we won't hang the controller
                //
                if ( (_errataBits & kErrataNECOHCIIsochWraparound) && ((segs[0].location & kOHCIPageMask) == (segs[1].location & kOHCIPageMask)) )
                {
                    USBLog(1,"%s[%p]::UIMCreateIsochTransfer(LL) On an NEC controller and frame data wraps from end of buffer to beginning.  Dropping data to avoid controller hang");
                    numSegs = 1;
                }
            }
            
            if ( (segs[numSegs-1].location & kOHCIPageMask) != ((segs[numSegs-1].location + segs[numSegs-1].length) & kOHCIPageMask))
            {
                multiPageSegment = true;
                USBLog(8,"We have a segment that crosses a page boundary:  start: %p, length: %d, end: %p, curFrameinTD: %d", segs[numSegs-1].location, segs[numSegs-1].length,segs[numSegs-1].location + segs[numSegs-1].length, curFrameInTD);
            }
            else
                multiPageSegment = false;
        }

        if (curFrameInTD == 0) 
	{
            // set up counters which get reinitialized with each TD
            physPageStart = segs[0].location & kOHCIPageMask;	// for calculating real 13 bit offsets
            pageSelectMask = 0;					// First frame always starts on first page
            needNewITD = false;

            // set up the header of the TD - itdFlags will be stored into flags later
            itdFlags = (UInt16)(curFrameInRequest + frameNumber);
            pTailITD->pIsocFrame = pFrames;		// so we can get back to our info later
            pTailITD->frameNum = curFrameInRequest;	// our own index into the above array
            pTailITD->pType = tdType;			// So interrupt handler knows TD type.
            OSWriteLittleInt32(&pTailITD->pShared->bufferPage0, 0,  physPageStart);
        }
        else if ((segs[0].location & kOHCIPageMask) != physPageStart) 
	{
            // pageSelectMask is set if we've already used our one allowed page cross.
            //
            if ( (pageSelectMask && (((segs[0].location & kOHCIPageMask) != physPageEnd) || numSegs == 2)) )
	    {
                // Need new ITD for this condition
                needNewITD = true;
                
                USBLog(8, "%s[%p]::UIMCreateIsochTransfer - got it! (%d, %p, %p, %d)", getName(), this, pageSelectMask, segs[0].location & kOHCIPageMask, physPageEnd, numSegs);
                
            }
            else if ( pageSelectMask && multiPageSegment )
            {
                // We have already crossed one page and we have a segment that spans 2 or more pages
                //
                needNewITD = true;
                USBLog(8,"%s[%p]::UIMCreateIsochTransfer This frame spans 2 or more pages and we already used our page crossing ",getName(), this);
            }
            else if ( (prevFramesPage != (segs[0].location & kOHCIPageMask)) && (segmentEnd != 0) )
            {
                // We have a segment that starts in a new page but the previous one did not end
                // on a page boundary.  Need a new ITD for this condition. 
                // Need new ITD for this condition
                needNewITD = true;
                
                USBLog(8,"%s[%p]::UIMCreateIsochTransfer This frame starts on a new page and the previous one did NOT end on a page boundary (%d)",getName(), this, segmentEnd);
            }
            else
	    {
                if (pageSelectMask == 0 )
                    USBLog(8,"Using our page crossing for this TD (%p)",(segs[numSegs-1].location + segs[numSegs-1].length -1 ) & kOHCIPageMask);

                pageSelectMask = kOHCIPageSize;	// ie. set bit 13
		physPageEnd = (segs[numSegs-1].location  + segs[numSegs-1].length) & kOHCIPageMask;
	    }
        }
        
        // Save this frame's Page so that we can use it when the next frame is process to compare and see
        // if they are different
        //
        prevFramesPage = (segs[numSegs-1].location  + segs[numSegs-1].length) & kOHCIPageMask;
                
        if ((curFrameInTD > 7) || needNewITD) 
	{
            // we need to start a new TD
            needNewITD = true;	// To simplify test at top of loop.
            itdFlags |= (curFrameInTD-1) << kOHCIITDControl_FCPhase;
            OSWriteLittleInt32(&pTailITD->pShared->bufferEnd, 0, lastPhysical);
            curFrameInTD = 0;
            pNewITD = AllocateITD();
            USBLog(7, "%s[%p]::UIMCreateIsochTransfer - new iTD %p", getName(), this, pNewITD);
            if (pNewITD == NULL) 
	    {
                status = kIOReturnNoMemory;
		break;
            }
            // Handy for debugging transfer lists
            itdFlags |= (kOHCIGTDConditionNotAccessed << kOHCIGTDControl_CCPhase);
            
            // In the past, we set the DI bits to 111b at this point, so that we only interrupted in the last transfer.  
            // However, this presents a problem when we abort a pipe, there might be a partial transfer 
            // in the done queue that has not caused an interrupt.  Then  when another TD completes, we will
            // process this orphaned  TD and update the framelist the memory could have been released in the meantime, since
            // our client could have closed the connection.  So, we need to interrupt for every TD.
            
            OSWriteLittleInt32(&pTailITD->pShared->flags, 0, itdFlags);

            // print_itd(pTailITD);

            pTailITD->completion.action = NULL;
            pTailITD = pTailITD->pLogicalNext;		// this is the "old" pNewTD
            OSWriteLittleInt32(&pTailITD->pShared->nextTD, 0, pNewITD->pPhysical);	// link to the "new" pNewTD
            pTailITD->pLogicalNext = pNewITD;
            continue;		// start over
        }
        //
        // at this point we know we have a frame which will fit into the current TD
        //
        // calculate the buffer offset for the beginning of this frame
        OSWriteLittleInt16(&pTailITD->pShared->offset[curFrameInTD], 0,
            pageOffset |		// offset
            pageSelectMask |		// offset from BP0 or BufferEnd
            (kOHCIITDOffsetConditionNotAccessed << kOHCIITDOffset_CCPhase));	// mark as unused

        // adjust counters and calculate the physical offset of the end of the frame for the next time around the loop
        curFrameInRequest++;
        curFrameInTD++;
        lastPhysical = segs[numSegs-1].location + segs[numSegs-1].length - 1;
    }			

    if (status != kIOReturnSuccess)
    {
        // unlink the TDs, unlock the buffer, and return the status
        pNewITD = pTailITD->pLogicalNext;	// point to the "old" pNewTD, which will also get deallocated
        pTempITD = (AppleOHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;
        pTailITD = pTempITD->pLogicalNext;	// don't deallocate the real tail!
        pTempITD->pLogicalNext = NULL;		// just to make sure
        pTempITD->pShared->nextTD = NULL;			// just to make sure
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

        // print_itd(pTailITD);
        // Make new descriptor the tail
        pED->pLogicalTailP = pNewITD;
        OSWriteLittleInt32(&pED->pShared->tdQueueTailPtr, 0, pNewITD->pPhysical);
    }

    //print_isoc_ed(pED);

    return status;

}



IOReturn 
AppleUSBOHCI::UIMAbortEndpoint(
            short				functionAddress,
            short				endpointNumber,
            short				direction)
{
    AppleOHCIEndpointDescriptorPtr	pED;
    AppleOHCIEndpointDescriptorPtr	pEDQueueBack;
    UInt32			something, controlMask;

    USBLog(5, "%s[%p] UIMAbortEndpoint: Addr: %d, Endpoint: %d,%d", getName(), this, functionAddress,endpointNumber,direction);

    if (functionAddress == _rootHubFuncAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "%s[%p] UIMAbortEndpoint: bad params - endpNumber: %d", getName(), this, endpointNumber );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDAbort (endpointNumber, direction) in 9
        //
        USBLog(5, "%s[%p] UIMAbortEndpoint: Attempting operation on root hub", getName(), this);
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
        USBLog(3, "%s[%p] UIMAbortEndpoint- Could not find endpoint!", getName(), this);
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

    RemoveTDs(pED);

    pED->pShared->flags &= ~HostToUSBLong(kOHCIEDControl_K);	// activate ED again


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

    USBLog(5, "%s[%p] UIMDeleteEndpoint: Addr: %d, Endpoint: %d,%d",getName(), this, functionAddress,endpointNumber,direction);
    
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
        USBLog(3, "%s[%p] UIMDeleteEndpoint- Could not find endpoint!", getName(), this);
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

    USBLog(5, "%s[%p]::UIMDeleteEndpoint", getName(), this); 
    
    if (GetEDType(pED) == kOHCIEDFormatIsochronousTD)
    {
        UInt32 maxPacketSize = (USBToHostLong(pED->pShared->flags) & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase;
        _isochBandwidthAvail += maxPacketSize;
        USBLog(5, "%s[%p]::UIMDeleteEndpoint (Isoch) - bandwidth returned %d, new available: %d", getName(), this, maxPacketSize, _isochBandwidthAvail);
    }
    RemoveAllTDs(pED);

    pED->pShared->nextED = NULL;

    //deallocate ED
    DeallocateED(pED);
#if (DEBUGGING_LEVEL > 2)
        print_bulk_list();
        print_control_list();
#endif
    return (kIOReturnSuccess);
}



IOReturn 
AppleUSBOHCI::UIMClearEndpointStall(
            short				functionAddress,
            short				endpointNumber,
            short				direction)
{
    AppleOHCIEndpointDescriptorPtr		pEDQueueBack, pED;
    AppleOHCIGeneralTransferDescriptorPtr	transaction;
    UInt32					tail, controlMask;


    USBLog(5, "+%s[%p]: clearing endpoint %d:%d stall", getName(), this, functionAddress, endpointNumber);
    
    if (_rootHubFuncAddress == functionAddress)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "%s[%p] UIMClearEndpointStall: bad params - endpNumber: %d", getName(), this, endpointNumber );
            return kIOReturnBadArgument;
        }
        
        USBLog(5, "%s[%p] UIMClearEndpointStall: Attempting operation on root hub", getName(), this);
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
        USBLog(3, "%s[%p] UIMClearEndpointStall- Could not find endpoint!", getName(), this);
        return (kIOUSBEndpointNotFound);
    }

    if (pED != NULL)
    {
        tail = USBToHostLong(pED->pShared->tdQueueTailPtr);
        transaction = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCIHeadPMask);
        // unlink all transactions at once (this also clears the halted bit)
        pED->pShared->tdQueueHeadPtr = pED->pShared->tdQueueTailPtr;
        pED->pLogicalHeadP = pED->pLogicalTailP;
    }	

    if (transaction != NULL)
    {
        ReturnTransactions(transaction, tail);
    }
    
    USBLog(5, "-%s[%p]: clearing endpoint %d:%d stall", getName(), this, functionAddress, endpointNumber);

    return (kIOReturnSuccess);
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
        if ((USBToHostLong(pEDQueue->pShared->flags) & kUniqueNumNoDirMask) == unique)
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
	short 						functionNumber, 
	short						endpointNumber,
	short						direction,
	AppleOHCIEndpointDescriptorPtr			*pEDBack)
{

    UInt32			unique;
    UInt32			myEndpointDirection;
    AppleOHCIEndpointDescriptorPtr	pEDQueue;
    AppleOHCIEndpointDescriptorPtr	pEDQueueBack;


    // search for endpoint descriptor
    myEndpointDirection = ((UInt32) direction) << kOHCIEndpointDirectionOffset;
    unique = (UInt32) ((((UInt32) endpointNumber) << kOHCIEndpointNumberOffset)
                       | ((UInt32) functionNumber) | myEndpointDirection);
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
        if(_OptiOn)
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
            if(pEDBack)
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
	short 					functionNumber,
	short					endpointNumber,
        short					direction,
	AppleOHCIEndpointDescriptorPtr		*pEDBack)
{
    UInt32				myEndpointDirection;
    UInt32				unique;
    AppleOHCIEndpointDescriptorPtr		pEDQueue;
    int					i;
    UInt32				temp;
    
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
            temp = (USBToHostLong(pEDQueue->pShared->flags)) & kUniqueNumMask;

            if ( temp == unique)
            {
                return(pEDQueue);
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
        USBError(1,"%s[%p]::DetermineInterruptOffset pollingRate of 0 -- that's illegal!", getName(), this);
        return(false);
    }
    else if(pollingRate < 2)
        *offset = 62;
    else if(pollingRate < 4)
        *offset = (num%2) + 60;
    else if(pollingRate < 8)
        *offset = (num%4) + 56;
    else if(pollingRate < 16)
        *offset = (num%8) + 48;
    else if(pollingRate < 32)
        *offset = (num%16) + 32;
    else
        *offset = (num%32) + 0;
    return (true);
}

#if (DEBUGGING_LEVEL > 2)
static char *cc_errors[] = {
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
AppleUSBOHCI::print_td(AppleOHCIGeneralTransferDescriptorPtr pTD)
{
    UInt32 w0, dir, err;

    if (pTD == 0) return;

    w0 = USBToHostLong(pTD->pShared->ohciFlags);
    dir = (w0 & kOHCIGTDControl_DP) >> kOHCIGTDControl_DPPhase;
    err = (w0 & kOHCIGTDControl_CC) >> kOHCIGTDControl_CCPhase;
    USBLog(7, "%s[%p]\tTD(0x%08lx->0x%08lx) dir=%s cc=%s errc=%ld t=%ld rd=%s: c=0x%08lx cbp=0x%08lx, next=0x%08lx, bend=0x%08lx",
        getName(), this,
	(UInt32)pTD, pTD->pPhysical,
	dir == 0 ? "SETUP" : (dir==2?"IN":"OUT"),
	cc_errors[err],
	(w0 & kOHCIGTDControl_EC) >> kOHCIGTDControl_ECPhase,
	(w0 & kOHCIGTDControl_T)  >> kOHCIGTDControl_TPhase,
	(w0 & kOHCIGTDControl_R)?"yes":"no",
	USBToHostLong(pTD->pShared->ohciFlags),
	USBToHostLong(pTD->pShared->currentBufferPtr),
	USBToHostLong(pTD->pShared->nextTD),
	USBToHostLong(pTD->pShared->bufferEnd));
}



void 
AppleUSBOHCI::print_itd(AppleOHCIIsochTransferDescriptorPtr pTD)
{
    UInt32 w0, err;
    int i;
    if (pTD == 0) return;

    w0 = USBToHostLong(pTD->pShared->flags);
    err = (w0 & kOHCIITDControl_CC) >> kOHCIITDControl_CCPhase;
    USBLog(5, "%s[%p]\tTD(%p->%p) cc=%s fc=%ld sf=0x%lx c=0x%08lx bp0=%p, bend=%p, next=%p",
           getName(), this,
           (UInt32)pTD, pTD->pPhysical,
           cc_errors[err],
           (w0 & kOHCIITDControl_FC) >> kOHCIITDControl_FCPhase,
           (w0 & kOHCIITDControl_SF)  >> kOHCIITDControl_SFPhase,
           w0,
           USBToHostLong(pTD->pShared->bufferPage0),
           USBToHostLong(pTD->pShared->bufferEnd),
           USBToHostLong(pTD->pShared->nextTD)
           );
    for(i=0; i<8; i++)
    {
        USBLog(5, "Offset/PSW %d = 0x%x", i, USBToHostWord(pTD->pShared->offset[i]));
    }
    USBLog(5, "frames = 0x%lx, FrameNumber %ld", (UInt32)pTD->pIsocFrame, pTD->frameNum);
}



void 
AppleUSBOHCI::print_ed(AppleOHCIEndpointDescriptorPtr pED)
{
    AppleOHCIGeneralTransferDescriptorPtr	pTD;
    UInt32 w0;


    if (pED == 0) {
	kprintf("Null ED\n");
	return;
    }
    w0 = USBToHostLong(pED->pShared->flags);

    if ((w0 & kOHCIEDControl_K) == 0 /*noskip*/)
    {
        USBLog(7, "%s[%p] ED(0x%08lx->0x%08lx) %ld:%ld d=%ld s=%s sk=%s i=%s max=%ld : c=0x%08lx tail=0x%08lx, head=0x%08lx, next=0x%08lx",
              getName(), this, 
              (UInt32)pED, (UInt32)pED->pPhysical,
              (w0 & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase,
              (w0 & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase,
              (w0 & kOHCIEDControl_D)  >> kOHCIEDControl_DPhase,
              w0 & kOHCIEDControl_S?"low":"hi",
              w0 & kOHCIEDControl_K?"yes":"no",
              w0 & kOHCIEDControl_F?"yes":"no",
              (w0 & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase,
              USBToHostLong(pED->pShared->flags),
              USBToHostLong(pED->pShared->tdQueueTailPtr),
              USBToHostLong(pED->pShared->tdQueueHeadPtr),
              USBToHostLong(pED->pShared->nextED));

        //pTD = (AppleOHCIGeneralTransferDescriptorPtr) pED->pVirtualHeadP;
       pTD = AppleUSBOHCIgtdMemoryBlock::GetGTDFromPhysical(USBToHostLong(pED->pShared->tdQueueHeadPtr) & kOHCINextEndpointDescriptor_nextED);
       while (pTD != 0)
        {
            // DEBUGLOG("\t");
            print_td(pTD);
            pTD = pTD->pLogicalNext;
        }
    }
}



void 
AppleUSBOHCI::print_isoc_ed(AppleOHCIEndpointDescriptorPtr pED)
{
    AppleOHCIIsochTransferDescriptorPtr	pTD;
    UInt32 w0;


    if (pED == 0) {
        kprintf("Null ED\n");
        return;
    }
    w0 = USBToHostLong(pED->pShared->flags);

    if ((w0 & kOHCIEDControl_K) == 0 /*noskip*/)
    {
        USBLog(7, "%s[%p] ED(0x%08lx->0x%08lx) %ld:%ld d=%ld s=%s sk=%s i=%s max=%ld : c=0x%08lx tail=0x%08lx, head=0x%08lx, next=0x%08lx",
              getName(), this,
              (UInt32)pED, (UInt32)pED->pPhysical,
              (w0 & kOHCIEDControl_FA) >> kOHCIEDControl_FAPhase,
              (w0 & kOHCIEDControl_EN) >> kOHCIEDControl_ENPhase,
              (w0 & kOHCIEDControl_D)  >> kOHCIEDControl_DPhase,
              w0 & kOHCIEDControl_S?"low":"hi",
              w0 & kOHCIEDControl_K?"yes":"no",
              w0 & kOHCIEDControl_F?"yes":"no",
              (w0 & kOHCIEDControl_MPS) >> kOHCIEDControl_MPSPhase,
              USBToHostLong(pED->pShared->flags),
              USBToHostLong(pED->pShared->tdQueueTailPtr),
              USBToHostLong(pED->pShared->tdQueueHeadPtr),
              USBToHostLong(pED->pShared->nextED));

        pTD = (AppleOHCIIsochTransferDescriptorPtr) pED->pLogicalHeadP;
        while (pTD != 0)
        {
           //  DEBUGLOG("\t");
            print_itd(pTD);
            pTD = pTD->pLogicalNext;
        }
    }
}



void 
AppleUSBOHCI::print_list(AppleOHCIEndpointDescriptorPtr pListHead, AppleOHCIEndpointDescriptorPtr pListTail)
{
    AppleOHCIEndpointDescriptorPtr		pED, pEDTail;


    pED = (AppleOHCIEndpointDescriptorPtr) pListHead;
    pEDTail = (AppleOHCIEndpointDescriptorPtr) pListTail;

    while (pED != pEDTail)
    {
        print_ed(pED);
        pED = pED->pLogicalNext;
    }
    print_ed(pEDTail);
}



void 
AppleUSBOHCI::print_control_list()
{
    USBLog(7, "%s[%p] Control List: h/w head = 0x%lx", getName(), this, USBToHostLong(_pOHCIRegisters->hcControlHeadED));
    print_list(_pControlHead, _pControlTail);
}



void 
AppleUSBOHCI::print_bulk_list()
{
    USBLog(7, "%s[%p] Bulk List: h/w head = 0x%lx", getName(), this, USBToHostLong(_pOHCIRegisters->hcBulkHeadED));
    print_list((AppleOHCIEndpointDescriptorPtr) _pBulkHead, (AppleOHCIEndpointDescriptorPtr) _pBulkTail);
}



void 
AppleUSBOHCI::print_int_list()
{
    int				i;
    UInt32			w0;
    AppleOHCIEndpointDescriptorPtr 	pED;


    USBLog(7, "%s[%p]Interrupt List:", getName(), this);
    for (i = 0; i < 63; i++)
    {
        pED = _pInterruptHead[i].pHead->pLogicalNext;
        w0 = USBToHostLong(pED->pShared->flags);

        if ((w0 & kOHCIEDControl_K) == 0 /*noskip*/)
        {
            USBLog(7, "%d:", i);
            print_ed(pED);
        }
    }
}
#endif


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
                USBLog(2, "(%p)Found a transaction past the completion deadline, timing out! (%x - %x)", pTD, curFrame, firstActiveFrame);
                USBError(1,"%s[%p]::Found a transaction past the completion deadline on bus %d, timing out!",getName(), this, _busNumber);
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
            USBLog(2, "(%p)Found a transaction which hasn't moved in 5 seconds, timing out! (%x - %x)", pTD, curFrame, pTD->lastFrame);
            USBError(1,"%s[%p]::Found a transaction which hasn't moved in 5 seconds on bus %d, timing out!",getName(), this, _busNumber);
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
	IOReturn		err;

	// Check to see if we need to recreate our root hub device
	if (_needToCreateRootHub)
	{
		USBLog(5,"%s[%p] Need to recreate root hub on bus %d, sleeping", getName(), this, _busNumber);
		_needToCreateRootHub = false;
		
		IOSleep(4000);  // Sleep for 4s
		
		USBLog(5,"%s[%p] Need to recreate root hub on bus %d, powering up hardware", getName(), this, _busNumber);

		// Initialize the hardware
		//
		UIMInitializeForPowerUp();
		
		_ohciAvailable = true;                          // tell the interrupt filter routine that we are on
		_ohciBusState = kOHCIBusStateRunning;
		
		if ( _rootHubDevice == NULL )
		{
			err = CreateRootHubDevice( _device, &_rootHubDevice );
			if ( err != kIOReturnSuccess )
			{
				USBError(1,"%s[%p] Could not create root hub device upon wakeup (%x)!",getName(), this, err);
			}
			else
			{
				_rootHubDevice->registerService(kIOServiceRequired | kIOServiceSynchronous);
			}
		}
	}
	
    // If we are not active anymore or if we're in ohciBusStateOff, then don't check for timeouts 
    //
    if ( isInactive() || (_onCardBus && _pcCardEjected) || !_ohciAvailable || (_ohciBusState != kOHCIBusStateRunning))
	{
		USBLog(5,"%s[%p]  UIMCheckForTimeouts for bus %d -- not appropriate", getName(), this, _busNumber);
        return;
	}
    
    // Check to see if our control or bulk lists have a TD that has timed out
    //
    CheckEDListForTimeouts(_pControlHead, _pControlTail);
    CheckEDListForTimeouts(_pBulkHead, _pBulkTail);

    // See if it's time to check for Root Hub inactivity
    //
    if ( !_idleSuspend )
    {
        // Check to see if it's been kOHCICheckForRootHubConnectionsPeriod seconds
        // since we last checked this port
        //
        clock_get_uptime( &currentTime );
        SUB_ABSOLUTETIME(&currentTime, &_lastCheckedTime );
        absolutetime_to_nanoseconds(currentTime, &elapsedTime);
        elapsedTime /= 1000000000;				// Convert to seconds from nanoseconds
        
        if ( elapsedTime >= kOHCICheckForRootHubConnectionsPeriod )
        {
            USBLog(6,"%s[%p] Time to check for root hub inactivity on bus %d", getName(), this, _busNumber);
            clock_get_uptime( &_lastCheckedTime );
            
            // Check to see if the root hub has been inactive for kOHCICheckForRootHubInactivityPeriod seconds
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
            
                if ( elapsedTime >= kOHCICheckForRootHubInactivityPeriod )
                {
                    // Yes, nothing connected to this root hub and it's been more than kOHCICheckForRootHubInactivityPeriod secs since
                    // we last saw something happen on it, so let's suspend that bus
                    //
                    USBLog(5,"%s[%p] Time to suspend the ports of bus %d", getName(), this, _busNumber);
                    setPowerState( kOHCISetPowerLevelIdleSuspend, this);
                }
            }
        }
    }
    
    // From OS9:  Ferg 1-29-01
    // some controllers can be swamped by PCI traffic and essentially go dead.  
    // here we attempt to detect this condition and recover from it.
    //
    if ( _errataBits & kErrataNeedsWatchdogTimer ) 
    {
        UInt16 			hccaFrameNumber, hcFrameNumber;
        UInt32			fmInterval, hcca, bulkHead, controlHead, periodicStart, intEnable, fmNumber;
        
        hcFrameNumber = (UInt16) USBToHostLong(_pOHCIRegisters->hcFmNumber);  // check this first in case an interrupt delays the second read
        hccaFrameNumber = (UInt16) USBToHostLong(*(UInt32 *)(_pHCCA + 0x80));
        
        if ( (hcFrameNumber > 5) && (hcFrameNumber > (hccaFrameNumber+5)) )
        {
            USBError(1,"%s[%p] Watchdog detected dead controller (hcca #: %d, hc #: %d)", getName(), this,  (UInt32) hccaFrameNumber, (UInt32) hcFrameNumber);
                    
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
AppleUSBOHCI::UIMCreateIsochTransfer(
            short				functionAddress,
            short				endpointNumber,
            IOUSBIsocCompletion			completion,
            UInt8				direction,
            UInt64				frameNumberStart,
            IOMemoryDescriptor *		pBuffer,
            UInt32				frameCount,
            IOUSBLowLatencyIsocFrame		*pFrames,
            UInt32				updateFrequency)
{
    IOReturn 				status = kIOReturnSuccess;
    AppleOHCIIsochTransferDescriptorPtr	pTailITD = NULL;
    AppleOHCIIsochTransferDescriptorPtr	pNewITD = NULL;
    AppleOHCIIsochTransferDescriptorPtr	pTempITD = NULL;
    UInt32				i;
    UInt32				curFrameInRequest = 0;
    UInt32				bufferSize = 0;
    UInt32				pageOffset = 0;
    UInt32				segmentEnd = 0;
    UInt32				lastPhysical = 0;
    AppleOHCIEndpointDescriptorPtr		pED;
    UInt32				curFrameInTD = 0;
    UInt16				frameNumber = (UInt16) frameNumberStart;
    UInt64				curFrameNumber = GetFrameNumber();
    UInt64				frameDiff;
    UInt64				maxOffset = (UInt64)(0x00007FF0);
    UInt32				diff32;

    UInt32				itdFlags = 0;
    UInt32				numSegs = 0;
    UInt32				physPageStart = 0;
    UInt32				prevFramesPage = 0;
    UInt32				physPageEnd = 0;
    UInt32				pageSelectMask = 0;
    bool				needNewITD;
    bool				multiPageSegment = false;
    IOPhysicalSegment			segs[2];
    UInt32				tdType;
    IOByteCount				transferOffset;
    bool				useUpdateFrequency = true;
    
    if ( (frameCount == 0) || (frameCount > 1000) )
    {
        USBLog(3,"%s[%p]::UIMCreateIsochTransfer(LL) bad frameCount: %d",getName(), this, frameCount);
        return kIOReturnBadArgument;
    }

    if (direction == kUSBOut) {
        direction = kOHCIEDDirectionOut;
        tdType = kOHCIIsochronousOutLowLatencyType;
    }
    else if (direction == kUSBIn) {
        direction = kOHCIEDDirectionIn;
        tdType = kOHCIIsochronousInLowLatencyType;
    }
    else
        return kIOReturnInternalError;

    pED = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);

    if (!pED)
    {
        USBLog(3,"%s[%p]::UIMCreateIsochTransfer(LL) endpoint (%d) not found. Returning 0x%x", getName(), this, endpointNumber, kIOUSBEndpointNotFound);
        return kIOUSBEndpointNotFound;
    }

    if ( updateFrequency == 0 )
        useUpdateFrequency = false;
        
    if (frameNumberStart <= curFrameNumber)
    {
        if (frameNumberStart < (curFrameNumber - maxOffset))
        {
            USBLog(3,"%s[%p]::UIMCreateIsochTransfer(LL) request frame WAY too old.  frameNumberStart: %ld, curFrameNumber: %ld.  Returning 0x%x", getName(), this, (UInt32) frameNumberStart, (UInt32) curFrameNumber, kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
       USBLog(6,"%s[%p]::UIMCreateIsochTransfer(LL) WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %ld, curFrameNumber: %ld.  USBIsocFrame Ptr: %p, First ITD: %p",getName(), this, (UInt32) frameNumberStart, (UInt32) curFrameNumber, pFrames, pED->pLogicalTailP);
    } else 
    {	
        // frameNumberStart > curFrameNumber
        //
        if (frameNumberStart > (curFrameNumber + maxOffset))
        {
            USBLog(3,"%s[%p]::UIMCreateIsochTransfer(LL) request frame too far ahead!  frameNumberStart: %ld, curFrameNumber: %ld, Returning 0x%x",getName(), this, (UInt32) frameNumberStart, (UInt32) curFrameNumber, kIOReturnIsoTooNew);
            return kIOReturnIsoTooNew;
        }
        
        // Check to see how far in advance the frame is scheduled
        frameDiff = frameNumberStart - curFrameNumber;
        diff32 = (UInt32)frameDiff;
        if (diff32 < 2)
        {
            USBLog(5,"%s[%p]::UIMCreateIsochTransfer(LL) WARNING! - frameNumberStart less than 2 ms (is %ld)!  frameNumberStart: %ld, curFrameNumber: %ld",getName(), this, (UInt32) diff32, (UInt32) frameNumberStart, (UInt32) curFrameNumber);
        }
    }

    //
    //  Get the total size of buffer
    //
    for ( i = 0; i< frameCount; i++)
    {
        if (pFrames[i].frReqCount > kUSBMaxFSIsocEndpointReqCount)
        {
            USBLog(3,"%s[%p]::UIMCreateIsochTransfer(LL) Isoch frame (%d) too big %d",getName(), this, i + 1, pFrames[i].frReqCount);
            return kIOReturnBadArgument;
        }
        bufferSize += pFrames[i].frReqCount;
        
        // Make sure our frStatus field has a known value (debugging aid)
        //
        pFrames[i].frStatus = (IOReturn) kUSBLowLatencyIsochTransferKey;
    }

    USBLog(7,"%s[%p]::UIMCreateIsochTransfer(LL) transfer %s, buffer: %p, length: %d frames: %d, updateFreq: %ld",getName(), this, (direction == kOHCIEDDirectionIn) ? "in" : "out", pBuffer, bufferSize, frameCount, updateFrequency);

    //
    // go ahead and make sure we can grab at least ONE TD, before we lock the buffer	
    //
    pNewITD = AllocateITD();
    USBLog(7, "%s[%p]::UIMCreateIsochTransfer(LL) - new iTD %p", getName(), this, pNewITD);
    if (pNewITD == NULL)
    {
        USBLog(1,"%s[%p]::UIMCreateIsochTransfer(LL) Could not allocate a new iTD",getName(), this);
        return kIOReturnNoMemory;
    }

    if (!bufferSize) 
    {
	// Set up suitable dummy info
        numSegs = 1;
        segs[0].location = segs[0].length = 0;
	pageOffset = 0;
    }
    
    pTailITD = (AppleOHCIIsochTransferDescriptorPtr)pED->pLogicalTailP;	// start with the unused TD on the tail of the list
    OSWriteLittleInt32(&pTailITD->pShared->nextTD, 0, pNewITD->pPhysical);	// link in the new ITD
    pTailITD->pLogicalNext = pNewITD;

    needNewITD = false;
    transferOffset = 0;
    while (curFrameInRequest < frameCount) 
    {
        // Get physical segments for next frame
        if (!needNewITD && bufferSize && (pFrames[curFrameInRequest].frReqCount != 0) ) 
		{
            numSegs = _isoCursor->getPhysicalSegments(pBuffer, transferOffset, segs, 2, pFrames[curFrameInRequest].frReqCount);
            pageOffset = segs[0].location & kOHCIPageOffsetMask;
            transferOffset += segs[0].length;
            segmentEnd = (segs[0].location + segs[0].length )  & kOHCIPageOffsetMask;
			
            USBLog(8,"curFrameInRequest: %d, curFrameInTD: %d, pageOffset: %x, numSegs: %d, seg[0].location: %p, seg[0].length: %d", curFrameInRequest, curFrameInTD, pageOffset, numSegs, segs[0].location, segs[0].length);
			
            if(numSegs == 2)
            {
                transferOffset += segs[1].length;
                USBLog(8 ,"seg[1].location: %p, seg[1].length %d",segs[1].location, segs[1].length);
                
                // If we are wrapping around the same physical page and we are on an NEC controller, then we need to discard the 2nd segment.  It will click
                // but at least we won't hang the controller
                //
                if ( (_errataBits & kErrataNECOHCIIsochWraparound) && ((segs[0].location & kOHCIPageMask) == (segs[1].location & kOHCIPageMask)) )
                {
                    USBLog(1,"%s[%p]::UIMCreateIsochTransfer(LL) On an NEC controller and frame data wraps from end of buffer to beginning.  Dropping data to avoid controller hang");
                    numSegs = 1;
                }
            }
			
            if ( (segs[numSegs-1].location & kOHCIPageMask) != ((segs[numSegs-1].location + segs[numSegs-1].length) & kOHCIPageMask))
            {
                multiPageSegment = true;
                USBLog(8,"We have a segment that crosses a page boundary:  start: %p, length: %d, end: %p, curFrameinTD: %d", segs[numSegs-1].location, segs[numSegs-1].length,segs[numSegs-1].location + segs[numSegs-1].length, curFrameInTD);
            }
            else
                multiPageSegment = false;
        }

        if (curFrameInTD == 0) 
		{
            // set up counters which get reinitialized with each TD
            physPageStart = segs[0].location & kOHCIPageMask;	// for calculating real 13 bit offsets
            pageSelectMask = 0;					// First frame always starts on first page
            needNewITD = false;
			
            // set up the header of the TD - itdFlags will be stored into flags later
            itdFlags = (UInt16)(curFrameInRequest + frameNumber);
            pTailITD->pIsocFrame = (IOUSBIsocFrame *) pFrames;		// so we can get back to our info later
            pTailITD->frameNum = curFrameInRequest;	// our own index into the above array
            pTailITD->pType = tdType;			// So interrupt handler knows TD type.
            OSWriteLittleInt32(&pTailITD->pShared->bufferPage0, 0,  physPageStart);
        }
        else if ((segs[0].location & kOHCIPageMask) != physPageStart) 
		{
            // pageSelectMask is set if we've already used our one allowed page cross.
            //
            if ( (pageSelectMask && (((segs[0].location & kOHCIPageMask) != physPageEnd) || numSegs == 2)) )
			{
                // Need new ITD for this condition
                needNewITD = true;
				
                USBLog(8, "%s[%p]::UIMCreateIsochTransfer - got it! (%d, %p, %p, %d)", getName(), this, pageSelectMask, segs[0].location & kOHCIPageMask, physPageEnd, numSegs);
                
            }
            else if ( pageSelectMask && multiPageSegment )
            {
                // We have already crossed one page and we have a segment that spans 2 or more pages
                //
                needNewITD = true;
                USBLog(8,"%s[%p]::UIMCreateIsochTransfer This frame spans 2 or more pages and we already used our page crossing ",getName(), this);
            }
            else if ( (prevFramesPage != (segs[0].location & kOHCIPageMask)) && (segmentEnd != 0) )
            {
                // We have a segment that starts in a new page but the previous one did not end
                // on a page boundary.  Need a new ITD for this condition. 
                // Need new ITD for this condition
                needNewITD = true;
                
                USBLog(8,"%s[%p]::UIMCreateIsochTransfer(LL) This frame starts on a new page and the previous one did NOT end on a page boundary (%d)",getName(), this, segmentEnd);
            }
            else
			{
                if (pageSelectMask == 0 )
                    USBLog(8,"Using our page crossing for this TD (%p)",(segs[numSegs-1].location + segs[numSegs-1].length -1 ) & kOHCIPageMask);
				
                pageSelectMask = kOHCIPageSize;	// ie. set bit 13
                physPageEnd = (segs[numSegs-1].location  + segs[numSegs-1].length) & kOHCIPageMask;
            }
        }
        
        // Save this frame's Page so that we can use it when the next frame is process to compare and see
        // if they are different
        //
        prevFramesPage = (segs[numSegs-1].location  + segs[numSegs-1].length) & kOHCIPageMask;
        
        if ( (curFrameInTD > 7) || needNewITD || (useUpdateFrequency && (curFrameInTD >= updateFrequency)) ) 
		{
            // Need to start a new TD
            //
            itdFlags |= (curFrameInTD-1) << kOHCIITDControl_FCPhase;
            OSWriteLittleInt32(&pTailITD->pShared->bufferEnd, 0, lastPhysical);
            pNewITD = AllocateITD();
            USBLog(7, "%s[%p]::UIMCreateIsochTransfer(LL) - new iTD %p (curFrameInRequest: %d, curFrameInTD: %d, needNewITD: %d, updateFrequency: %d", getName(), this, pNewITD, curFrameInRequest, curFrameInTD, needNewITD, updateFrequency);
            if (pNewITD == NULL) 
			{
                curFrameInTD = 0;
                needNewITD = true;	// To simplify test at top of loop.
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
			
            curFrameInTD = 0;
            needNewITD = true;	// To simplify test at top of loop.
			
            OSWriteLittleInt32(&pTailITD->pShared->flags, 0, itdFlags);
			
            pTailITD->completion.action = NULL;
            //print_itd(pTailITD);
            pTailITD = pTailITD->pLogicalNext;		// this is the "old" pNewTD
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
                                (kOHCIITDOffsetConditionNotAccessed << kOHCIITDOffset_CCPhase) 	// mark as unused
                                );	

        // adjust counters and calculate the physical offset of the end of the frame for the next time around the loop
        //
        curFrameInRequest++;
        curFrameInTD++;
        lastPhysical = segs[numSegs-1].location + segs[numSegs-1].length - 1;
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
        
        //print_itd(pTailITD);
        // Make new descriptor the tail
        pED->pLogicalTailP = pNewITD;
        OSWriteLittleInt32(&pED->pShared->tdQueueTailPtr, 0, pNewITD->pPhysical);
    }


    return status;

}
