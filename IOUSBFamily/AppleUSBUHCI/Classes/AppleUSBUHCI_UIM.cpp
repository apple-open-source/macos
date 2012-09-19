/*
 * Copyright © 2004-2009 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <kern/clock.h>
#include <machine/limits.h>

#include <IOKit/usb/IOUSBRootHubDevice.h>
#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBUHCI.h"
#include "AppleUHCIListElement.h"
#include "USBTracepoints.h"


/*
 * UIM methods
 */
// ========================================================================
#pragma mark Control
// ========================================================================

IOReturn
AppleUSBUHCI::UIMCreateControlEndpoint(
                                       UInt8				functionNumber,
                                       UInt8				endpointNumber,
                                       UInt16				maxPacketSize,
                                       UInt8				speed,
                                       USBDeviceAddress    		highSpeedHub,
                                       int			        highSpeedPort)
{
#pragma unused (highSpeedHub, highSpeedPort)
   return UIMCreateControlEndpoint(functionNumber, endpointNumber, maxPacketSize, speed);
}



IOReturn
AppleUSBUHCI::UIMCreateControlEndpoint(UInt8				functionNumber,
                                       UInt8				endpointNumber,
                                       UInt16				maxPacketSize,
                                       UInt8				speed)
{
    AppleUHCIQueueHead					*pQH;
    AppleUHCIQueueHead					*prevQH;
	AppleUHCITransferDescriptor			*pTD;
    
    
    if (functionNumber == _rootFunctionNumber) 
	{
        return kIOReturnSuccess;
    }
    
    USBLog(3, "AppleUSBUHCI[%p]::UIMCreateControlEndpoint (f %d ep %d) max %d spd %d", this, functionNumber, endpointNumber, maxPacketSize, speed);

    if (maxPacketSize == 0) 
	{
		USBLog(1, "AppleUSBUHCI[%p]::UIMCreateControlEndpoint - maxPacketSize 0 is illegal (kIOReturnBadArgument)", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateControlEndpoint, functionNumber, endpointNumber, speed, kIOReturnBadArgument );
        return kIOReturnBadArgument;
    }
    
    USBLog(5, "AppleUSBUHCI[%p]::UIMCreateControlEndpoint allocating endpoint", this);
    pQH = AllocateQH(functionNumber, endpointNumber, kUSBNone, speed, maxPacketSize, kUSBControl);
    
    if (pQH == NULL)
        return kIOReturnNoMemory;
    
	pTD = AllocateTD(pQH);
	if (!pTD)
	{
		DeallocateQH(pQH);
		return kIOReturnNoMemory;
	}

	// this is a dummy TD which will be filled in when we create a transfer
	pTD->GetSharedLogical()->ctrlStatus = 0;						// make sure it is inactive
	pQH->firstTD = pTD;
	pQH->lastTD = pTD;
	pQH->GetSharedLogical()->elink = HostToUSBLong(pTD->GetPhysicalAddrWithType());
	
    // Now link the endpoint's queue head into the schedule
    if (speed == kUSBDeviceSpeedLow) 
	{
        prevQH = _lsControlQHEnd;
    } else 
	{
        prevQH = _fsControlQHEnd;
    }
    USBLog(5, "AppleUSBUHCI[%p]::UIMCreateControlEndpoint linking qh %p into schedule after %p", this, pQH, prevQH);
	
    pQH->_logicalNext = prevQH->_logicalNext;
    pQH->SetPhysicalLink(prevQH->GetPhysicalLink());
    IOSync();
    
    prevQH->_logicalNext = pQH;
    prevQH->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
    IOSync();
    if (speed == kUSBDeviceSpeedLow) 
	{
        _lsControlQHEnd = pQH;
    } else 
	{
        _fsControlQHEnd = pQH;
    }
    
    USBLog(3, "AppleUSBUHCI[%p]::UIMCreateControlEndpoint done pQH %p FN %d EP %d MPS %d", this, pQH, pQH->functionNumber, pQH->endpointNumber, pQH->maxPacketSize);

    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::UIMCreateControlTransfer(short				functionNumber,
                                       short				endpointNumber,
                                       IOUSBCommand*		command,
                                       IOMemoryDescriptor	*CBP,
                                       bool					bufferRounding,			// short packet OK
                                       UInt32				bufferSize,
                                       short				direction)
{
#pragma unused (bufferRounding)
    AppleUHCIQueueHead					*pQH;
    AppleUHCITransferDescriptor			*td, *last_td;
    IOReturn							status;
    
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateControlTransfer (f %d ep %d dir %d) size %d", this, functionNumber, endpointNumber, direction, (int)bufferSize);
    
    pQH = FindQueueHead(functionNumber, endpointNumber, kUSBAnyDirn, kUSBControl);
    if (pQH == NULL) 
	{
        USBLog(2, "AppleUSBUHCI[%p]::UIMCreateControlTransfer - queue head not found - FN(%d) EN(%d)", this, functionNumber, endpointNumber);
        return kIOUSBEndpointNotFound;
    }
    
    if (pQH->stalled) 
	{
		if (endpointNumber == 0)
		{
			USBError(1, "AppleUSBUHCI[%p]::UIMCreateControlTransfer - address %d endpoint 0 is incorrectly marked as stalled!", this, functionNumber);
		}
		else
		{
			USBLog(2, "AppleUSBUHCI[%p]::UIMCreateControlTransfer - Control pipe for ep %d stalled", this, endpointNumber);
			return kIOUSBPipeStalled;
		}
    }
    
	// control transactions may be coming from sources other than the device driver (e.g. Prober)
	// so restart the bus and let them through.. we will start it up again later
    // Here's how we will assemble the transaction:
	// There are up to three parts to a control transaction.  If the command says
	// that it's a multi-part transaction, and this is not the last part of the transaction,
	// assemble the parts in a queue but don't start it yet.
    
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateControlTransfer - allocating TD chain with CBP %p", this, CBP);
    status = AllocTDChain(pQH, command, CBP, bufferSize, direction, true);
    if (status != kIOReturnSuccess) 
	{
		USBLog(2, "AppleUSBUHCI[%p]::UIMCreateControlTransfer - returning status %p", this, (void*)status);
    }
    	
	USBLog(7, "AppleUSBUHCI[%p]::UIMCreateControlTransfer - pQH[%p] firstTD[%p] lastTD[%p] status[%p]", this, pQH, pQH->firstTD, pQH->lastTD, (void*)status);
	return status;
}


// ========================================================================
#pragma mark Bulk
// ========================================================================


IOReturn
AppleUSBUHCI::UIMCreateBulkEndpoint(UInt8				functionNumber,
                                    UInt8				endpointNumber,
                                    UInt8				direction,
                                    UInt8				speed,
                                    UInt16				maxPacketSize,
                                    USBDeviceAddress    highSpeedHub,
                                    int			        highSpeedPort)
{
#pragma unused (highSpeedHub, highSpeedPort)
    return UIMCreateBulkEndpoint(functionNumber, endpointNumber, direction, speed, maxPacketSize);
}



IOReturn
AppleUSBUHCI::UIMCreateBulkEndpoint(UInt8 functionNumber, UInt8 endpointNumber, UInt8 direction, UInt8 speed, UInt8	maxPacketSize)
{
    AppleUHCITransferDescriptor			*pTD;
    AppleUHCIQueueHead					*pQH;
    AppleUHCIQueueHead					*prevQH;
    
    USBLog(5, "AppleUSBUHCI[%p]::UIMCreateBulkEndpoint (fn %d ep %d dir %d) speed %d mp %d", this, functionNumber, endpointNumber, direction, speed, maxPacketSize);
    
    if (maxPacketSize == 0) 
	{
		USBLog(1, "AppleUSBUHCI[%p]::UIMCreateBulkEndpoint - maxPacketSize 0 is illegal (kIOReturnBadArgument)", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateBulkEndpoint, functionNumber, endpointNumber, direction, kIOReturnBadArgument );
        return kIOReturnBadArgument;
    }
    
    pQH = AllocateQH(functionNumber, endpointNumber, direction, speed, maxPacketSize, kUSBBulk);
    
    if (pQH == NULL) 
	{
        return kIOReturnNoMemory;
    }

    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateBulkEndpoint allocating dummy TD", this);
	pTD = AllocateTD(pQH);
	if (!pTD)
	{
		DeallocateQH(pQH);
		return kIOReturnNoMemory;
	}

	// this is a dummy TD which will be filled in when we create a transfer
	pTD->GetSharedLogical()->ctrlStatus = 0;						// make sure it is inactive
	pQH->firstTD = pTD;
	pQH->lastTD = pTD;
	pQH->GetSharedLogical()->elink = HostToUSBLong(pTD->GetPhysicalAddrWithType());
	
    // Now link the endpoint's queue head into the schedule
    prevQH = _bulkQHEnd;
    pQH->_logicalNext = prevQH->_logicalNext;
    pQH->SetPhysicalLink(prevQH->GetPhysicalLink());
    IOSync();
    
    prevQH->_logicalNext = pQH;
    prevQH->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
    IOSync();
    _bulkQHEnd = pQH;

    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::UIMCreateBulkTransfer(IOUSBCommand* command)
{
    AppleUHCIQueueHead				*pQH = NULL;
    IOMemoryDescriptor				*mp = NULL;
    IOReturn						status = 0xDEADBEEF;
    
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateBulkTransfer (%d, %d, %d) size %d", this, command->GetAddress(), command->GetEndpoint(), command->GetDirection(), (int)command->GetReqCount());
    
    pQH = FindQueueHead(command->GetAddress(), command->GetEndpoint(), command->GetDirection(), kUSBBulk);
    if (pQH == NULL) 
	{
        USBLog(2, "AppleUSBUHCI[%p]::UIMCreateBulkTransfer - endpoint (fn %d, ep %d, dir %d) not found", this, command->GetAddress(), command->GetEndpoint(), command->GetDirection() );
        return kIOUSBEndpointNotFound;
    }
    
    if (pQH->stalled) 
	{
        USBLog(4, "AppleUSBUHCI[%p]::UIMCreateBulkTransfer - Bulk pipe stalled", this);
        return kIOUSBPipeStalled;
    }
    mp = command->GetBuffer();
	
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateBulkTransfer - allocating TD chain with mp %p", this, mp);
    status = AllocTDChain(pQH, command, mp, command->GetReqCount(), command->GetDirection(), false);
    if (status != kIOReturnSuccess) 
	{
        USBLog(4, "AppleUSBUHCI[%p]::UIMCreateBulkTransfer - AllocTDChain returns %d", this, status);
        return status;
    }

    return kIOReturnSuccess;
}


// ========================================================================
#pragma mark Interrupt
// ========================================================================

IOReturn
AppleUSBUHCI::UIMCreateInterruptEndpoint(
                                         short				functionNumber,
                                         short				endpointNumber,
                                         UInt8				direction,
                                         short				speed,
                                         UInt16				maxPacketSize,
                                         short				pollingRate,
                                         USBDeviceAddress   highSpeedHub,
                                         int                highSpeedPort)
{
#pragma unused (highSpeedHub, highSpeedPort)
    return UIMCreateInterruptEndpoint(functionNumber, endpointNumber, direction, speed, maxPacketSize, pollingRate);
}



IOReturn
AppleUSBUHCI::UIMCreateInterruptEndpoint(short				functionNumber,
                                         short				endpointNumber,
                                         UInt8				direction,
                                         short				speed,
                                         UInt16				maxPacketSize,
                                         short				pollingRate)
{
    AppleUHCIQueueHead			*pQH, *prevQH;
	AppleUHCITransferDescriptor	*pTD;
    int							i;
	UInt32						currentToggle = 0;
    
    USBLog(3, "AppleUSBUHCI[%p]::UIMCreateInterruptEndpoint (fn %d, ep %d, dir %d) spd %d pkt %d rate %d", this, functionNumber, endpointNumber, direction, speed, maxPacketSize, pollingRate );
	
    if (functionNumber == _rootFunctionNumber) 
	{
        if (endpointNumber != 0 && endpointNumber != 1) 
		{
			USBLog(1, "AppleUSBUHCI[%p]::UIMCreateInterruptEndpoint - maxPacketSize 0 is illegal (kIOReturnBadArgument)", this);
			USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateInterruptEndpoint, functionNumber, endpointNumber, direction, kIOReturnBadArgument );
            return kIOReturnBadArgument;
        }
        return RootHubStartTimer32(pollingRate);
    }

    // If the interrupt already exists, then we need to delete it first, as we're probably trying
    // to change the Polling interval via SetPipePolicy().
    //
    pQH = FindQueueHead(functionNumber, endpointNumber, direction, kUSBInterrupt);
    if ( pQH != NULL )
    {
        IOReturn ret;
        USBLog(3, "AppleUSBUHCI[%p]::UIMCreateInterruptEndpoint endpoint already existed -- deleting it", this);
		
		currentToggle = USBToHostLong(pQH->firstTD->GetSharedLogical()->token) & kUHCI_TD_D;
		if ( currentToggle != 0)
		{
			USBLog(6,"AppleUSBOHCI[%p]::UIMCreateInterruptEndpoint:  Preserving a data toggle of 1 before of the EP that we are going to delete!", this);
		}
        ret = UIMDeleteEndpoint(functionNumber, endpointNumber, direction);
        if ( ret != kIOReturnSuccess)
        {
            USBLog(1, "AppleUSBUHCI[%p]::UIMCreateInterruptEndpoint deleting endpoint returned %p", this, (void*)ret);
			USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateInterruptEndpoint, functionNumber, endpointNumber, direction, ret );
            return ret;
        }
    }

	for (i=kUHCI_NINTR_QHS-1; i>=0; i--) 
	{
        if ((1 << i) <= pollingRate) 
		{
            break;
        }
    }
    if (i<0) 
	{
        i = 0;
    }
    USBLog(5, "AppleUSBUHCI[%p]::UIMCreateInterruptEndpoint - we will use interrupt queue %d, which corresponds to a rate of %d", this, i, (1 << i));
    
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateInterruptEndpoint allocating endpoint", this);
    pQH = AllocateQH(functionNumber, endpointNumber, direction, speed, maxPacketSize, kUSBInterrupt);
    
    if (pQH == NULL)
        return kIOReturnNoMemory;
    
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateInterruptEndpoint allocating dummy TD", this);
	pTD = AllocateTD(pQH);
	if (!pTD)
	{
		DeallocateQH(pQH);
		return kIOReturnNoMemory;
	}
	
	// this is a dummy TD which will be filled in when we create a transfer
	pTD->GetSharedLogical()->ctrlStatus = 0;							// make sure it is inactive
	pTD->GetSharedLogical()->token |= HostToUSBLong(currentToggle);		// Restore any data toggle from a deleted EP
	pQH->firstTD = pTD;
	pQH->lastTD = pTD;
	pQH->GetSharedLogical()->elink = HostToUSBLong(pTD->GetPhysicalAddrWithType());
	
    // Now link the endpoint's queue head into the schedule
	pQH->interruptSlot = i;
    prevQH = _intrQH[pQH->interruptSlot];
    pQH->_logicalNext = prevQH->_logicalNext;
    pQH->SetPhysicalLink(prevQH->GetPhysicalLink());
    IOSync();
    
    prevQH->_logicalNext = pQH;
    prevQH->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
	
    USBLog(3, "AppleUSBUHCI[%p]::UIMCreateInterruptEndpoint done pQH[%p]", this, pQH);

    return kIOReturnSuccess;
}



