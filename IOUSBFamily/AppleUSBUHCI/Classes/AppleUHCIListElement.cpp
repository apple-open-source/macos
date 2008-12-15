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


#include <IOKit/usb/IOUSBLog.h>

#include "AppleUHCIListElement.h"

#undef super
#define super IOUSBControllerListElement
// -----------------------------------------------------------------
//		AppleUHCIQueueHead
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleUHCIQueueHead, IOUSBControllerListElement);

AppleUHCIQueueHead *
AppleUHCIQueueHead::WithSharedMemory(UHCIQueueHeadSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical)
{
    AppleUHCIQueueHead *me = new AppleUHCIQueueHead;
    if (!me || !me->init())
		return NULL;
    me->_sharedLogical = sharedLogical;
    me->_sharedPhysical = sharedPhysical;
    return me;
}


UHCIQueueHeadSharedPtr		
AppleUHCIQueueHead::GetSharedLogical(void)
{
    return (UHCIQueueHeadSharedPtr)_sharedLogical;
}


void 
AppleUHCIQueueHead::SetPhysicalLink(IOPhysicalAddress next)
{
    GetSharedLogical()->hlink = HostToUSBLong(next);
	IOSync();
}


IOPhysicalAddress
AppleUHCIQueueHead::GetPhysicalLink(void)
{
    return USBToHostLong(GetSharedLogical()->hlink);
}


IOPhysicalAddress 
AppleUHCIQueueHead::GetPhysicalAddrWithType(void)
{
    return _sharedPhysical | kUHCI_QH_Q;
}

void
AppleUHCIQueueHead::print(int level)
{
    UHCIQueueHeadSharedPtr shared = GetSharedLogical();
	
    super::print(level);
    USBLog(level, "AppleUHCIQueueHead::print - shared.hlink[%p]", (void*)USBToHostLong(shared->hlink));
    USBLog(level, "AppleUHCIQueueHead::print - shared.elink[%p]", (void*)USBToHostLong(shared->elink));
    USBLog(level, "AppleUHCIQueueHead::print - functionNumber[%d]", functionNumber);
    USBLog(level, "AppleUHCIQueueHead::print - endpointNumber[%d]", endpointNumber);
    USBLog(level, "AppleUHCIQueueHead::print - speed[%s]", speed == kUSBDeviceSpeedLow ? "low" : "full");
    USBLog(level, "AppleUHCIQueueHead::print - maxPacketSize[%d]", maxPacketSize);
    USBLog(level, "AppleUHCIQueueHead::print - pollingRate[%d]", pollingRate);
    USBLog(level, "AppleUHCIQueueHead::print - direction[%s]", (direction == kUSBIn) ? "IN" : ((direction == kUSBOut) ? "OUT" : "BOTH"));
    USBLog(level, "AppleUHCIQueueHead::print - type[%s]", (type == kUSBControl) ? "Control" : ((type == kUSBIsoc) ? "Isoc" : ((type == kUSBBulk) ? "Bulk" : ((type == kUSBInterrupt) ? "Interrupt" : ((type == kQHTypeDummy) ? "Dummy" : "Unknown")))));
    USBLog(level, "AppleUHCIQueueHead::print - stalled[%s]", stalled ? "true" : "false");
    USBLog(level, "AppleUHCIQueueHead::print - firstTD[%p]", firstTD);
    USBLog(level, "AppleUHCIQueueHead::print - lastTD[%p]", lastTD);
    USBLog(level, "---------------------------------------------");
}



// -----------------------------------------------------------------
//		AppleUHCITransferDescriptor
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleUHCITransferDescriptor, IOUSBControllerListElement);

AppleUHCITransferDescriptor *
AppleUHCITransferDescriptor::WithSharedMemory(UHCITransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical)
{
    AppleUHCITransferDescriptor *me = new AppleUHCITransferDescriptor;
    if (!me || !me->init())
		return NULL;
    me->_sharedLogical = sharedLogical;
    me->_sharedPhysical = sharedPhysical;
    return me;
}



UHCITransferDescriptorSharedPtr		
AppleUHCITransferDescriptor::GetSharedLogical(void)
{
    return (UHCITransferDescriptorSharedPtr)_sharedLogical;
}



void 
AppleUHCITransferDescriptor::SetPhysicalLink(IOPhysicalAddress next)
{
    GetSharedLogical()->link = HostToUSBLong(next);
	IOSync();
}



IOPhysicalAddress
AppleUHCITransferDescriptor::GetPhysicalLink(void)
{
    return USBToHostLong(GetSharedLogical()->link);
}



IOPhysicalAddress 
AppleUHCITransferDescriptor::GetPhysicalAddrWithType(void)
{
    return _sharedPhysical;
}