// method in 1.8.2
IOReturn
AppleUSBUHCI::UIMCreateInterruptTransfer(IOUSBCommand* command)
{
    AppleUHCIQueueHead				*pQH;
    IOReturn						status;
    IOMemoryDescriptor				*mp;
    IOByteCount						len;
	
    
    if (command->GetAddress() == _rootFunctionNumber)
    {
		IODMACommand			*dmaCommand = command->GetDMACommand();
		IOMemoryDescriptor		*memDesc = dmaCommand ? (IOMemoryDescriptor*)dmaCommand->getMemoryDescriptor() : NULL;
		
		if (memDesc)
		{
			USBLog(3, "AppleUSBUHCI[%p]::UIMCreateInterruptTransfer - root hub interrupt transfer - clearing unneeded memDesc (%p) from dmaCommand (%p)", this, memDesc, dmaCommand);
			dmaCommand->clearMemoryDescriptor();
		}
 		if (command->GetEndpoint() == 1)
		{
			status = RootHubQueueInterruptRead(command->GetBuffer(), command->GetReqCount(), command->GetUSLCompletion());
		}
		else
		{
			Complete(command->GetUSLCompletion(), kIOUSBEndpointNotFound, command->GetReqCount());
			status = kIOUSBEndpointNotFound;
		}
        return status;
    }
    
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateInterruptTransfer - adr=(%d,%d) len %d rounding %d", this, command->GetAddress(), command->GetEndpoint(), (int)command->GetReqCount(), command->GetBufferRounding());
    pQH = FindQueueHead(command->GetAddress(), command->GetEndpoint(), command->GetDirection(), kUSBInterrupt);
    if (pQH == NULL) 
	{
		USBLog(1, "AppleUSBUHCI[%p]::UIMCreateInterruptTransfer - QH not found", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateInterruptTransfer, command->GetAddress(), command->GetEndpoint(), command->GetDirection(), kIOUSBEndpointNotFound );
        return kIOUSBEndpointNotFound;
    }
    
    if (pQH->stalled) 
	{
        USBLog(1, "AppleUSBUHCI[%p]::UIMCreateInterruptTransfer - Interrupt pipe stalled", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateInterruptTransfer, command->GetAddress(), command->GetEndpoint(), command->GetDirection(), kIOUSBPipeStalled );
        return kIOUSBPipeStalled;
    }
        
	if ( pQH->maxPacketSize == 0 )
	{
		USBLog(2, "AppleUSBUHCI[%p]::UIMCreateInterruptTransfer - maxPacketSize is 0, returning kIOUSBNotEnoughBandwidth", this);
		return kIOReturnNoBandwidth;
	}

	mp = command->GetBuffer();
    len = command->GetReqCount();
    
#define INTERRUPT_TRANSFERS_ONE_PACKET 0
#if INTERRUPT_TRANSFERS_ONE_PACKET
    // Restrict interrupt transfers to one packet only.
    // This seems to help Bluetooth USB adapters work.
    if ((int)len > ep->maxPacketSize) 
	{
        len = ep->maxPacketSize;
    }
#endif
    
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateInterruptTransfer - allocating TD chain with mp %p", this, mp);
    status = AllocTDChain(pQH, command, mp, len, command->GetDirection(), false);
    if (status != kIOReturnSuccess) 
	{
        return status;
    }

    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateInterruptTransfer - done", this);
    return kIOReturnSuccess;
}


// ========================================================================
#pragma mark Isochronous
// ========================================================================


IOReturn
AppleUSBUHCI::UIMCreateIsochEndpoint(short				functionNumber,
                                     short				endpointNumber,
                                     UInt32				maxPacketSize,
                                     UInt8				direction,
                                     USBDeviceAddress   highSpeedHub,
                                     int                highSpeedPort)
{
#pragma unused (highSpeedHub, highSpeedPort)
    return UIMCreateIsochEndpoint(functionNumber, endpointNumber, maxPacketSize, direction);
}


IOReturn 
AppleUSBUHCI::UIMCreateIsochEndpoint(short					functionAddress,
									 short					endpointNumber,
									 UInt32					maxPacketSize,
									 UInt8					direction)
{
    IOUSBControllerIsochEndpoint*			pEP;
    UInt32									curMaxPacketSize;
    UInt32									xtraRequest;
	IOReturn								res;
    
	
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateIsochEndpoint(%d, %d, %d, %d)", this, functionAddress, endpointNumber, (uint32_t)maxPacketSize, direction);
	// see if the endpoint already exists - if so, this is a SetPipePolicy
    pEP = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);
	
    if (pEP) 
    {
        // this is the case where we have already created this endpoint, and now we are adjusting the maxPacketSize
        //
        USBLog(6,"AppleUSBUHCI[%p]::UIMCreateIsochEndpoint endpoint already exists, attempting to change maxPacketSize to %d", this, (uint32_t)maxPacketSize);
		
        curMaxPacketSize = pEP->maxPacketSize;
        if (maxPacketSize == curMaxPacketSize) 
		{
            USBLog(4, "AppleUSBUHCI[%p]::UIMCreateIsochEndpoint maxPacketSize (%d) the same, no change", this, (uint32_t)maxPacketSize);
            return kIOReturnSuccess;
        }
        if (maxPacketSize > curMaxPacketSize) 
		{
            // client is trying to get more bandwidth
            xtraRequest = maxPacketSize - curMaxPacketSize;
			if (xtraRequest > _isocBandwidth)
			{
				USBLog(1,"AppleUSBUHCI[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %d, available: %d", this, (uint32_t)xtraRequest, (uint32_t)_isocBandwidth);
				USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateIsochEndpoint, (uint32_t)xtraRequest, (uint32_t)_isocBandwidth, kIOReturnNoBandwidth, 1 );
				return kIOReturnNoBandwidth;
			}
			_isocBandwidth -= xtraRequest;
			USBLog(5, "AppleUSBUHCI[%p]::UIMCreateIsochEndpoint grabbing additional bandwidth: %d, new available: %d", this, (uint32_t)xtraRequest, (uint32_t)_isocBandwidth);
        } else 
		{
            // client is trying to return some bandwidth
            xtraRequest = curMaxPacketSize - maxPacketSize;
            _isocBandwidth += xtraRequest;
            USBLog(5, "AppleUSBUHCI[%p]::UIMCreateIsochEndpoint returning some bandwidth: %d, new available: %d", this, (uint32_t)xtraRequest, (uint32_t)_isocBandwidth);
        }
        pEP->maxPacketSize = maxPacketSize;
		
		USBLog(6,"AppleUSBUHCI[%p]::UIMCreateIsochEndpoint new size %d", this, (uint32_t)maxPacketSize);
        return kIOReturnSuccess;
    }
	else
	{
		USBLog(7,"AppleUSBUHCI[%p]::UIMCreateIsochEndpoint no endpoint", this);
	}

    
    // we neeed to create a new EP structure
	if (maxPacketSize > _isocBandwidth) 
	{
		USBLog(1,"AppleUSBUHCI[%p]::UIMCreateIsochEndpoint out of bandwidth, request (extra) = %d, available: %d", this, (uint32_t)maxPacketSize, (uint32_t)_isocBandwidth);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateIsochEndpoint, (uint32_t)maxPacketSize, (uint32_t)_isocBandwidth, kIOReturnNoBandwidth, 2 );
		return kIOReturnNoBandwidth;
	}
	
    pEP = CreateIsochronousEndpoint(functionAddress, endpointNumber, direction);

    if (pEP == NULL) 
        return kIOReturnNoMemory;

	pEP->maxPacketSize = maxPacketSize;
	USBLog(5,"AppleUSBUHCI[%p]::UIMCreateIsochEndpoint 2 size %d", this, (uint32_t)maxPacketSize);

    pEP->inSlot = kUHCI_NVFRAMES+1;
	
    _isocBandwidth -= maxPacketSize;
    USBLog(6, "AppleUSBUHCI[%p]::UIMCreateIsochEndpoint success. bandwidth used = %d, new available: %d", this, (uint32_t)maxPacketSize, (uint32_t)_isocBandwidth);
    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::DeleteIsochEP(IOUSBControllerIsochEndpoint* pEP)
{
    IOUSBControllerIsochEndpoint		*curEP, *prevEP;
    UInt32								currentMaxPacketSize;
	
    USBLog(7, "AppleUSBUHCI[%p]::DeleteIsochEP (%p)", this, pEP);
    if (pEP->activeTDs)
    {
		USBLog(6, "AppleUSBUHCI[%p]::DeleteIsochEP- there are still %d active TDs - aborting", this, (uint32_t)pEP->activeTDs);
		AbortIsochEP(pEP);
		if (pEP->activeTDs)
		{
			USBError(1, "AppleUSBUHCI[%p]::DeleteIsochEP- after abort there are STILL %d active TDs", this, (uint32_t) pEP->activeTDs);
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
	
    // need to return the bandwidth used
	_isocBandwidth += currentMaxPacketSize;
	USBLog(4, "AppleUSBUHCI[%p]::DeleteIsochEP returned bandwidth %d, new available: %d", this, (uint32_t)currentMaxPacketSize, (uint32_t)_isocBandwidth);	
	
	DeallocateIsochEP(pEP);

    return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::CreateIsochTransfer(IOUSBControllerIsochEndpoint* pEP, IOUSBIsocCommand *command)
{
    AppleUHCIIsochTransferDescriptor		*pNewITD=NULL;
    UInt64									maxOffset;
    UInt64									curFrameNumber = GetFrameNumber();
    UInt64									frameDiff;
    UInt32									diff32;
    IOByteCount								transferOffset;
    UInt32									bufferSize;
    UInt32									ioc = 0;
    UInt32									i;
    UInt32									pageOffset;
    IOPhysicalAddress						dmaStartAddr;
    IOByteCount								segLen;
	UInt32									myStatFlags;
	UInt32									myDirection;
	bool									lowLatency = command->GetLowLatency();
	UInt32									updateFrequency = command->GetUpdateFrequency();
	IOUSBIsocFrame *						pFrames = command->GetFrameList();
	IOUSBLowLatencyIsocFrame *				pLLFrames = (IOUSBLowLatencyIsocFrame *)pFrames;
	UInt32									frameCount = command->GetNumFrames();
	UInt64									frameNumberStart = command->GetStartFrame();
	IODMACommand *							dmaCommand = command->GetDMACommand();
	IOMemoryDescriptor *					pBuffer = command->GetBuffer();					// to use for alignment buffers
	UInt64									offset;
	IODMACommand::Segment64					segments64;
	UInt32									numSegments;
	IOReturn								status;
	
    USBLog(7, "AppleUSBUHCI[%p]::CreateIsochTransfer - frameCount %d - frameNumberStart %qd - curFrameNumber %qd", this, (uint32_t)frameCount, frameNumberStart, curFrameNumber);
    USBLog(7, "AppleUSBUHCI[%p]::CreateIsochTransfer - updateFrequency %d - lowLatency %d", this, (uint32_t)updateFrequency, lowLatency);
	
	if (!pEP)
	{
		USBError(1, "AppleUSBUHCI[%p]::CreateIsochTransfer - no endpoint", this);
		return kIOReturnBadArgument;
	}
	
    maxOffset = kUHCI_NVFRAMES;
    if (frameNumberStart < pEP->firstAvailableFrame)
    {
		USBLog(3,"AppleUSBUHCI[%p]::CreateIsochTransfer: no overlapping frames -   EP (%p) frameNumberStart: %qd, pEP->firstAvailableFrame: %qd.  Returning 0x%x", this, pEP, frameNumberStart, pEP->firstAvailableFrame, kIOReturnIsoTooOld);
		return kIOReturnIsoTooOld;
    }
    pEP->firstAvailableFrame = frameNumberStart;
	
	switch (pEP->direction)
	{
		case kUSBIn:
			myDirection = kUHCI_TD_PID_IN;
			break;
			
		case kUSBOut:
			myDirection = kUHCI_TD_PID_OUT;
			break;
			
		default:
			USBLog(3,"AppleUSBUHCI[%p]::CreateIsochTransfer: bad direction(%d) in pEP[%p]", this, pEP->direction, pEP);
			return kIOReturnBadArgument;
	}
	
	
    if (frameNumberStart <= curFrameNumber)
    {
        if (frameNumberStart < (curFrameNumber - maxOffset))
        {
            USBLog(3,"AppleUSBUHCI[%p]::CreateIsochTransfer request frame WAY too old.  frameNumberStart: %qd, curFrameNumber: %qd.  Returning 0x%x", this, frameNumberStart, curFrameNumber, kIOReturnIsoTooOld);
            return kIOReturnIsoTooOld;
        }
        USBLog(5,"AppleUSBUHCI[%p]::CreateIsochTransfer WARNING! curframe later than requested, expect some notSent errors!  frameNumberStart: %qd, curFrameNumber: %qd.  USBIsocFrame Ptr: %p, First ITD: %p", this, frameNumberStart, curFrameNumber, pFrames, pEP->toDoEnd);
    } else 
    {					// frameNumberStart > curFrameNumber
        if (frameNumberStart > (curFrameNumber + maxOffset))
        {
            USBLog(3,"AppleUSBUHCI[%p]::CreateIsochTransfer request frame too far ahead!  frameNumberStart: %qd, curFrameNumber: %qd", this, frameNumberStart, curFrameNumber);
            return kIOReturnIsoTooNew;
        }
        frameDiff = frameNumberStart - curFrameNumber;
        diff32 = (UInt32)frameDiff;
        if (diff32 < 2)
        {
            USBLog(5,"AppleUSBUHCI[%p]::CreateIsochTransfer WARNING! - frameNumberStart less than 2 ms (is %d)!  frameNumberStart: %qd, curFrameNumber: %qd", this, (uint32_t)diff32, frameNumberStart, curFrameNumber);
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
            if (pFrames[i].frReqCount > pEP->maxPacketSize)
            {
                USBLog(1,"AppleUSBUHCI[%p]::CreateIsochTransfer - Isoch frame (%d) too big (%d) MPS (%d)", this, (uint32_t)(i + 1), pFrames[i].frReqCount, (uint32_t)pEP->maxPacketSize);
				USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateIsochTransfer, (uint32_t)(i + 1), pFrames[i].frReqCount, (uint32_t)pEP->maxPacketSize, 1 );
                return kIOReturnBadArgument;
            }
            bufferSize += pFrames[i].frReqCount;
        } else
        {
            if (pLLFrames[i].frReqCount > pEP->maxPacketSize)
            {
                USBLog(1,"AppleUSBUHCI[%p]::CreateIsochTransfer(LL) - Isoch frame (%d) too big (%d) MPS (%d)", this, (uint32_t)(i + 1), pLLFrames[i].frReqCount, (uint32_t)pEP->maxPacketSize);
				USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateIsochTransfer, (uint32_t)(i + 1), pLLFrames[i].frReqCount, (uint32_t)pEP->maxPacketSize, 2 );
                return kIOReturnBadArgument;
            }
            bufferSize += pLLFrames[i].frReqCount;
            
			// Make sure our frStatus field has a known value.  This is used by the client to know whether the transfer has been completed or not
            //
            pLLFrames[i].frStatus = command->GetIsRosettaClient() ? (IOReturn) OSSwapInt32(kUSBLowLatencyIsochTransferKey) : (IOReturn) kUSBLowLatencyIsochTransferKey;
        }
    }
	
    // Format all the TDs, attach them to the pseudo endpoint.
    // let the frame interrupt routine put them in the periodic list
	
    transferOffset = 0;
	
    // Do this one frame at a time
    for ( i = 0; i < frameCount; i++)
    {
		UInt16			reqCount;
		UInt32			token, ctrlStatus;
		
        pNewITD = AllocateITD();
		if (lowLatency)
			reqCount = pLLFrames[i].frReqCount;
		else
			reqCount = pFrames[i].frReqCount;
	    
        USBLog(7, "AppleUSBUHCI[%p]::CreateIsochTransfer - new iTD %p size (%d)", this, pNewITD, reqCount);
        if (!pNewITD)
        {
            USBLog(1,"AppleUSBUHCI[%p]::CreateIsochTransfer Could not allocate a new iTD", this);
			USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateIsochTransfer, (uintptr_t)this, (uintptr_t)pNewITD, reqCount, 3 );
            return kIOReturnNoMemory;
        }
		
		pEP->firstAvailableFrame++;
        pNewITD->_lowLatency = lowLatency;
		
		// set up the physical page pointers
		
		offset = transferOffset;
		numSegments = 1;
		status = dmaCommand->gen64IOVMSegments(&offset, &segments64, &numSegments);
		if (status || (numSegments != 1) || ((UInt32)(segments64.fIOVMAddr >> 32) > 0) || ((UInt32)(segments64.fLength >> 32) > 0))
		{
			USBError(1, "AppleUSBUHCI[%p]::CreateIsochTransfer - could not generate segments err (%p) numSegments (%d) fIOVMAddr (0x%qx) fLength (0x%qx)", this, (void*)status, (int)numSegments, segments64.fIOVMAddr, segments64.fLength);
			status = status ? status : kIOReturnInternalError;
			dmaStartAddr = 0;
			segLen = 0;
		}
		else
		{
			dmaStartAddr = (IOPhysicalAddress)segments64.fIOVMAddr;
			segLen = (UInt32)segments64.fLength;
		}			

		// TODO - use an alignment buffer if necessary for this frame
		if (segLen > reqCount)
		{
			segLen = reqCount;
		}
				
		if (segLen < reqCount)
		{
			UHCIAlignmentBuffer		*bp  = GetIsochAlignmentBuffer();
			
			if (!bp)
			{
				//  If we run out of alignment buffers, we need to return an error and unravel the TDs that we have created for this transfer.  That will need to wait until
				//  rdar://4595324.  For now, just break out of the loop while adjusting the reqCount. This will cause errors in the stream, but we won't panic.
				USBLog(1, "AppleUSBUHCI[%p]:CreateIsochTransfer - could not get the alignment buffer I needed", this);
				USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateIsochTransfer, (uintptr_t)this, command->GetAddress(), command->GetEndpoint(), 4 );
				USBError(1,"The USB UHCI driver could not get the resources it needed.  Transfers will be affected (%d:%d)", command->GetAddress(), command->GetEndpoint());
				reqCount = segLen;
				// return kIOReturnNoResources;
				break;
			}
			
			USBLog(6, "AppleUSBUHCI[%p]:CreateIsochTransfer - using alignment buffer %p at pPhysical %p instead of %p direction %s EP %d", this, (void*)bp->vaddr, (void*)bp->paddr, (void*)dmaStartAddr, (pEP->direction == kUSBOut) ? "OUT" : "IN", pEP->endpointNumber);
			pNewITD->alignBuffer = bp;
			dmaStartAddr = bp->paddr;
			if (pEP->direction == kUSBOut) 
			{
				pBuffer->readBytes(transferOffset, (void *)bp->vaddr, reqCount);
			}
			bp->userBuffer = pBuffer;
			bp->userOffset = transferOffset;
			bp->dmaCommand = OSDynamicCast(AppleUSBUHCIDMACommand, dmaCommand);
			bp->actCount = 0;
		}

		transferOffset += reqCount;
		bufferSize -= reqCount;

        pNewITD->_pFrames = pFrames;
        pNewITD->_frameNumber = frameNumberStart + i;
		pNewITD->_frameIndex = i;
                
        // calculate IOC and completion if necessary
		if (i == (frameCount-1))
		{
			ioc = kUHCI_TD_IOC;
			pNewITD->_completion = command->GetUSLCompletion();
		}
		else if (lowLatency)
		{
			if (!updateFrequency)
				ioc = (((i+1) % 8) == 0) ? (UInt32)kUHCI_TD_IOC : 0;
			else
				ioc = (((i+1) % updateFrequency) == 0) ? (UInt32)kUHCI_TD_IOC : 0;
		}
		else
			ioc = 0;
		
		pNewITD->GetSharedLogical()->buffer = HostToUSBLong(dmaStartAddr);
        pNewITD->SetPhysicalLink(kUHCI_QH_T);
		pNewITD->_lowLatency = lowLatency;
		
		token = myDirection | UHCI_TD_SET_MAXLEN(reqCount) | UHCI_TD_SET_ENDPT(pEP->endpointNumber) | UHCI_TD_SET_ADDR(pEP->functionAddress);
		pNewITD->GetSharedLogical()->token = HostToUSBLong(token);
		
		ctrlStatus = kUHCI_TD_ACTIVE | kUHCI_TD_ISO | UHCI_TD_SET_ACTLEN(0) | ioc;
		pNewITD->GetSharedLogical()->ctrlStatus = HostToUSBLong(ctrlStatus);
		
		pNewITD->_pEndpoint = pEP;
		
		pNewITD->_requestFromRosettaClient = command->GetIsRosettaClient();
		
		PutTDonToDoList(pEP, pNewITD);
    }
	USBLog(7, "AppleUSBUHCI[%p]::CreateIsochTransfer - calling AddIsochFramesToSchedule", this);
	AddIsochFramesToSchedule(pEP);
	return kIOReturnSuccess;
}


IOReturn
AppleUSBUHCI::UIMCreateIsochTransfer(short					functionNumber,
                                     short					endpointNumber,
                                     IOUSBIsocCompletion	completion,
                                     UInt8					direction,
                                     UInt64					frameStart,
                                     IOMemoryDescriptor		*pBuffer,
                                     UInt32					frameCount,
                                     IOUSBIsocFrame			*pFrames)
{
#pragma unused (functionNumber, endpointNumber, completion, direction, frameStart, pBuffer, frameCount, pFrames)
	USBError(1, "AppleUSBUHCI::UIMCreateIsochTransfer(LL) - old method");
	return kIOReturnIPCError;
	
	/*****
		IOUSBControllerIsochEndpoint*		pEP;
	bool								requestFromRosettaClient = false;
	
    USBLog(7, "AppleUSBUHCI[%p]::UIMCreateIsochTransfer - adr=%d:%d cbp=%p:%lx (cback=[%lx:%lx:%lx])", this,  
		   functionNumber, endpointNumber, pBuffer, 
		   pBuffer->getLength(), 
		   (UInt32)completion.action, (UInt32)completion.target, 
		   (UInt32)completion.parameter);
	
    if ( (frameCount == 0) || (frameCount > 1000) )
    {
        USBLog(3,"AppleUSBUHCI[%p]::UIMCreateIsochTransfer bad frameCount: %ld", this, frameCount);
        return kIOReturnBadArgument;
    }
	
	// Determine if our request came from a rosetta client
	if ( direction & 0x80 )
	{
		requestFromRosettaClient = true;
		direction &= ~0x80;
	}
	
    pEP = FindIsochronousEndpoint(functionNumber, endpointNumber, direction, NULL);
    if (pEP == NULL)
    {
        USBLog(1, "AppleUSBUHCI[%p]::UIMCreateIsochTransfer - Endpoint not found", this);
        return kIOUSBEndpointNotFound;        
    }
    
	return CreateIsochTransfer(pEP, command);
	*****/
}



IOReturn 
AppleUSBUHCI::UIMCreateIsochTransfer(short						functionNumber,
                                     short						endpointNumber,
                                     IOUSBIsocCompletion		completion,
                                     UInt8						direction,
                                     UInt64						frameStart,
                                     IOMemoryDescriptor			*pBuffer,
                                     UInt32						frameCount,
                                     IOUSBLowLatencyIsocFrame	*pFrames,
                                     UInt32						updateFrequency)
{
#pragma unused (functionNumber, endpointNumber, completion, direction, frameStart, pBuffer, frameCount, pFrames, updateFrequency)
	USBError(1, "AppleUSBUHCI::UIMCreateIsochTransfer(LL) - old method");
	return kIOReturnIPCError;
	
}