void
AppleUHCITransferDescriptor::print(int level)
{
    UHCITransferDescriptorSharedPtr		shared = GetSharedLogical();
	UInt32								value;
	const char								*token_type;
	
    super::print(level);
	value = USBToHostLong(shared->link);
	USBLog(level, "AppleUHCITransferDescriptor::print HW: link     %p %s%s%s",
		   (void*)(value & 0xFFFFFFF0),
		   (value & 0x4) ? "Vf " : "",
		   (value & 0x2) ? "Q " : "",
		   (value & 0x1) ? "T" : "");
	value = USBToHostLong(shared->ctrlStatus);
	USBLog(level, "AppleUHCITransferDescriptor::print   ctrlStatus %p ActLen %x Status %x Err %x %s%s%s%s", (void*)value,
		   (unsigned int)UHCI_TD_GET_ACTLEN(value),
		   (unsigned int)UHCI_TD_GET_STATUS(value),
		   (unsigned int)UHCI_TD_GET_ERRCNT(value),
		   (value & kUHCI_TD_IOC) ? "IOC " : "",
		   (value & kUHCI_TD_ISO) ? "ISO " : "",
		   (value & kUHCI_TD_LS) ? "LS " : "",
		   (value & kUHCI_TD_SPD) ? "SPD " : "");
	USBLog(level, "\t\t\t\t\t\t\tSTATUS DECODE: %s%s%s%s%s%s%s",
		   (value & kUHCI_TD_ACTIVE) ? "ACTIVE " : "",
		   (value & kUHCI_TD_STALLED) ? "STALL " : "",
		   (value & kUHCI_TD_DBUF) ? "DBUF " : "",
		   (value & kUHCI_TD_BABBLE) ? "BABBLE " : "",
		   (value & kUHCI_TD_NAK) ? "NAK " : "",
		   (value & kUHCI_TD_CRCTO) ? "CRCTO " : "",
		   (value & kUHCI_TD_BITSTUFF) ? "BITSTUFF" : "");
	value = USBToHostLong(shared->token);
	switch (value & kUHCI_TD_PID) 
	{
		case kUHCI_TD_PID_SETUP:
			token_type = "(SETUP)";
			break;
		case kUHCI_TD_PID_IN:
			token_type = "(IN)";
			break;
		case kUHCI_TD_PID_OUT:
			token_type = "(OUT)";
			break;
		default:
			token_type = "(UNKNOWN)";
			break;
	}
	USBLog(level, "AppleUHCITransferDescriptor::print        token %p %s DevAddr %x EndPt %x MaxLen %x %s", (void*)value,
		   token_type,
		   (unsigned int)UHCI_TD_GET_ADDR(value),
		   (unsigned int)UHCI_TD_GET_ENDPT(value),
		   (unsigned int)UHCI_TD_GET_MAXLEN(value),
		   (value & kUHCI_TD_D) ? "D" : "");
	USBLog(level, "AppleUHCITransferDescriptor::print       shared.buffer %08x", USBToHostLong(shared->buffer));
	USBLog(level, "AppleUHCITransferDescriptor::print - alignment buffer[%p]", alignBuffer);
	USBLog(level, "AppleUHCITransferDescriptor::print - command[%p]", command);
	USBLog(level, "AppleUHCITransferDescriptor::print - memory descriptor[%p]", logicalBuffer);
	USBLog(level, "AppleUHCITransferDescriptor::print - callbackOnTD[%s]", callbackOnTD ? "true" : "false");
	USBLog(level, "AppleUHCITransferDescriptor::print - multiXferTransaction[%s]", multiXferTransaction ? "true" : "false");
	USBLog(level, "AppleUHCITransferDescriptor::print - finalXferInTransaction[%s]", finalXferInTransaction ? "true" : "false");
	USBLog(level, "AppleUHCITransferDescriptor::print -----------------------------------");
}



// -----------------------------------------------------------------
//		AppleUHCITransferDescriptor
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleUHCIIsochTransferDescriptor, IOUSBControllerIsochListElement);

AppleUHCIIsochTransferDescriptor *
AppleUHCIIsochTransferDescriptor::WithSharedMemory(UHCITransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical)
{
    AppleUHCIIsochTransferDescriptor *me = new AppleUHCIIsochTransferDescriptor;
    if (!me || !me->init())
		return NULL;
	
    me->_sharedLogical = sharedLogical;
    me->_sharedPhysical = sharedPhysical;
    return me;
}



UHCITransferDescriptorSharedPtr		
AppleUHCIIsochTransferDescriptor::GetSharedLogical(void)
{
    return (UHCITransferDescriptorSharedPtr)_sharedLogical;
}



void 
AppleUHCIIsochTransferDescriptor::SetPhysicalLink(IOPhysicalAddress next)
{
    GetSharedLogical()->link = HostToUSBLong(next);
	IOSync();
}



IOPhysicalAddress
AppleUHCIIsochTransferDescriptor::GetPhysicalLink(void)
{
    return USBToHostLong(GetSharedLogical()->link);
}



IOPhysicalAddress 
AppleUHCIIsochTransferDescriptor::GetPhysicalAddrWithType(void)
{
    return _sharedPhysical;
}



IOReturn
AppleUHCIIsochTransferDescriptor::Deallocate(IOUSBControllerV2 *uim)
{
    return ((AppleUSBUHCI*)uim)->DeallocateITD(this);
}



IOReturn
AppleUHCIIsochTransferDescriptor::UpdateFrameList(AbsoluteTime timeStamp)
{
    UInt32						statFlags;
    IOUSBIsocFrame 				*pFrames;    
    IOUSBLowLatencyIsocFrame 	*pLLFrames;    
    IOReturn					frStatus = kIOReturnSuccess;
    UInt16						frActualCount = 0;
    UInt16						frReqCount;
    
    statFlags = USBToHostLong(GetSharedLogical()->ctrlStatus);
	frActualCount = UHCI_TD_GET_ACTLEN(statFlags);
    // warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
    // USBLog(7, "AppleUHCIIsochTransferDescriptor[%p]::UpdateFrameList statFlags (%x)", this, statFlags);
    pFrames = _pFrames;
	if (!pFrames)							// this will be the case for the dummy TD
		return kIOReturnSuccess;
	
    pLLFrames = (IOUSBLowLatencyIsocFrame*)_pFrames;
    if (_lowLatency)
    {
		frReqCount = pLLFrames[_frameIndex].frReqCount;
    }
    else
    {
		frReqCount = pFrames[_frameIndex].frReqCount;
    }
	
    if (statFlags & kUHCI_TD_ACTIVE)
    {
		frStatus = kIOUSBNotSent2Err;
    }
    else if (statFlags & kUHCI_TD_CRCTO)
    {
		frStatus = kIOReturnNotResponding;
    }
    else if (statFlags & kUHCI_TD_DBUF)									// data buffer (PCI error)
    {
		if (_pEndpoint->direction == kUSBOut)
			frStatus = kIOUSBBufferUnderrunErr;
		else
			frStatus = kIOUSBBufferOverrunErr;
    }
    else if (statFlags & kUHCI_TD_BABBLE)
    {
		if (_pEndpoint->direction == kUSBOut)
			frStatus = kIOReturnNotResponding;							// babble on OUT. this should never happen
		else
			frStatus = kIOReturnOverrun;
    }
    else if (statFlags & kUHCI_TD_STALLED)								// if STALL happens on Isoch, it is most likely covered by one of the other bits above
    {
		frStatus = kIOUSBWrongPIDErr;
    }
    else
    {
		if (frActualCount != frReqCount)
		{
			if (_pEndpoint->direction == kUSBOut)
			{
				// warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
				// USBLog(7, "AppleUHCIIsochTransferDescriptor[%p]::UpdateFrameList - (OUT) reqCount (%d) actCount (%d)", this, frReqCount, frActualCount);
				frStatus = kIOUSBBufferUnderrunErr;						// this better have generated a DBUF or other error
			}
			else if (_pEndpoint->direction == kUSBIn)
			{
				// warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
				// USBLog(7, "AppleUHCIIsochTransferDescriptor[%p]::UpdateFrameList - (IN) reqCount (%d) actCount (%d)", this, frReqCount, frActualCount);
				frStatus = kIOReturnUnderrun;							// benign error
			}
		}
    }

    if (alignBuffer && alignBuffer->userBuffer && alignBuffer->vaddr && (_pEndpoint->direction == kUSBIn))
	{
		// i can't log in here because this is called at interrupt time
		// i know that this is OK for Low Latency because the buffer will be allocated in low memory and wont' be bounced
		alignBuffer->userBuffer->writeBytes(alignBuffer->userOffset, (void*)alignBuffer->vaddr, frActualCount);
		alignBuffer->actCount = frActualCount;
	}
	
	
	if (_lowLatency)
    {
		if ( _requestFromRosettaClient )
		{
			pLLFrames[_frameIndex].frActCount = OSSwapInt16(frActualCount);
			pLLFrames[_frameIndex].frReqCount = OSSwapInt16(pLLFrames[_frameIndex].frReqCount);
			AbsoluteTime_to_scalar(&pLLFrames[_frameIndex].frTimeStamp) 
			= OSSwapInt64(AbsoluteTime_to_scalar(&timeStamp));
			pLLFrames[_frameIndex].frStatus = OSSwapInt32(frStatus);
		}
		else
		{
			pLLFrames[_frameIndex].frActCount = frActualCount;
			pLLFrames[_frameIndex].frTimeStamp = timeStamp;
			pLLFrames[_frameIndex].frStatus = frStatus;
		}
    }
    else
    {
		if ( _requestFromRosettaClient )
		{
			pFrames[_frameIndex].frActCount = OSSwapInt16(frActualCount);
			pFrames[_frameIndex].frReqCount = OSSwapInt16(pFrames[_frameIndex].frReqCount);
			pFrames[_frameIndex].frStatus = OSSwapInt32(frStatus);
		}
		else
		{
			pFrames[_frameIndex].frActCount = frActualCount;
			pFrames[_frameIndex].frStatus = frStatus;
		}
    }
	
    if(frStatus != kIOReturnSuccess)
    {
		if(frStatus != kIOReturnUnderrun)
		{
			_pEndpoint->accumulatedStatus = frStatus;
		}
		else if(_pEndpoint->accumulatedStatus == kIOReturnSuccess)
		{
			_pEndpoint->accumulatedStatus = kIOReturnUnderrun;
		}
    }
    return frStatus;
}