IOReturn 
AppleUSBUHCI::UIMCreateIsochTransfer(IOUSBIsocCommand * command)
{
	IOReturn								err;
    IOUSBControllerIsochEndpoint  *			pEP;
    UInt64									maxOffset;
    UInt64									curFrameNumber = GetFrameNumber();
    UInt64									frameDiff;
    UInt32									diff32;

	USBLog(7, "AppleUSBUHCI[%p]::UIMCreateIsochTransfer - adr=%d:%d IOMD=%p, frameStart: %qd, count: %d, pFrames: %p", this,  
		   command->GetAddress(), command->GetEndpoint(), 
		   command->GetBuffer(), 
		   command->GetStartFrame(), (uint32_t)command->GetNumFrames(), command->GetFrameList());
	

    if ( (command->GetNumFrames() == 0) || (command->GetNumFrames() > 1000) )
    {
        USBLog(3,"AppleUSBUHCI[%p]::UIMCreateIsochTransfer bad frameCount: %d", this, (uint32_t)command->GetNumFrames());
        return kIOReturnBadArgument;
    }
	
    pEP = FindIsochronousEndpoint(command->GetAddress(), command->GetEndpoint(), command->GetDirection(), NULL);
	
    if (pEP == NULL)
    {
        USBLog(1, "AppleUSBUHCI[%p]::UIMCreateIsochTransfer - Endpoint not found", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCreateIsochTransfer, (uintptr_t)this, command->GetAddress(), command->GetEndpoint(), 5 );
        return kIOUSBEndpointNotFound;        
    }
    
	return CreateIsochTransfer(pEP, command);
}


// ========================================================================
#pragma mark Endpoints
// ========================================================================


IOReturn 
AppleUSBUHCI::HandleEndpointAbort(short functionAddress, short endpointNumber, short direction, bool clearToggle)
{
    AppleUHCIQueueHead						*pQH, *pQHPrev;
	UInt32									link = 0;
	IOUSBControllerIsochEndpoint			*pIsochEP;
	AppleUHCITransferDescriptor				*savedFirstTD = NULL;
	AppleUHCITransferDescriptor				*savedLastTD = NULL;
    
    USBLog(4, "AppleUSBUHCI[%p]::HandleEndpointAbort: Addr: %d, Endpoint: %d,%d", this, functionAddress, endpointNumber, direction);
	
    if (functionAddress == _rootFunctionNumber)
    {
        if ( (endpointNumber != 1) && (endpointNumber != 0) )
        {
            USBLog(1, "AppleUSBUHCI[%p]::HandleEndpointAbort: bad params - endpNumber: %d", this, endpointNumber );
			USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMHandleEndpointAbort, (uintptr_t)this, endpointNumber, direction, kIOReturnBadArgument );
            return kIOReturnBadArgument;
        }
        
        // We call SimulateEDDelete (endpointNumber, direction) in 9
        //
        USBLog(5, "AppleUSBUHCI[%p]::HandleEndpointAbort: Attempting operation on root hub", this);
        return RHAbortEndpoint(endpointNumber, direction);
    }
	
    pIsochEP = FindIsochronousEndpoint(functionAddress, endpointNumber, direction, NULL);
    if (pIsochEP)
    {
		return AbortIsochEP(pIsochEP);
    }
    
	if (pQH = FindQueueHead(functionAddress, endpointNumber, direction, kUSBControl, &pQHPrev), pQH)
	{
		USBLog(5, "AppleUSBUHCI[%p]::HandleEndpointAbort - Found control queue head %p prev %p", this, pQH, pQHPrev);
	}
	else if (pQH = FindQueueHead(functionAddress, endpointNumber, direction, kUSBInterrupt, &pQHPrev), pQH)
	{
		USBLog(5, "AppleUSBUHCI[%p]::HandleEndpointAbort - Found interrupt queue head %p prev %p", this, pQH, pQHPrev);
	}
	else if (pQH = FindQueueHead(functionAddress, endpointNumber, direction, kUSBBulk, &pQHPrev), pQH)
	{
		USBLog(5, "AppleUSBUHCI[%p]::HandleEndpointAbort - Found bulk queue head %p prev %p", this, pQH, pQHPrev);
	}
	
    if (pQH == NULL) 
	{
		USBLog(1, "AppleUSBUHCI[%p]::HandleEndpointAbort - endpoint not found", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMHandleEndpointAbort, (uintptr_t)this, endpointNumber, direction, kIOUSBEndpointNotFound );
        return kIOUSBEndpointNotFound;
    }
	
	if (pQH->aborting)
	{
		// 7946083 - don't allow the abort to recurse
		USBLog(1, "AppleUSBUHCI[%p]::HandleEndpointAbort - endpoint [%p] currently aborting. not recursing", this, pQH);
		return kIOUSBClearPipeStallNotRecursive;
	}
	
	pQH->aborting = true;
	
	// we don't need to handle any TDs if the queue is empty, so check for that..
    if (pQH->firstTD != pQH->lastTD)			// There are transactions on this queue
    {
		UInt32		nextToggle = 0;
		
		// we need to unlink the QH, since we can't modify the elink field if it is active
		// don't call UnlinkQueueHead, since we don't want to change the markers
		if (pQHPrev)
		{
			link = pQHPrev->GetPhysicalLink();						// save the link to ourself
			pQHPrev->SetPhysicalLink(pQH->GetPhysicalLink());		// temporarily change the physical link
			IOSleep(1);												// make sure the HW is not looking at it
		}
        USBLog(4, "AppleUSBUHCI[%p]::HandleEndpointAbort: removing TDs", this);
		savedFirstTD = pQH->firstTD;
		savedLastTD = pQH->lastTD;
        pQH->firstTD = pQH->lastTD;
		
		if (!clearToggle)
		{
			nextToggle = USBToHostLong(savedFirstTD->GetSharedLogical()->token) & kUHCI_TD_D;
		}
		
		if (nextToggle != 0)
		{
			USBLog(6, "AppleUSBUHCI[%p]::HandleEndpointAbort  Preserving a data toggle of 1 in response to an Abort()!", this);
		}
		
		// this is actually the dummy TD, which will have the same value as the first TD used to
		pQH->firstTD->GetSharedLogical()->token = HostToUSBLong(nextToggle);

		pQH->GetSharedLogical()->elink = HostToUSBLong(pQH->firstTD->GetPhysicalAddrWithType());
		IOSync();
		
		if (pQHPrev)
			pQHPrev->SetPhysicalLink(link);							// restore it to the original value
    }
	else
	{
		if ( (USBToHostLong(pQH->firstTD->GetSharedLogical()->token) & kUHCI_TD_D) && !clearToggle )
		{
			USBLog(6, "AppleUSBUHCI[%p]::HandleEndpointAbort  Preserving a data toggle of 1 in response to an Abort()!", this);
		}
		
		if (clearToggle)
			pQH->firstTD->GetSharedLogical()->token = 0;	// this will reset the data toggle

		pQH->GetSharedLogical()->elink = HostToUSBLong(pQH->firstTD->GetPhysicalAddrWithType());
		IOSync();
	}
	pQH->stalled = false;

    USBLog(4, "AppleUSBUHCI[%p]::HandleEndpointAbort: Addr: %d, Endpoint: %d,%d - calling DoDoneQueue", this, functionAddress, endpointNumber, direction);
	UHCIUIMDoDoneQueueProcessing(savedFirstTD, kIOUSBTransactionReturned, savedLastTD);
	
	pQH->aborting = false;
    return kIOReturnSuccess;
	
}



IOReturn 
AppleUSBUHCI::UIMAbortEndpoint(short functionAddress, short endpointNumber, short direction)
{
    USBLog(3, "AppleUSBUHCI[%p]::UIMAbortEndpoint - endpoint %d:%d,%d", this, functionAddress, endpointNumber, direction);
    return HandleEndpointAbort(functionAddress, endpointNumber, direction, false);
}



IOReturn 
AppleUSBUHCI::UIMClearEndpointStall(short functionAddress, short endpointNumber, short direction)
{
    
    USBLog(5, "AppleUSBUHCI[%p]::UIMClearEndpointStall - endpoint %d:%d,%d", this, functionAddress, endpointNumber, direction);
    return  HandleEndpointAbort(functionAddress, endpointNumber, direction, true);
}



IOReturn
AppleUSBUHCI::UIMDeleteEndpoint(short functionNumber, short	endpointNumber, short direction)
{
    AppleUHCIQueueHead					*pQH;
	IOUSBControllerIsochEndpoint*		pIsochEP;
    int									i;
	AppleUHCIQueueHead					*pQHPrev = NULL;
	IOReturn							err;

    USBLog(3, "AppleUSBUHCI[%p]::UIMDeleteEndpoint %d %d %d", this, functionNumber, endpointNumber, direction);
        
    if (functionNumber == _rootFunctionNumber) 
	{
        if (endpointNumber != 0 && endpointNumber != 1) 
		{
			USBLog(1, "AppleUSBUHCI[%p]::UIMDeleteEndpoint - not ep 0 or ep 1 [%d](kIOReturnBadArgument)", this, endpointNumber);
			USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMDeleteEndpoint, (uintptr_t)this, endpointNumber, direction, kIOReturnBadArgument );
            return kIOReturnBadArgument;
        }
        return RHDeleteEndpoint(endpointNumber, direction);
    }

    pIsochEP = FindIsochronousEndpoint(functionNumber, endpointNumber, direction, NULL);
    if (pIsochEP)
		return DeleteIsochEP(pIsochEP);
	
	if (pQH = FindQueueHead(functionNumber, endpointNumber, direction, kUSBControl, &pQHPrev), pQH)
	{
		USBLog(7, "AppleUSBUHCI[%p]::UIMDeleteEndpoint - Found control queue head %p prev %p", this, pQH, pQHPrev);
	}
	else if (pQH = FindQueueHead(functionNumber, endpointNumber, direction, kUSBInterrupt, &pQHPrev), pQH)
	{
		USBLog(7, "AppleUSBUHCI[%p]::UIMDeleteEndpoint - Found interrupt queue head %p prev %p", this, pQH, pQHPrev);
	}
	else if (pQH = FindQueueHead(functionNumber, endpointNumber, direction, kUSBBulk, &pQHPrev), pQH)
	{
		USBLog(7, "AppleUSBUHCI[%p]::UIMDeleteEndpoint - Found bulk queue head %p prev %p", this, pQH, pQHPrev);
	}
	
    if (pQH == NULL) 
	{
		USBLog(1, "AppleUSBUHCI[%p]::UIMDeleteEndpoint - endpoint not found", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMDeleteEndpoint, (uintptr_t)this, endpointNumber, direction, kIOUSBEndpointNotFound );
        return kIOUSBEndpointNotFound;
    }
			
	err = UnlinkQueueHead(pQH, pQHPrev);
	if (err)
	{
		USBLog(1, "AppleUSBUHCI[%p]::UIMDeleteEndpoint - err %p unlinking endpoint", this, (void*)err);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMDeleteEndpoint, (uintptr_t)this, err, 0, 0);
	}
	
    if (pQH->firstTD != pQH->lastTD)			// There are transactions on this queue
    {
        USBLog(5, "AppleUSBUHCI[%p]::UIMDeleteEndpoint: removing TDs", this);
        UHCIUIMDoDoneQueueProcessing(pQH->firstTD, kIOUSBTransactionReturned, pQH->lastTD);
        pQH->firstTD = pQH->lastTD;
    }
	
	if ( pQH->firstTD != NULL )
	{
		// I need to delete the dummy TD
		USBLog(7, "AppleUSBUHCI[%p]::UIMDeleteEndpoint - deallocating the dummy TD", this);
		DeallocateTD(pQH->firstTD);
		pQH->firstTD = NULL;
    }
	
    USBLog(7, "AppleUSBUHCI[%p]::UIMDeleteEndpoint: Deallocating %p", this, pQH);
    DeallocateQH(pQH);
	
    return kIOReturnSuccess;
}



AppleUHCIQueueHead *
AppleUSBUHCI::FindQueueHead(short functionNumber, short endpointNumber, UInt8 direction, UInt8 type, AppleUHCIQueueHead **ppQHPrev)
{
    AppleUHCIQueueHead		*pQH, *firstQH, *lastQH, *prevQH = NULL;
	
    USBLog(7, "AppleUSBUHCI[%p]::FindQueueHead(%d, %d, %d, %d)", this, functionNumber, endpointNumber, direction, type);
	
	// first check the disabled list
	pQH = _disabledQHList;
	while (pQH)
	{
		if ((pQH->functionNumber == functionNumber) && (pQH->endpointNumber == endpointNumber) && ((direction == kUSBAnyDirn) || (pQH->direction == direction)) && (pQH->type == type))
		{
			USBLog(2, "AppleUSBUHCI[%p]::FindQueueHead - found Queue Head[%p] on the DISABLED list - returning NULL", this, pQH);
			return NULL;
#if 0			
			if (ppQHPrev)
				*ppQHPrev = prevQH;
			return pQH;
#endif
		}
		prevQH = pQH;
		pQH = OSDynamicCast(AppleUHCIQueueHead, pQH->_logicalNext);
	}
	
	// it is not disabled - look on the regular list
	switch (type)
	{
		case kUSBInterrupt:
			firstQH = _intrQH[kUHCI_NINTR_QHS-1];
			lastQH = _lsControlQHStart;
			break;
			
		case kUSBControl:
			firstQH = _lsControlQHStart;
			lastQH = _bulkQHStart;
			break;
			
		case kUSBBulk:
			firstQH = _bulkQHStart;
			lastQH = _lastQH;
			break;
				
		default:
			firstQH = lastQH  = NULL;
	}

    pQH = firstQH;
	while (pQH && (pQH != lastQH))
	{
		USBLog(7, "AppleUSBUHCI[%p]::FindQueueHead - looking at queue head %p (%d, %d, %d, %d)", this, pQH, pQH->functionNumber, pQH->endpointNumber, pQH->direction, pQH->type);
		if ((pQH->functionNumber == functionNumber) && (pQH->endpointNumber == endpointNumber) && ((direction == kUSBAnyDirn) || (pQH->direction == direction)) && (pQH->type == type))
		{
			USBLog(7, "AppleUSBUHCI[%p]::FindQueueHead - found Queue Head %p", this, pQH);
			if (ppQHPrev)
				*ppQHPrev = prevQH;
			return pQH;
		}
		prevQH = pQH;
		pQH = OSDynamicCast(AppleUHCIQueueHead, pQH->_logicalNext);
	}
    USBLog(7, "AppleUSBUHCI[%p]::FindQueueHead - endpoint not found", this);
    return NULL;
}