void
AppleUHCIIsochTransferDescriptor::print(int level)
{
    UHCITransferDescriptorSharedPtr		shared = GetSharedLogical();
	UInt32								value;
	const char								*token_type;
	
    super::print(level);
	value = USBToHostLong(shared->link);
	USBLog(level, "AppleUHCIIsochTransferDescriptor::print HW: link     %p %s%s%s",
		   (void*)(value & 0xFFFFFFF0),
		   (value & 0x4) ? "Vf " : "",
		   (value & 0x2) ? "Q " : "",
		   (value & 0x1) ? "T" : "");
	value = USBToHostLong(shared->ctrlStatus);
	USBLog(level, "AppleUHCIIsochTransferDescriptor::print   ctrlStatus %p ActLen %x Status %x Err %x %s%s%s%s", (void*)value,
		   (unsigned int)UHCI_TD_GET_ACTLEN(value),
		   (unsigned int)UHCI_TD_GET_STATUS(value),
		   (unsigned int)UHCI_TD_GET_ERRCNT(value),
		   (value & kUHCI_TD_IOC) ? "IOC " : "",
		   (value & kUHCI_TD_ISO) ? "ISO " : "",
		   (value & kUHCI_TD_LS) ? "LS " : "",
		   (value & kUHCI_TD_SPD) ? "SPD " : "");
	USBLog(level, "\t\t\t\t\t\t\tSTATUS DECODE: %s%s%s%s%s%s%s",
		   (value & kUHCI_TD_ACTIVE) ? "ACTIVE " : "",
		   (value & kUHCI_TD_STALLED) ? "STALL " : "",
		   (value & kUHCI_TD_DBUF) ? "DBUF " : "",
		   (value & kUHCI_TD_BABBLE) ? "BABBLE " : "",
		   (value & kUHCI_TD_NAK) ? "NAK " : "",
		   (value & kUHCI_TD_CRCTO) ? "CRCTO " : "",
		   (value & kUHCI_TD_BITSTUFF) ? "BITSTUFF" : "");
	value = USBToHostLong(shared->token);
	switch (value & kUHCI_TD_PID) 
	{
		case kUHCI_TD_PID_SETUP:
			token_type = "(SETUP)";
			break;
		case kUHCI_TD_PID_IN:
			token_type = "(IN)";
			break;
		case kUHCI_TD_PID_OUT:
			token_type = "(OUT)";
			break;
		default:
			token_type = "(UNKNOWN)";
			break;
	}
	USBLog(level, "AppleUHCIIsochTransferDescriptor::print        token %p %s DevAddr %x EndPt %x MaxLen %x %s", (void*)value,
		   token_type,
		   (unsigned int)UHCI_TD_GET_ADDR(value),
		   (unsigned int)UHCI_TD_GET_ENDPT(value),
		   (unsigned int)UHCI_TD_GET_MAXLEN(value),
		   (value & kUHCI_TD_D) ? "D" : "");
	USBLog(level, "AppleUHCIIsochTransferDescriptor::print       shared.buffer %08x", USBToHostLong(shared->buffer));
	USBLog(level, "AppleUHCIIsochTransferDescriptor::print - alignment buffer[%p]", alignBuffer);
	USBLog(level, "AppleUHCIIsochTransferDescriptor::print -----------------------------------");
}