IOReturn
AppleUSBUHCI::UnlinkQueueHead(AppleUHCIQueueHead *pQH, AppleUHCIQueueHead *pQHBack)
{
	if (!pQH || !pQHBack)
	{
		USBLog(1, "AppleUSBUHCI[%p]::UnlinkQueueHead - invalid params (%p, %p)", this, pQH, pQHBack);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMUnlinkQueueHead, (uintptr_t)this, (uintptr_t)pQH, (uintptr_t)pQHBack, kIOReturnBadArgument );
		return kIOReturnBadArgument;
	}
	USBLog(7, "AppleUSBUHCI[%p]::UnlinkQueueHead(%p, %p)", this, pQH, pQHBack);
	// need to back out the end markers if appropriate
	if (pQH == _lsControlQHEnd)
		_lsControlQHEnd = pQHBack;
	else if (pQH == _fsControlQHEnd)
		_fsControlQHEnd = pQHBack;
	else if (pQH == _bulkQHEnd)
		_bulkQHEnd = pQHBack;
		
	// change the hardware and software link pointers
	pQHBack->SetPhysicalLink(pQH->GetPhysicalLink());
	pQHBack->_logicalNext = pQH->_logicalNext;
	
	// there is no doorbell in UHCI like in EHCI
	// we need to make sure that the queue head is not cached in the controller
	// the easiest way to do so it to just wait 1 ms for the next frame
	IOSleep(1);
	
	return kIOReturnSuccess;
}



// ========================================================================
#pragma mark Transaction starting and completing
// ========================================================================


#define	kUHCIUIMScratchFirstActiveFrame	0

void 
AppleUSBUHCI::UIMCheckForTimeouts(void)
{
    AbsoluteTime					currentTime, t;
    UInt64							elapsedTime;
    UInt64							frameNumber;
    UInt16							status, cmd, intr;
	AppleUHCIQueueHead				*pQH = NULL, *pQHBack = NULL, *pQHBack1 = NULL;
	AppleUHCITransferDescriptor		*pTD = NULL;
	IOPhysicalAddress				pTDPhys;
    UInt32							noDataTimeout;
    UInt32							completionTimeout;
    UInt32							curFrame;
	UInt32							rem;
	bool							logging = false;
	int								loopCount = 0;
	uint64_t						tempTime;

    if (isInactive() || (_myBusState != kUSBBusStateRunning) || _wakingFromHibernation)
	{
        return;
    }
	
    // Check to see if we missed an interrupt.
	USBLog(7, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - calling ProcessCompletedTransactions", this);
	ProcessCompletedTransactions();
	
 	tempTime = mach_absolute_time();
	currentTime = *(AbsoluteTime*)&tempTime;
   
    status = ioRead16(kUHCI_STS);
    cmd = ioRead16(kUHCI_CMD);
    intr = ioRead16(kUHCI_INTR);
	
	// this code probably doesn't work
    if (status & kUHCI_STS_HCH) 
	{
        // acknowledge
        ioWrite16(kUHCI_STS, kUHCI_STS_HCH);
        
        USBError(1, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - Host controller halted, resetting", this);
        Reset(true);
        Run(true);
    }
    
    // Adjust 64-bit frame number.
	// This is a side-effect of GetFrameNumber().
    frameNumber = GetFrameNumber();
    
    
    _lastTimeoutFrameNumber = frameNumber;
    _lastFrameNumberTime = currentTime;

	for (pQH = _lsControlQHStart; pQH && (loopCount++ < 100); pQH = OSDynamicCast(AppleUHCIQueueHead, pQH->_logicalNext))
	{
		if (pQH == pQH->_logicalNext)
		{
			USBError(1,"AppleUSBUHCI[%p]::UIMCheckForTimeouts  pQH (%p) linked to itself", this, pQH);
		}
		
		// Need to keep a note of the previous ED for back links. Usually I'd
		// put a pEDBack = pED at the end of the loop, but there are lots of 
		// continues in this loop so it was getting skipped (and unlinking the
		// entire async schedule). These lines get the ED from the previous 
		// interation in pEDBack.
		pQHBack = pQHBack1;
		pQHBack1 = pQH;
		
		if (pQH->type == kQHTypeDummy)
			continue;
		
		logging = true;
		USBLog(7, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - checking QH [%p]", this, pQH);
		pQH->print(7);

		// OHCI gets phys pointer and logicals that, that seems a little complicated, so
		// I'll get the logical pointer and compare it to the phys. If they're different,
		// this transaction has only just got to the head and the previous one(s) haven't
		// been scavenged yet. Assume its not a good candidate for a timeout.
		
		// get the top TD
		pTDPhys = USBToHostLong(pQH->GetSharedLogical()->elink);
		pTD = pQH->firstTD;
		
		if (!pTD)
		{
			USBLog(3, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - no TD", this);
			continue;
		}
		
		if (!pTD->command)
		{
			USBLog(7, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - found a TD without a command - moving on", this);
			continue;
		}

		if (pTD == pQH->lastTD)
		{
			USBLog(1, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - ED (%p) - TD is TAIL but there is a command - pTD (%p)", this, pQH, pTD);
			USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCheckForTimeouts, (uintptr_t)this, (uintptr_t)pQH, (uintptr_t)pTD, 1 );
			pQH->print(1);
		}
		
		if (pTDPhys != pTD->GetPhysicalAddrWithType())
		{
			USBLog(6, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - pED (%p) - mismatched logical and physical - TD (%p) will be scavenged later", this, pQH, pTD);
			pQH->print(7);
			pTD->print(7);
			continue;
		}
		
		noDataTimeout = pTD->command->GetNoDataTimeout();
		completionTimeout = pTD->command->GetCompletionTimeout();
		curFrame = GetFrameNumber32();
		
		if (completionTimeout)
		{
			UInt32	firstActiveFrame = pTD->command->GetUIMScratch(kUHCIUIMScratchFirstActiveFrame);
			if (!firstActiveFrame)
			{
				pTD->command->SetUIMScratch(kUHCIUIMScratchFirstActiveFrame, curFrame);
				continue;
			}
			if ((curFrame - firstActiveFrame) >= completionTimeout)
			{
				USBLog(2, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - Found a TD [%p] on QH [%p] past the completion deadline, timing out! (0x%x - 0x%x)", this, pTD, pQH, (uint32_t)curFrame, (uint32_t)firstActiveFrame);
				USBError(1, "AppleUSBUHCI[%p]::Found a transaction past the completion deadline on bus 0x%x, timing out! (Addr: %d, EP: %d)", this, (uint32_t) _busNumber, pQH->functionNumber, pQH->endpointNumber );
				pQH->print(2);
				ReturnOneTransaction(pTD, pQH, pQHBack, kIOUSBTransactionTimeout);
				continue;
			}
		}
		
		if (!noDataTimeout)
			continue;
		
		if (!pTD->lastFrame || (pTD->lastFrame > curFrame))
		{
			// this pTD is not a candidate yet, remember the frame number and go on
			pTD->lastFrame = curFrame;
			pTD->lastRemaining = findBufferRemaining(pQH);
			continue;
		}
		rem = findBufferRemaining(pQH);
		
		if (pTD->lastRemaining != rem)
		{
			// there has been some activity on this TD. update and move on
			pTD->lastRemaining = rem;
			continue;
		}
		if ((curFrame - pTD->lastFrame) >= noDataTimeout)
		{
			USBLog(2, "AppleUSBUHCI[%p]UIMCheckForTimeouts:  Found a transaction (%p) which hasn't moved in 5 seconds, timing out! (0x%x - 0x%x)(CMD:%p STS:%p INTR:%p PORTSC1:%p PORTSC2:%p FRBASEADDR:%p ConfigCMD:%p)", this, pTD, (uint32_t)curFrame, (uint32_t)pTD->lastFrame, (void*)ioRead16(kUHCI_CMD), (void*)ioRead16(kUHCI_STS), (void*)ioRead16(kUHCI_INTR), (void*)ioRead16(kUHCI_PORTSC1), (void*)ioRead16(kUHCI_PORTSC2), (void*)ioRead32(kUHCI_FRBASEADDR), (void*)_device->configRead16(kIOPCIConfigCommand));
			//PrintFrameList(curFrame & kUHCI_NVFRAMES_MASK, 7);
			USBError(1, "AppleUSBUHCI[%p]::Found a transaction which hasn't moved in 5 seconds on bus 0x%x, timing out! (Addr: %d, EP: %d)", this, (uint32_t) _busNumber, pQH->functionNumber, pQH->endpointNumber );
			pQH->print(2);
			pTD->print(2);
			ReturnOneTransaction(pTD, pQH, pQHBack, kIOUSBTransactionTimeout);
			//printED(pED);
			//printTD(pTD);
			//printAsyncQueue();
			
			continue;
		}
	}
	
	if (loopCount > 99)
	{
		USBLog(1,"AppleUSBUHCI[%p]::UIMCheckForTimeouts  Too many loops around", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMCheckForTimeouts, (uintptr_t)this, loopCount, 0, 2 );
	}
	
	if (logging)
	{
		USBLog(7, "AppleUSBUHCI[%p]::UIMCheckForTimeouts - done", this);
	}
}



void 
AppleUSBUHCI::ReturnOneTransaction(AppleUHCITransferDescriptor		*pTD,
								   AppleUHCIQueueHead				*pQH,
								   AppleUHCIQueueHead				*pQHBack,
								   IOReturn							err)
{
	AppleUHCITransferDescriptor		*pTDFirst = pTD;
	
	// momentarily take the queue head out of the queue list so we can modify the transaction
	pQHBack->SetPhysicalLink(pQH->GetPhysicalLink());
	IOSleep(1);								// have to make sure the controller doesn't have it in the cache
		
    while(pTD != NULL)
    {
		if (pTD->callbackOnTD)
		{
			if (!pTD->multiXferTransaction)
			{
				USBLog(2, "AppleUSBUHCI[%p]::ReturnOneTransaction - found the end of a non-multi transaction(%p)!", this, pTD);
				break;
			}
			// this is a multi-TD transaction (control) - check to see if we are at the end of it
			else if (pTD->finalXferInTransaction)
			{
				USBLog(2, "AppleUSBUHCI[%p]::ReturnOneTransaction - found the end of a MULTI transaction(%p)!", this, pTD);
				break;
			}
			else
			{
				USBLog(2, "AppleUSBUHCI[%p]::ReturnOneTransaction - returning the non-end of a MULTI transaction(%p)!", this, pTD);
				// keep going around the loop - this is a multiXfer transaction and we haven't found the end yet
			}
		}

		pTD = OSDynamicCast(AppleUHCITransferDescriptor, pTD->_logicalNext);
    }
    if (pTD == NULL)
    {
		// This works, sort of, NULL for an end transction means remove them all.
		// But there will be no callback
		USBLog(1, "AppleUSBUHCI[%p]::ReturnOneTransaction - got to the end with no callback", this);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMReturnOneTransaction, (uintptr_t)this, (uintptr_t)pTD, 0, 0 );
    }
    else
    {
		pTD = OSDynamicCast(AppleUHCITransferDescriptor, pTD->_logicalNext);
		pQH->GetSharedLogical()->elink = HostToUSBLong(pTD->GetPhysicalAddrWithType());
		IOSync();
		pQH->firstTD = pTD;
		pQHBack->SetPhysicalLink(pQH->GetPhysicalAddrWithType());						// link us back in
    }
	USBLog(5, "AppleUSBUHCI[%p]::ReturnOneTransaction - returning transactions from (%p) to, but not including(%p)", this, pTDFirst, pTD);
    UHCIUIMDoDoneQueueProcessing(pTDFirst, err, pTD);
}



UInt32 
AppleUSBUHCI::findBufferRemaining(AppleUHCIQueueHead *pQH)
{
	UInt32							ctrlStatus, token, bufferSizeRemaining = 0;
	AppleUHCITransferDescriptor		*pTD = pQH->firstTD;
	
	while (pTD && (pTD != pQH->lastTD))
	{
		ctrlStatus = USBToHostLong(pTD->GetSharedLogical()->ctrlStatus);
		token = USBToHostLong(pTD->GetSharedLogical()->token);
		bufferSizeRemaining += (UHCI_TD_GET_MAXLEN(token) - UHCI_TD_GET_ACTLEN(ctrlStatus));
		if (pTD->callbackOnTD)
			break;
		pTD = OSDynamicCast(AppleUHCITransferDescriptor, pTD->_logicalNext);
	}
	
	return bufferSizeRemaining;
}



IOReturn
AppleUSBUHCI::TDToUSBError(UInt32 status)
{
    IOReturn result;
    
    status &= kUHCI_TD_ERROR_MASK;
    if (status == 0) 
	{
        result = kIOReturnSuccess;
    } else if (status & kUHCI_TD_CRCTO) 
	{
        // In this case, the STALLED bit is also set
        result = kIOReturnNotResponding;
    } else if (status & kUHCI_TD_BABBLE) 
	{
        // In this case, the STALLED bit is probably also set
        result = kIOReturnOverrun;
    } else if (status & kUHCI_TD_STALLED) 
	{
        result = kIOUSBPipeStalled;
    } else if (status & kUHCI_TD_DBUF)
	{
        result = kIOReturnOverrun;
    } else if (status & kUHCI_TD_CRCTO) 
	{
        result = kIOUSBCRCErr;
    } else if (status & kUHCI_TD_BITSTUFF) 
	{
        result = kIOUSBBitstufErr;
    } else 
	{
        result = kIOUSBTransactionReturned;
    }
    return result;
}



// ========================================================================
#pragma mark Transaction descriptors
// ========================================================================


IOReturn
AppleUSBUHCI::AllocTDChain(AppleUHCIQueueHead* pQH, IOUSBCommand *command, IOMemoryDescriptor* CBP, UInt32 bufferSize, UInt16 direction, Boolean controlTransaction)
{
	
    AppleUHCITransferDescriptor			*pTD1, *pTD, *pTDnew, *pTDLast;
    UInt32								myToggle = 0;
    UInt32								myDirection = 0;
    IOByteCount							transferOffset;
    UInt32								token;
	UInt32								ctrlStatus;
    IOReturn							status = kIOReturnSuccess;
    UInt32								maxPacket;
    UInt32								totalPhysLength;
    IOPhysicalAddress					dmaStartAddr;
    UInt32								bytesToSchedule;
	IODMACommand						*dmaCommand = NULL;
	UInt64								offset;
	IODMACommand::Segment64				segments64;
	UInt32								numSegments;
				
	/* *********** Note: Always put the flags in the TD last. ************** */
	/* *********** This is what kicks off the transaction if  ************** */
	/* *********** the next and alt pointers are not set up   ************** */
	/* *********** then the controller will pick up and cache ************** */
	/* *********** crap for the TD.                           ************** */
	
    if (controlTransaction)
    {
		if (direction != kUSBNone)
		{												// Setup phase uses Data 0, data phase & status phase use Data1 
            myToggle = kUHCI_TD_D;						// use Data1 
		}
    }
	else
	{
		myToggle = USBToHostLong(pQH->lastTD->GetSharedLogical()->token) & kUHCI_TD_D;
	}
	
	switch (direction)
	{
		case kUSBIn:
			myDirection = kUHCI_TD_PID_IN;
			break;
			
		case kUSBOut:
			myDirection = kUHCI_TD_PID_OUT;
			break;
			
		default:
			myDirection = kUHCI_TD_PID_SETUP;
	}
    maxPacket   =  pQH->maxPacketSize;
	if ((bufferSize > 0) && (maxPacket == 0))
	{
		USBError(1, "AppleUSBUHCI[%p]::AllocTDChain - buffserSize %d with maxPacket %d", this, (int)bufferSize, (int)maxPacket);
		return kIOReturnNoResources;
	}
	
    // First allocate the first of the new bunch
    pTD1 = AllocateTD(pQH);
	
    if (pTD1 == NULL)
    {
		USBError(1, "AppleUSBUHCI[%p]::AllocTDChain - can't allocate 1st new TD", this);
		return kIOReturnNoMemory;
    }
    pTD = pTD1;	// We'll be working with pTD
	
	ctrlStatus = kUHCI_TD_ACTIVE | UHCI_TD_SET_ERRCNT(3) | UHCI_TD_SET_ACTLEN(0);

	// if an IN token, allow short packet detect
	if (direction == kUSBIn)
		ctrlStatus |= kUHCI_TD_SPD;
	
	// If device is low speed, set LS bit in status.
	if (pQH->speed == kUSBDeviceSpeedLow) 
		ctrlStatus |= kUHCI_TD_LS;
	
	if (CBP && bufferSize)
	{
		dmaCommand = command->GetDMACommand();
		if (!dmaCommand)
		{
			USBError(1, "AppleUSBUHCI[%p]::AllocTDChain - no dmaCommand", this);
			DeallocateTD(pTD1);
			return kIOReturnInternalError;
		}
		if (dmaCommand->getMemoryDescriptor() != CBP)
		{
			USBError(1, "AppleUSBUHCI[%p]::AllocTDChain - mismatched CBP (%p) and dmaCommand memory descriptor (%p)", this, CBP, dmaCommand->getMemoryDescriptor());
			DeallocateTD(pTD);
			return kIOReturnInternalError;
		}
	}
	
    if (bufferSize != 0)
    {	    
        transferOffset = 0;
        while (transferOffset < bufferSize)
        {
			offset = transferOffset;
			numSegments = 1;
			
			status = dmaCommand->gen64IOVMSegments(&offset, &segments64, &numSegments);
			if (status || (numSegments != 1))
			{
				USBError(1, "AppleUSBUHCI[%p]::AllocTDChain - could not generate segments err (%p) offset (%d) transferOffset (%d) bufferSize (%d) getMemoryDescriptor (%p)", this, (void*)status, (int)offset, (int)transferOffset, (int)bufferSize, dmaCommand->getMemoryDescriptor());
				status = status ? status : kIOReturnInternalError;
				return status;
			}
			
			if (((UInt32)(segments64.fIOVMAddr >> 32) > 0) || ((UInt32)(segments64.fLength >> 32) > 0))
			{
				USBError(1, "AppleUSBUHCI[%p]::AllocTDChain - generated segment not 32 bit -  offset (0x%qx) length (0x%qx) ", this, segments64.fIOVMAddr, segments64.fLength);
				return kIOReturnInternalError;
			}
			
			dmaStartAddr = (IOPhysicalAddress)segments64.fIOVMAddr;
			totalPhysLength = (UInt32)segments64.fLength;
		
			USBLog(7, "AppleUSBUHCI[%p]::AllocTDChain - gen64IOVMSegments returned length of %d (out of %d) and start of 0x%x", this, (uint32_t)totalPhysLength, (uint32_t)bufferSize, (uint32_t)dmaStartAddr);
			bytesToSchedule = 0;
			if ((bufferSize-transferOffset) > maxPacket)
			{
				bytesToSchedule = maxPacket;
			}
			else 
			{
				bytesToSchedule = (bufferSize-transferOffset);
			}
				
			if (totalPhysLength < bytesToSchedule)
			{
                UHCIAlignmentBuffer *bp;
                
                // Use alignment buffer
                bp = GetCBIAlignmentBuffer();
				if (!bp)
				{
					USBError(1, "AppleUSBUHCI[%p]:AllocTDChain - could not get the alignment buffer I needed", this);
					return kIOReturnNoResources;
				}
                USBLog(1, "AppleUSBUHCI[%p]:AllocTDChain - pTD (%p) using UHCIAlignmentBuffer (%p) paddr (%p) instead of CBP (%p) dmaStartAddr (%p) totalPhysLength (%d) bytesToSchedule (%d)", this, pTD, bp, (void*)bp->paddr, CBP, (void*)dmaStartAddr, (int)totalPhysLength, (int)bytesToSchedule);
				USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAllocateTDChain, (uintptr_t)this, (uintptr_t)pTD, (uintptr_t)bp, 1);
				USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAllocateTDChain, (uintptr_t)this, bp->paddr, (uintptr_t)CBP, 2);
				USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAllocateTDChain, (uintptr_t)this, dmaStartAddr, totalPhysLength, bytesToSchedule);
                pTD->alignBuffer = bp;
                dmaStartAddr = bp->paddr;
                if (direction != kUSBIn) 
				{
                    CBP->readBytes(transferOffset, (void *)bp->vaddr, bytesToSchedule);
                }
                bp->userBuffer = CBP;
                bp->userOffset = transferOffset;
				bp->actCount = 0;
			}
			
			transferOffset += bytesToSchedule;
			pTD->direction = direction;
			pTD->GetSharedLogical()->buffer = HostToUSBLong(dmaStartAddr);
						
			token = myDirection | myToggle | UHCI_TD_SET_MAXLEN(bytesToSchedule) | UHCI_TD_SET_ENDPT(pQH->endpointNumber) | UHCI_TD_SET_ADDR(pQH->functionNumber);
			pTD->GetSharedLogical()->token = HostToUSBLong(token);
			myToggle = myToggle ? 0 : (int)kUHCI_TD_D;
			
			USBLog(7, "AppleUSBUHCI[%p]::AllocTDChain - putting command into TD (%p) on QH (%p)", this, pTD, pQH);
			pTD->command = command;								// Do like OHCI, link to command from each TD
            if (transferOffset >= bufferSize)
            {
				ctrlStatus |= kUHCI_TD_IOC;
				pTD->callbackOnTD = true;
				pTD->logicalBuffer = CBP;
				pTD->GetSharedLogical()->ctrlStatus = HostToUSBLong(ctrlStatus & ~kUHCI_TD_SPD);		// make sure the SPD bit is clear on the last TD (rdar://4464945)
				pTD->multiXferTransaction = command->GetMultiTransferTransaction();
				pTD->finalXferInTransaction = command->GetFinalTransferInTransaction();
            }
			else
            {
				pTD->callbackOnTD = false;
				pTDnew = AllocateTD(pQH);
				if (pTDnew == NULL)
				{
					status = kIOReturnNoMemory;
					USBError(1, "AppleUSBUHCI[%p]::AllocTDChain can't allocate new TD", this);
				}
				else
				{
					pTD->SetPhysicalLink(pTDnew->GetPhysicalAddrWithType());
					pTD->_logicalNext = pTDnew;																	// if (trace)printTD(pTD);
					pTD->GetSharedLogical()->ctrlStatus = HostToUSBLong(ctrlStatus);
					pTD = pTDnew;
					USBLog(7, "AppleUSBUHCI[%p]::AllocTDChain - got another TD - going to fill it up too (%d, %d)", this, (uint32_t)transferOffset, (uint32_t)bufferSize);
				}
            }
        }
    }
    else
    {
		// no buffer to transfer
		USBLog(7, "AppleUSBUHCI[%p]::AllocTDChain - (no buffer)- putting command into TD (%p) on QH (%p)", this, pTD, pQH);
		pTD->command = command;								// Do like OHCI, link to command from each TD
		ctrlStatus |= kUHCI_TD_IOC;
		pTD->callbackOnTD = true;
		pTD->multiXferTransaction = command->GetMultiTransferTransaction();
		pTD->finalXferInTransaction = command->GetFinalTransferInTransaction();
		pTD->logicalBuffer = CBP;
		token = myDirection | myToggle | UHCI_TD_SET_MAXLEN(0) | UHCI_TD_SET_ENDPT(pQH->endpointNumber) | UHCI_TD_SET_ADDR(pQH->functionNumber);
		pTD->GetSharedLogical()->token = HostToUSBLong(token);
		pTD->GetSharedLogical()->ctrlStatus = HostToUSBLong(ctrlStatus);
		myToggle = myToggle ? 0 : (int)kUHCI_TD_D;
	}
	
    pTDLast = pQH->lastTD;
	
	pTD->SetPhysicalLink(pTD1->GetPhysicalAddrWithType());
	pTD->_logicalNext = pTD1;
	
    // We now have a new chain of TDs. link it in.
    // pTD1, pointer to first TD
    // pTD, pointer to last TD
    // pTDLast is last currently on endpoint, will be made first
	
    ctrlStatus = pTD1->GetSharedLogical()->ctrlStatus;
	
    pTD1->GetSharedLogical()->ctrlStatus = 0;										// turn off the active bit before we make it live
	
    // Copy contents of first TD to old tail
    pTDLast->SetPhysicalLink(pTD1->GetPhysicalLink());
    pTDLast->GetSharedLogical()->ctrlStatus = pTD1->GetSharedLogical()->ctrlStatus;
	pTDLast->GetSharedLogical()->token = pTD1->GetSharedLogical()->token;
	pTDLast->GetSharedLogical()->buffer = pTD1->GetSharedLogical()->buffer;
    
    USBLog(7, "AppleUSBUHCI[%p]::AllocTDChain - transfering command from TD (%p) to TD (%p)", this, pTD1, pTDLast);
    pTDLast->command = pTD1->command;
    pTDLast->callbackOnTD = pTD1->callbackOnTD;
    pTDLast->multiXferTransaction = pTD1->multiXferTransaction;
    pTDLast->finalXferInTransaction = pTD1->finalXferInTransaction;

    pTDLast->_logicalNext = pTD1->_logicalNext;
    pTDLast->logicalBuffer = pTD1->logicalBuffer;
	if (pTD1->alignBuffer)
	{
		USBLog(1, "AppleUSBUHCI[%p]::AllocTDChain - moving alignBuffer from TD (%p) to TD (%p)", this, pTD1, pTDLast);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAllocateTDChain, (uintptr_t)this, (uintptr_t)pTD1, (uintptr_t)pTDLast, 3);
	}
	pTDLast->alignBuffer = pTD1->alignBuffer;

    // Point end of new TDs to first TD, now new tail
    pTD->SetPhysicalLink(pTD1->GetPhysicalAddrWithType());
    pTD->_logicalNext = pTD1;
    
    // squash pointers on new tail
    pTD1->SetPhysicalLink(kUHCI_TD_T);
	pTD1->GetSharedLogical()->token = HostToUSBLong(myToggle);							// the last - invalid TD - contains the correct toggle for the next TD
	pTD1->GetSharedLogical()->buffer = 0;												// smash the buffer
    pTD1->_logicalNext = NULL;
	pTD1->alignBuffer = NULL;
    USBLog(7, "AppleUSBUHCI[%p]::AllocTDChain - zeroing out command in  TD (%p)", this, pTD1);
    pTD1->command = NULL;
    
    pQH->lastTD = pTD1;
    pTDLast->GetSharedLogical()->ctrlStatus = ctrlStatus;
	USBLog(7, "AllocTDChain - TD list for QH %p firstTD %p lastTD %p ================================================", pQH, pQH->firstTD, pQH->lastTD);
	pTD = pQH->firstTD;
	while (pTD)
	{
		pTD->print(7);
		pTD = (AppleUHCITransferDescriptor*)pTD->_logicalNext;
	}
	USBLog(7, "AllocTDChain - end of chain =========================================================");
    if (status)
    {
		USBLog(2, "AppleUSBUHCI[%p]::AllocTDChain: returning status 0x%x", this, status);
    }
	
	if ((pQH->type == kUSBControl) || (pQH->type == kUSBBulk))
	{
		if (!_controlBulkTransactionsOut)
		{
			UInt32 link;
			link = _lastQH->GetPhysicalLink();
			USBLog(7, "AppleUSBUHCI[%p]::AllocTDChain - first transaction - unblocking list (%p to %p)", this, (void*)link, (void*)(link & ~kUHCI_QH_T));
			_lastQH->SetPhysicalLink(link & ~kUHCI_QH_T);
		}
		_controlBulkTransactionsOut++;
		USBLog(7, "AppleUSBUHCI[%p]::AllocTDChain - _controlBulkTransactionsOut(%p)", this, (void*)_controlBulkTransactionsOut);
	}
    return status;
}



void 
AppleUSBUHCI::AddIsochFramesToSchedule(IOUSBControllerIsochEndpoint* pEP)
{
    UInt64									currFrame, startFrame, finFrame;
    IOUSBControllerIsochListElement			*pTD = NULL;
    UInt16									nextSlot, firstOutSlot;
	uint64_t								currentTime;
	
    if (pEP->toDoList == NULL)
    {
		USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - no frames to add fn:%d EP:%d", this, pEP->functionAddress, pEP->endpointNumber);
		return;
    }
    if (pEP->aborting)
    {
		USBLog(1, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - EP (%p) is aborting - not adding", this, pEP);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAddIsochFramesToSchedule, (uintptr_t)this, pEP->functionAddress, pEP->endpointNumber, 1);
		return;
    }

	// 4211382 - This routine is already non-reentrant, since it runs on the WL.
	// However, we also need to disable preemption while we are in here, since we have to get everything
	// done within a couple of milliseconds, and if we are preempted, we may come back long after that
	// point. So take a SimpleLock to prevent preemption
	if (!IOSimpleLockTryLock(_isochScheduleLock))
	{
		// This would indicate reentrancy, which should never ever happen
		USBError(1, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - could not obtain scheduling lock", this);
		return;
	}
	
	//*******************************************************************************************************
	// ************* WARNING WARNING WARNING ****************************************************************
	// Preemption is now off, which means that we cannot make any calls which may block
	// So don't call USBLog, and don't call for example
	//*******************************************************************************************************
	
    // Don't get GetFrameNumber() unless we're going to use it
    //
    currFrame = GetFrameNumber();
	startFrame = currFrame;
    
    // USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - fn:%d EP:%d inSlot (0x%x), currFrame: 0x%qx", this, pEP->functionAddress, pEP->endpointNumber, pEP->inSlot, currFrame);
	
 	currentTime = mach_absolute_time();

	// USBLog(7, "AddIsochFramesToSchedule - first Todo frame (%qd), currFrame+1(%qd)", pEP->toDoList->_frameNumber, (currFrame+1));
    while(pEP->toDoList->_frameNumber <= (currFrame+1))		// Add 1, and use <= so you never put in a new frame 
															// at less than 2 ahead of now.
    {
		IOReturn	ret;
		UInt64		newCurrFrame;
		
		// this transaction is old before it began, move to done queue
		pTD = GetTDfromToDoList(pEP);
		//USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - ignoring TD(%p) because it is too old (%qx) vs (%qx) ", this, pTD, pTD->_frameNumber, currFrame);
		ret = pTD->UpdateFrameList(*(AbsoluteTime *)&currentTime);		// TODO - accumulate the return values
		if (pEP->scheduledTDs > 0)
			PutTDonDeferredQueue(pEP, pTD);
		else
		{
			//USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - putting TD(%p) on Done Queue instead of Deferred Queue ", this, pTD);
			PutTDonDoneQueue(pEP, pTD, true);
		}
	    
        //USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - pTD = %p", this, pTD);
		if (pEP->toDoList == NULL)
		{	
			// Run out of transactions to move.  Call this on a separate thread so that we return to the caller right away
            // 
			// ReturnIsocDoneQueue(pEP);
			IOSimpleLockUnlock(_isochScheduleLock);
			// OK to call USBLog, now that preemption is reenabled
            USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - calling the ReturnIsocDoneQueue on a separate thread", this);
            thread_call_enter1(_returnIsochDoneQueueThread, (thread_call_param_t) pEP);
			return;
		}
		newCurrFrame = GetFrameNumber();
		if (newCurrFrame > currFrame)
		{
			//USBLog(1, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - Current frame moved (0x%qx->0x%qx) resetting", this, currFrame, newCurrFrame);
			USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAddIsochFramesToSchedule, (uintptr_t)this, currFrame, newCurrFrame, 2);
			currFrame = newCurrFrame;
		}		
    }
    
	firstOutSlot = (currFrame+1) & kUHCI_NVFRAMES_MASK;							// this will be used if the _outSlot is not yet initialized
	
    currFrame = pEP->toDoList->_frameNumber;									// start looking at the first available number
	
    // This needs to be fixed up when we have variable length lists.
    pEP->inSlot = currFrame & kUHCI_NVFRAMES_MASK;
	
    do
    {		
		nextSlot = (pEP->inSlot + 1) & kUHCI_NVFRAMES_MASK;
		if (pEP->inSlot == _outSlot)
		{
			//USBLog(2, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - caught up pEP->inSlot (0x%x) _outSlot (0x%x)", this, pEP->inSlot, _outSlot);
			break;
		}
		if ( nextSlot == _outSlot) 								// weve caught up with our tail
		{
			//USBLog(2, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - caught up nextSlot (0x%x) _outSlot (0x%x)", this, nextSlot, _outSlot);
			break;
		}
		
		pTD = GetTDfromToDoList(pEP);
		//USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - checking TD(%p) FN(0x%qx) against currFrame (0x%qx)", this, pTD, pTD->_frameNumber, currFrame);
		
		if (currFrame == pTD->_frameNumber)
		{			
			if (_outSlot > kUHCI_NVFRAMES)
			{
				_outSlot = firstOutSlot;
			}
			// Place TD in list
			//USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - linking TD (%p) with frame (0x%qd) into slot (0x%x) - curr next log (%p) phys (%p)", this, pTD, pTD->_frameNumber, pEP->inSlot, _logicalFrameList[pEP->inSlot], (void*)USBToHostLong(_frameList[pEP->inSlot]));
			//pTD->print(7);
			
			pTD->SetPhysicalLink(USBToHostLong(_frameList[pEP->inSlot]));
			pTD->_logicalNext = _logicalFrameList[pEP->inSlot];
			_logicalFrameList[pEP->inSlot] = pTD;
			_frameList[pEP->inSlot] = HostToUSBLong(pTD->GetPhysicalAddrWithType());			
			OSIncrementAtomic( &(pEP->scheduledTDs) );
			//USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - _frameList[%x]:%x", this, pEP->inSlot, USBToHostLong(_frameList[pEP->inSlot]));
		}
		//		else
		//		{
		//			USBError(1, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - expected frame (%qd) and see frame (%qd) - should do something here!!", this, currFrame, pTD->_frameNumber);
		//		}
		
		currFrame++;
		pEP->inSlot = nextSlot;
		
		// USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - pEP->inSlot is now 0x%x", this, pEP->inSlot);	
    } while(pEP->toDoList != NULL);
	
	finFrame = GetFrameNumber();
	// Unlock, reenable preemption, so we can log
	IOSimpleLockUnlock(_isochScheduleLock);
	if ((finFrame - startFrame) > 1)
		USBError(1, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - end -  startFrame(0x%qd) finFrame(0x%qd)", this, startFrame, finFrame);
    USBLog(7, "AppleUSBUHCI[%p]::AddIsochFramesToSchedule - finished,  currFrame: %qd", this, GetFrameNumber() );
}



IOReturn 
AppleUSBUHCI::AbortIsochEP(IOUSBControllerIsochEndpoint* pEP)
{
    UInt32								slot;
    IOReturn							err;
    IOUSBControllerIsochListElement		*pTD;
    uint64_t							timeStamp;
    
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(6, "+AppleUSBUHCI[%p]::AbortIsochEP[%p] - start - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP,  (uint32_t)_outSlot, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)_consumerCount, (uint32_t)_producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
	}
	
    USBLog(7, "+AppleUSBUHCI[%p]::AbortIsochEP (%p)", this, pEP);
	
	if (pEP->aborting)
	{
		USBLog(1, "AppleUSBUHCI[%p]::AbortIsochEP[%p] - re-enterred.. bailing out", this, pEP);
		return kIOReturnSuccess;
	}

    // we need to make sure that the interrupt routine is not processing the periodic list
	_inAbortIsochEP = true;
	pEP->aborting = true;
	
    // DisablePeriodicSchedule();
	timeStamp = mach_absolute_time();
	
    // now make sure we finish any periodic processing we were already doing (for MP machines)
    while (_filterInterruptActive)
		;


	// now scavange any transactions which were already on the done queue
    err = scavengeIsochTransactions();
	
	
    if (err)
    {
		USBLog(1, "AppleUSBUHCI[%p]::AbortIsochEP - err (0x%x) from scavengeIsochTransactions", this, err);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAbortIsochEP, (uintptr_t)this, err, 0, 1);
    }
    
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(6, "+AppleUSBUHCI[%p]::AbortIsochEP[%p] - after scavenge - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP, (uint32_t)_outSlot, (uint32_t)pEP->inSlot,(uint32_t) pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)_consumerCount, (uint32_t)_producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
	}
	
    if ((_outSlot < kUHCI_NVFRAMES) && (pEP->inSlot < kUHCI_NVFRAMES))
    {
		bool								stopAdvancing = false;
		IOUSBControllerIsochListElement		*activeTD = NULL;
		
        // now scavenge the remaining transactions on the periodic list
        slot = _outSlot;
        while (slot != pEP->inSlot)
        {
            IOUSBControllerListElement 		*thing;
            IOUSBControllerListElement		*nextThing;
            IOUSBControllerListElement		*prevThing;
			UInt32							nextSlot;
            
			nextSlot = (slot+1) & kUHCI_NVFRAMES_MASK;
            thing = _logicalFrameList[slot];
            prevThing = NULL;
			if (thing == NULL && (nextSlot != pEP->inSlot))
				_outSlot = nextSlot;
            while (thing != NULL)
            {
                nextThing = thing->_logicalNext;
                pTD = OSDynamicCast(IOUSBControllerIsochListElement, thing);
                if (pTD)
                {
                    if (pTD->_pEndpoint == pEP)
                    {
						UInt16		frSlot = (ReadFrameNumber() & kUHCI_NVFRAMES_MASK);
						
                        // first unlink it
                        if (prevThing)
                        {
                            prevThing->_logicalNext = thing->_logicalNext;
                            prevThing->SetPhysicalLink(thing->GetPhysicalLink());
							thing = prevThing;															// to cause prevThing to remain unchanged at the bottom of the loop
                        }
                        else
                        {
                            _logicalFrameList[slot] = nextThing;
                            _frameList[slot] = HostToUSBLong(thing->GetPhysicalLink());
							thing = NULL;																// to cause prevThing to remain unchanged (NULL) at the bottom of the loop
							if (nextThing == NULL)
							{
								if (!stopAdvancing)
								{
									USBLog(5, "AppleUSBUHCI[%p]::AbortIsochEP(%p) - advancing _outslot from 0x%x to 0x%x", this, pEP, _outSlot, (uint32_t)nextSlot);
									_outSlot = nextSlot;
								}
								else
								{
									USBLog(5, "AppleUSBUHCI[%p]::AbortIsochEP(%p) - would have advanced _outslot from 0x%x to 0x%x", this, pEP, _outSlot, (uint32_t)nextSlot);
								}
							}
                        }
						// before we make a change to the recently unlinked thing (pTD)
						// we need to make sure that the controller is not actually looking at it
						
						if (frSlot == slot)
						{
							// be very very careful here - we don't want to confuse the controller
							// IODelay(1000);																// need to delay (NOT SLEEP) for 1 ms to make sure that we go to the next frame
							// we used to delay as above. the problem with that is that it caused a 1ms delay for every list element we found
							// on consecutive frames so now, we just need to remember that we have one element whose software link and hardware link
							// may be inconsistent, and we need to delay to let the processer get it out of its cache
							if (activeTD)
							{
								// this is not the first one we have seen this trip. update the last one we saw and save this one
								// we can do this because there is only on TD per EP on the list, so the old one is no longer being examined by the controller
								activeTD->UpdateFrameList(*(AbsoluteTime *)&timeStamp);
							}
							// remember it so we can update the frame list when we are done
							activeTD = pTD;
						}
						else
						{
							err = pTD->UpdateFrameList(*(AbsoluteTime *)&timeStamp);
						}
						OSDecrementAtomic( &(pEP->scheduledTDs));
                        PutTDonDoneQueue(pEP, pTD, true	);
                    }
					else if (pTD->_pEndpoint == NULL)
					{
						USBLog(1, "AppleUSBUHCI[%p]::AbortIsochEP (%p) - NULL endpoint in pTD %p", this, pEP, pTD);
						USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAbortIsochEP, (uintptr_t)this, (uintptr_t)pEP, (uintptr_t)pTD, 2);
					}
					else
					{
						stopAdvancing = true;
						USBLog(7, "AppleUSBUHCI[%p]::AbortIsochEP (%p) - a different EP in play (%p) - stop advancing", this, pEP, pTD->_pEndpoint);
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
		if (activeTD)
		{
			// delay 1ms to make sure the controller is no longer tounching it before we update the frame list
			// note that it is already on the done queue
			IODelay(1000);
			activeTD->UpdateFrameList(*(AbsoluteTime *)&timeStamp);
		}
    }
    
    // now transfer any transactions from the todo list to the done queue
    pTD = GetTDfromToDoList(pEP);
    while (pTD)
    {
		err = pTD->UpdateFrameList(*(AbsoluteTime *)&timeStamp);
		PutTDonDoneQueue(pEP, pTD, true);
		pTD = GetTDfromToDoList(pEP);
    }
	
	if (pEP->scheduledTDs == 0)
	{
		// since we have no Isoch xactions on the endpoint, we can reset the counter
		pEP->firstAvailableFrame = 0;
		pEP->inSlot = kUHCI_NVFRAMES + 1;    
	}
    // we can go back to processing now
	_inAbortIsochEP = false;
    // EnablePeriodicSchedule();
    
    pEP->accumulatedStatus = kIOReturnAborted;
    ReturnIsochDoneQueue(pEP);
    pEP->accumulatedStatus = kIOReturnSuccess;
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(1, "+AppleUSBUHCI[%p]::AbortIsochEP[%p] - done - _outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP, (uint32_t)_outSlot, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)_consumerCount, (uint32_t)_producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAbortIsochEP, (uintptr_t)this, (uintptr_t)pEP, (uint32_t)_outSlot, 3 );
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAbortIsochEP, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, 4 );
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAbortIsochEP, (uintptr_t)pEP->toDoList, (uint32_t)pEP->deferredTDs, (uintptr_t)pEP->deferredQueue, 5 );
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAbortIsochEP, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)_consumerCount, 6 );
		USBTrace( kUSBTUHCIUIM,  kTPUHCIUIMAbortIsochEP, (uint32_t)_producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, (uintptr_t)pEP->doneQueue );
	}
	else
	{
		USBLog(6, "-AppleUSBUHCI[%p]::AbortIsochEP[%p] - done - all clean - _outSlot (0x%x), pEP->inSlot (0x%x)", this, pEP, _outSlot, pEP->inSlot);
	}
	pEP->aborting = false;
    USBLog(7, "-AppleUSBUHCI[%p]::AbortIsochEP (%p)", this, pEP);
    return kIOReturnSuccess;
}



// ========================================================================
#pragma mark Disabled Endpoints
// ========================================================================
IOReturn
AppleUSBUHCI::UIMEnableAddressEndpoints(USBDeviceAddress address, bool enable)
{
	UInt32							slot;
	IOReturn						err;
	
	USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints(%d, %s)", this, (int)address, enable ? "true" : "false");
	if (enable)
	{
		AppleUHCIQueueHead					*pQH = _disabledQHList;
		AppleUHCIQueueHead					*pPrevQH = NULL;
		AppleUHCIQueueHead					*pPrevQHLive = NULL;
		
		USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints- looking for QHs for address (%d) in the disabled queue", this, address);
		while (pQH)
		{
			if (pQH->functionNumber == address)
			{
				USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints- found QH[%p] which matches", this, pQH);
				// first adjust the list
				if (pPrevQH)
					pPrevQH->_logicalNext = pQH->_logicalNext;
				else
					_disabledQHList = OSDynamicCast(AppleUHCIQueueHead, pQH->_logicalNext);
				pQH->_logicalNext = NULL;
				// now stick pQH back in the queue
				switch (pQH->type)
				{
					case kUSBControl:
						// Now link the endpoint's queue head into the schedule
						if (pQH->speed == kUSBDeviceSpeedLow) 
						{
							pPrevQHLive = _lsControlQHEnd;
						} else 
						{
							pPrevQHLive = _fsControlQHEnd;
						}
						
						pQH->_logicalNext = pPrevQHLive->_logicalNext;
						pQH->SetPhysicalLink(pPrevQHLive->GetPhysicalLink());
						IOSync();
						
						pPrevQHLive->_logicalNext = pQH;
						pPrevQHLive->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
						IOSync();
						if (pQH->speed == kUSBDeviceSpeedLow) 
						{
							_lsControlQHEnd = pQH;
						} else 
						{
							_fsControlQHEnd = pQH;
						}
						break;
						
					case kUSBBulk:
						pPrevQHLive = _bulkQHEnd;
						pQH->_logicalNext = pPrevQHLive->_logicalNext;
						pQH->SetPhysicalLink(pPrevQHLive->GetPhysicalLink());
						IOSync();
						
						pPrevQHLive->_logicalNext = pQH;
						pPrevQHLive->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
						IOSync();
						_bulkQHEnd = pQH;
					break;
						
					case kUSBInterrupt:
						pPrevQHLive = _intrQH[pQH->interruptSlot];
						pQH->_logicalNext = pPrevQHLive->_logicalNext;
						pQH->SetPhysicalLink(pPrevQHLive->GetPhysicalLink());
						IOSync();
						
						pPrevQHLive->_logicalNext = pQH;
						pPrevQHLive->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
					break;
						
					default:
						USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints- found QH[%p] with unknown type(%d)", this, pQH, pQH->type);
						break;
				}
				// advance the pointer
				if (pPrevQH)
					pQH = OSDynamicCast(AppleUHCIQueueHead, pPrevQH->_logicalNext);
				else
					pQH = _disabledQHList;
			}
			else
			{
				// advance the pointer when we didn't find what we were looking for
				pPrevQH = pQH;
				pQH = OSDynamicCast(AppleUHCIQueueHead, pQH->_logicalNext);
			}
		}
		return kIOReturnSuccess;
	}
	
	// the disable case
	USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints- looking for endpoints for device address(%d) to disable", this, address);
	
	for (slot=0; slot < kUHCI_NVFRAMES; slot++)
	{
		IOUSBControllerListElement				*pLE;
		IOUSBControllerListElement				*pPrevLE = NULL;

		pLE = _logicalFrameList[slot];
		while (pLE)
		{
			// need to determine which kind of list element this is
			AppleUHCIIsochTransferDescriptor	*pITD = OSDynamicCast(AppleUHCIIsochTransferDescriptor, pLE);
			AppleUHCIQueueHead					*pQH =	OSDynamicCast(AppleUHCIQueueHead, pLE);
			
			if (pQH && (pQH->type != kQHTypeDummy) && (pQH->functionNumber == address))
			{
				AppleUHCIQueueHead				*pPrevQH =	OSDynamicCast(AppleUHCIQueueHead, pPrevLE);
				USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints- found pQH[%p] which matches (pPrevQH[%p])", this, pQH, pPrevQH);
				if (!pPrevQH)
				{
					USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints- unexpected NULL pPrevQH", this);
				}
				else
				{
					err = UnlinkQueueHead(pQH, pPrevQH);
					if (err)
					{
						USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints- err[%p] unlinking queue head", this, (void*)err);
					}
					else
					{
						// well now that it is off the queue, what do i do with it?
						pLE = pPrevLE;							// reset for the counter to the previous QH
						
						// now link the pQH into the disabled list
						pQH->_logicalNext = _disabledQHList;
						_disabledQHList = pQH;
					}
				}
			}
			
			// I need to be aware of where the current frame is when processing the Isoch List
			if (pITD && (UHCI_TD_GET_ADDR(USBToHostLong(pITD->GetSharedLogical()->token))))
			{
				USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAddressEndpoints- found pITD[%p] which matches", this, pITD);
			}
			pPrevLE = pLE;
			pLE = pLE->_logicalNext;
		}
	}
	return kIOReturnSuccess;
}



IOReturn
AppleUSBUHCI::UIMEnableAllEndpoints(bool enable)
{
	UInt32							slot;
	IOReturn						err;
	
	USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints(%s)", this, enable ? "true" : "false");
	if (enable)
	{
		AppleUHCIQueueHead					*pQH = _disabledQHList;
		AppleUHCIQueueHead					*pPrevQH = NULL;
		AppleUHCIQueueHead					*pPrevQHLive = NULL;
		
		USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints- looking for QHs in the disabled queue", this);
		while (pQH)
		{
			USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints- found QH[%p] which matches", this, pQH);
			// first adjust the list
			_disabledQHList = OSDynamicCast(AppleUHCIQueueHead, pQH->_logicalNext);
			pQH->_logicalNext = NULL;
			// now stick pQH back in the queue
			switch (pQH->type)
			{
				case kUSBControl:
					// Now link the endpoint's queue head into the schedule
					if (pQH->speed == kUSBDeviceSpeedLow) 
					{
						pPrevQHLive = _lsControlQHEnd;
					} else 
					{
						pPrevQHLive = _fsControlQHEnd;
					}
					
					pQH->_logicalNext = pPrevQHLive->_logicalNext;
					pQH->SetPhysicalLink(pPrevQHLive->GetPhysicalLink());
					IOSync();
					
					pPrevQHLive->_logicalNext = pQH;
					pPrevQHLive->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
					IOSync();
					if (pQH->speed == kUSBDeviceSpeedLow) 
					{
						_lsControlQHEnd = pQH;
					} else 
					{
						_fsControlQHEnd = pQH;
					}
					break;
					
				case kUSBBulk:
					pPrevQHLive = _bulkQHEnd;
					pQH->_logicalNext = pPrevQHLive->_logicalNext;
					pQH->SetPhysicalLink(pPrevQHLive->GetPhysicalLink());
					IOSync();
					
					pPrevQHLive->_logicalNext = pQH;
					pPrevQHLive->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
					IOSync();
					_bulkQHEnd = pQH;
				break;
					
				case kUSBInterrupt:
					pPrevQHLive = _intrQH[pQH->interruptSlot];
					pQH->_logicalNext = pPrevQHLive->_logicalNext;
					pQH->SetPhysicalLink(pPrevQHLive->GetPhysicalLink());
					IOSync();
					
					pPrevQHLive->_logicalNext = pQH;
					pPrevQHLive->SetPhysicalLink(pQH->GetPhysicalAddrWithType());
				break;
					
				default:
					USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints- found QH[%p] with unknown type(%d)", this, pQH, pQH->type);
					break;
			}
			// advance the pointer
			pQH = _disabledQHList;
		}
		return kIOReturnSuccess;
	}
	
	// the disable case
	USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints- looking for endpoints for to disable", this);
	
	for (slot=0; slot < kUHCI_NVFRAMES; slot++)
	{
		IOUSBControllerListElement				*pLE;
		IOUSBControllerListElement				*pPrevLE = NULL;

		pLE = _logicalFrameList[slot];
		while (pLE)
		{
			// need to determine which kind of list element this is
			AppleUHCIIsochTransferDescriptor	*pITD = OSDynamicCast(AppleUHCIIsochTransferDescriptor, pLE);
			AppleUHCIQueueHead					*pQH =	OSDynamicCast(AppleUHCIQueueHead, pLE);
			
			if (pQH && (pQH->type != kQHTypeDummy))
			{
				AppleUHCIQueueHead				*pPrevQH =	OSDynamicCast(AppleUHCIQueueHead, pPrevLE);
				USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints- found pQH[%p] which matches (pPrevQH[%p])", this, pQH, pPrevQH);
				if (!pPrevQH)
				{
					USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints- unexpected NULL pPrevQH", this);
				}
				else
				{
					err = UnlinkQueueHead(pQH, pPrevQH);
					if (err)
					{
						USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints- err[%p] unlinking queue head", this, (void*)err);
					}
					else
					{
						// well now that it is off the queue, what do i do with it?
						pLE = pPrevLE;							// reset for the counter to the previous QH
						
						// now link the pQH into the disabled list
						pQH->_logicalNext = _disabledQHList;
						_disabledQHList = pQH;
					}
				}
			}
			
			// I need to be aware of where the current frame is when processing the Isoch List
			if (pITD && (UHCI_TD_GET_ADDR(USBToHostLong(pITD->GetSharedLogical()->token))))
			{
				USBLog(2, "AppleUSBUHCI[%p]::UIMEnableAllEndpoints- found pITD[%p] which matches", this, pITD);
			}
			pPrevLE = pLE;
			pLE = pLE->_logicalNext;
		}
	}
	return kIOReturnSuccess;
}
