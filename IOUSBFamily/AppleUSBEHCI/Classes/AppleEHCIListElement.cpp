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

#include <IOKit/usb/IOUSBLog.h>

#include "AppleEHCIListElement.h"

#define super OSObject
// -----------------------------------------------------------------
//		AppleEHCIListElement
// -----------------------------------------------------------------
OSDefineMetaClass( AppleEHCIListElement, OSObject )
OSDefineAbstractStructors(AppleEHCIListElement, OSObject)

void
AppleEHCIListElement::print(int level)
{
    USBLog(level, "AppleEHCIListElement::print - _sharedPhysical[%x]", _sharedPhysical);
    USBLog(level, "AppleEHCIListElement::print - _sharedLogical[%x]", _sharedLogical);
    USBLog(level, "AppleEHCIListElement::print - _logicalNext[%x]", _logicalNext);
}


#undef super
#define super AppleEHCIListElement
// -----------------------------------------------------------------
//		AppleEHCIQueueHead
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleEHCIQueueHead, AppleEHCIListElement);

AppleEHCIQueueHead *
AppleEHCIQueueHead::WithSharedMemory(EHCIQueueHeadSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical)
{
    AppleEHCIQueueHead *me = new AppleEHCIQueueHead;
    if (!me || !me->init())
	return NULL;
    me->_sharedLogical = sharedLogical;
    me->_sharedPhysical = sharedPhysical;
    return me;
}


EHCIQueueHeadSharedPtr		
AppleEHCIQueueHead::GetSharedLogical(void)
{
    return (EHCIQueueHeadSharedPtr)_sharedLogical;
}


void 
AppleEHCIQueueHead::SetPhysicalLink(IOPhysicalAddress next)
{
    GetSharedLogical()->nextQH = next;
}


IOPhysicalAddress
AppleEHCIQueueHead::GetPhysicalLink(void)
{
    return GetSharedLogical()->nextQH;
}


IOPhysicalAddress 
AppleEHCIQueueHead::GetPhysicalAddrWithType(void)
{
    return _sharedPhysical | (kEHCITyp_QH << kEHCIEDNextED_TypPhase);
}

void
AppleEHCIQueueHead::print(int level)
{
    EHCIQueueHeadSharedPtr shared = GetSharedLogical();

    super::print(level);
    USBLog(level, "AppleEHCIQueueHead::print - shared.nextQH[%p]", USBToHostLong(shared->nextQH));
    USBLog(level, "AppleEHCIQueueHead::print - shared.flags[%p]", USBToHostLong(shared->flags));
    USBLog(level, "AppleEHCIQueueHead::print - shared.splitFlags[%p]", USBToHostLong(shared->splitFlags));
    USBLog(level, "AppleEHCIQueueHead::print - shared.CurrqTDPtr[%p]", USBToHostLong(shared->CurrqTDPtr));
    USBLog(level, "AppleEHCIQueueHead::print - shared.NextqTDPtr[%p]", USBToHostLong(shared->NextqTDPtr));
    USBLog(level, "AppleEHCIQueueHead::print - shared.AltqTDPtr[%p]", USBToHostLong(shared->AltqTDPtr));
    USBLog(level, "AppleEHCIQueueHead::print - shared.qTDFlags[%p]", USBToHostLong(shared->qTDFlags));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[0][%p]", USBToHostLong(shared->BuffPtr[0]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[1][%p]", USBToHostLong(shared->BuffPtr[1]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[2][%p]", USBToHostLong(shared->BuffPtr[2]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[3][%p]", USBToHostLong(shared->BuffPtr[3]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[4][%p]", USBToHostLong(shared->BuffPtr[4]));
}


// -----------------------------------------------------------------
//		AppleEHCIIsochListElement
// -----------------------------------------------------------------
OSDefineMetaClass( AppleEHCIIsochListElement, AppleEHCIListElement )
OSDefineAbstractStructors(AppleEHCIIsochListElement, AppleEHCIListElement)

void
AppleEHCIIsochListElement::print(int level)
{
    super::print(level);
    USBLog(level, "AppleEHCIIsochListElement::print - myEndpoint[%x]", myEndpoint);
    USBLog(level, "AppleEHCIIsochListElement::print - myFrames[%x]", myFrames);
    USBLog(level, "AppleEHCIIsochListElement::print - completion[%x, %x, %x]", completion.action, completion.target, completion.parameter);
    USBLog(level, "AppleEHCIIsochListElement::print - lowLatency[%x]", lowLatency);
    USBLog(level, "AppleEHCIIsochListElement::print - frameNumber[%x]", (UInt32)frameNumber);
}


#undef super
#define super AppleEHCIIsochListElement
// -----------------------------------------------------------------
//		AppleEHCIIsochTransferDescriptor
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleEHCIIsochTransferDescriptor, AppleEHCIIsochListElement);

AppleEHCIIsochTransferDescriptor *
AppleEHCIIsochTransferDescriptor::WithSharedMemory(EHCIIsochTransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical)
{
    AppleEHCIIsochTransferDescriptor *me = new AppleEHCIIsochTransferDescriptor;
    if (!me || !me->init())
	return NULL;
    me->_sharedLogical = sharedLogical;
    me->_sharedPhysical = sharedPhysical;
    return me;
}


EHCIIsochTransferDescriptorSharedPtr		
AppleEHCIIsochTransferDescriptor::GetSharedLogical(void)
{
    return (EHCIIsochTransferDescriptorSharedPtr)_sharedLogical;
}


void 
AppleEHCIIsochTransferDescriptor::SetPhysicalLink(IOPhysicalAddress next)
{
    GetSharedLogical()->nextiTD = next;
}


IOPhysicalAddress
AppleEHCIIsochTransferDescriptor::GetPhysicalLink(void)
{
    return GetSharedLogical()->nextiTD;
}


IOPhysicalAddress 
AppleEHCIIsochTransferDescriptor::GetPhysicalAddrWithType(void)
{
    return _sharedPhysical | (kEHCITyp_iTD << kEHCIEDNextED_TypPhase);
}


IOReturn 
AppleEHCIIsochTransferDescriptor::mungeEHCIStatus(UInt32 status, UInt16 *transferLen, UInt32 maxPacketSize, UInt8 direction)
{
/*  This is how I'm unmangling the EHCI error status 

iTD has these possible status bits:

31 Active.							- If Active, then not accessed.
30 Data Buffer Error.				- Host data buffer under (out) over (in) run error
29 Babble Detected.                 - Recevied data overrun
28 Transaction Error (XactErr).	    - Everything else. Use not responding.

	if(active) kIOUSBNotSent1Err
	else if(DBE) if(out)kIOUSBBufferUnderrunErr else kIOUSBBufferOverrunErr
	else if(babble) kIOReturnOverrun
	else if(Xacterr) kIOReturnNotResponding
	else if(in) if(length < maxpacketsize) kIOReturnUnderrun
	else
		kIOReturnSuccess
*/
	if((status & (kEHCI_ITDStatus_Active | kEHCI_ITDStatus_BuffErr | kEHCI_ITDStatus_Babble | kEHCI_ITDStatus_XactErr)) == 0)
	{
		/* all status bits clear */
		
		*transferLen = (status & kEHCI_ITDTr_Len) >> kEHCI_ITDTr_LenPhase;
		if( (direction == kUSBIn) && (maxPacketSize != *transferLen) )
		{
			return(kIOReturnUnderrun);
		}
		return(kIOReturnSuccess);
	}
	*transferLen = 0;
	
	if( (status & kEHCI_ITDStatus_Active) != 0)
	{
		return(kIOUSBNotSent1Err);
	}
	else if( (status & kEHCI_ITDStatus_BuffErr) != 0)
	{
		if(direction == kUSBOut)
		{
			return(kIOUSBBufferUnderrunErr);
		}
		else
		{
			return(kIOUSBBufferOverrunErr);
		}
	}
	else if( (status & kEHCI_ITDStatus_Babble) != 0)
	{
		return(kIOReturnOverrun);
	}
	else // if( (status & kEHCI_ITDStatus_XactErr) != 0)
	{
		return(kIOReturnNotResponding);
	}
}


IOReturn
AppleEHCIIsochTransferDescriptor::UpdateFrameList(AbsoluteTime timeStamp)
{
    UInt32 				*TransactionP, statusWord;
    IOUSBLowLatencyIsocFrame 		*FrameP;
    IOReturn 				ret, frStatus;
    int 				i;

    ret = myEndpoint->accumulatedStatus;
	
    TransactionP = &GetSharedLogical()->Transaction0;
    FrameP = (IOUSBLowLatencyIsocFrame *)myFrames;
    for(i=0; i<8; i++)
    {
	    statusWord = USBToHostLong(*(TransactionP++));
	    frStatus = mungeEHCIStatus(statusWord, &FrameP->frActCount,  myEndpoint->maxPacketSize,  myEndpoint->direction);
	    if (lowLatency)
		FrameP->frTimeStamp = timeStamp;
	    
	    if(frStatus != kIOReturnSuccess)
	    {
		    USBError(1, "AppleEHCIIsochTransferDescriptor::UpdateFrameList - bad status word %p gives status %p", statusWord, frStatus);
		    if(frStatus != kIOReturnUnderrun)
		    {
			    ret = frStatus;
		    }
		    else if(ret == kIOReturnSuccess)
		    {
			    ret = kIOReturnUnderrun;
		    }
	    }
	    FrameP->frStatus = frStatus;
	    if(lowLatency)
	    {
		    FrameP++;
	    }
	    else
	    {
		    FrameP = (IOUSBLowLatencyIsocFrame *)(  ((IOUSBIsocFrame *)FrameP)+1);
	    }
    }
    myEndpoint->accumulatedStatus = ret;
    return ret;
}


IOReturn
AppleEHCIIsochTransferDescriptor::Deallocate(AppleUSBEHCI *uim)
{
    return uim->DeallocateITD(this);
}


void
AppleEHCIIsochTransferDescriptor::print(int level)
{
    EHCIIsochTransferDescriptorSharedPtr shared = GetSharedLogical();
    
    super::print(level);
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.nextiTD[%x]", USBToHostLong(shared->nextiTD));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.Transaction0[%x]", USBToHostLong(shared->Transaction0));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.Transaction1[%x]", USBToHostLong(shared->Transaction1));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.Transaction2[%x]", USBToHostLong(shared->Transaction2));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.Transaction3[%x]", USBToHostLong(shared->Transaction3));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.Transaction4[%x]", USBToHostLong(shared->Transaction4));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.Transaction5[%x]", USBToHostLong(shared->Transaction5));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.Transaction6[%x]", USBToHostLong(shared->Transaction6));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.Transaction7[%x]", USBToHostLong(shared->Transaction7));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.bufferPage0[%x]", USBToHostLong(shared->bufferPage0));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.bufferPage1[%x]", USBToHostLong(shared->bufferPage1));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.bufferPage2[%x]", USBToHostLong(shared->bufferPage2));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.bufferPage3[%x]", USBToHostLong(shared->bufferPage3));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.bufferPage4[%x]", USBToHostLong(shared->bufferPage4));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.bufferPage5[%x]", USBToHostLong(shared->bufferPage5));
    USBLog(level, "AppleEHCIIsochTransferDescriptor::print - shared.bufferPage6[%x]", USBToHostLong(shared->bufferPage6));
}


// -----------------------------------------------------------------
//		AppleEHCISplitIsochTransferDescriptor
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleEHCISplitIsochTransferDescriptor, AppleEHCIIsochListElement);
AppleEHCISplitIsochTransferDescriptor *
AppleEHCISplitIsochTransferDescriptor::WithSharedMemory(EHCISplitIsochTransferDescriptorSharedPtr sharedLogical, IOPhysicalAddress sharedPhysical)
{
    AppleEHCISplitIsochTransferDescriptor *me = new AppleEHCISplitIsochTransferDescriptor;
    if (!me || !me->init())
	return NULL;
    me->_sharedLogical = sharedLogical;
    me->_sharedPhysical = sharedPhysical;
    return me;
}

EHCISplitIsochTransferDescriptorSharedPtr		
AppleEHCISplitIsochTransferDescriptor::GetSharedLogical(void)
{
    return (EHCISplitIsochTransferDescriptorSharedPtr)_sharedLogical;
}


void 
AppleEHCISplitIsochTransferDescriptor::SetPhysicalLink(IOPhysicalAddress next)
{
    GetSharedLogical()->nextSITD = next;
}


IOPhysicalAddress
AppleEHCISplitIsochTransferDescriptor::GetPhysicalLink(void)
{
    return GetSharedLogical()->nextSITD;
}


IOPhysicalAddress 
AppleEHCISplitIsochTransferDescriptor::GetPhysicalAddrWithType(void)
{
    return _sharedPhysical | (kEHCITyp_siTD << kEHCIEDNextED_TypPhase);
}


IOReturn
AppleEHCISplitIsochTransferDescriptor::UpdateFrameList(AbsoluteTime timeStamp)
{
    UInt32					statFlags;
    IOUSBIsocFrame 				*pFrames;    
    IOUSBLowLatencyIsocFrame 			*pLLFrames;    
    IOReturn					frStatus = kIOReturnSuccess;
    UInt16					frActualCount = 0;
    UInt16					frReqCount;
    
    statFlags = USBToHostLong(GetSharedLogical()->statFlags);
    // warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
    // USBLog(7, "AppleEHCISplitIsochTransferDescriptor[%p]::UpdateFrameList statFlags (%x)", this, statFlags);
    pFrames = myFrames;
    pLLFrames = (IOUSBLowLatencyIsocFrame*)pFrames;
    if (lowLatency)
    {
	frReqCount = pLLFrames[frameIndex].frReqCount;
    }
    else
    {
	frReqCount = pFrames[frameIndex].frReqCount;
    }
	
    if (statFlags & kEHCIsiTDStatStatusActive)
    {
	frStatus = kIOUSBNotSent2Err;
    }
    else if (statFlags & kEHCIsiTDStatStatusERR)
    {
	frStatus = kIOReturnNotResponding;
    }
    else if (statFlags & kEHCIsiTDStatStatusDBE)
    {
	if (myEndpoint->direction == kUSBOut)
	    frStatus = kIOReturnUnderrun;
	else
	    frStatus = kIOReturnOverrun;
    }
    else if (statFlags & kEHCIsiTDStatStatusBabble)
    {
	frStatus = kIOReturnNotResponding;
    }
    else if (statFlags & kEHCIsiTDStatStatusXActErr)
    {
	frStatus = kIOUSBWrongPIDErr;
    }
    else if (statFlags & kEHCIsiTDStatStatusMMF)
    {
	frStatus = kIOUSBNotSent1Err;
    }
    else
    {
	frActualCount = frReqCount - ((statFlags & kEHCIsiTDStatLength) >> kEHCIsiTDStatLengthPhase);
	if (frActualCount != frReqCount)
	{
	    if (myEndpoint->direction == kUSBOut)
	    {
		// warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
		// USBLog(7, "AppleEHCISplitIsochTransferDescriptor[%p]::UpdateFrameList - (OUT) reqCount (%d) actCount (%d)", this, frReqCount, frActualCount);
		frStatus = kIOReturnUnderrun;
	    }
	    else if (myEndpoint->direction == kUSBIn)
	    {
		// warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
		// USBLog(7, "AppleEHCISplitIsochTransferDescriptor[%p]::UpdateFrameList - (IN) reqCount (%d) actCount (%d)", this, frReqCount, frActualCount);
		frStatus = kIOReturnUnderrun;
	    }
	}
    }
    if (lowLatency)
    {
	pLLFrames[frameIndex].frActCount = frActualCount;
	pLLFrames[frameIndex].frStatus = frStatus;
	pLLFrames[frameIndex].frTimeStamp = timeStamp;
    }
    else
    {
	pFrames[frameIndex].frActCount = frActualCount;
	pFrames[frameIndex].frStatus = frStatus;
    }
	
    if(frStatus != kIOReturnSuccess)
    {
	if(frStatus != kIOReturnUnderrun)
	{
	    myEndpoint->accumulatedStatus = frStatus;
	}
	else if(myEndpoint->accumulatedStatus == kIOReturnSuccess)
	{
	    myEndpoint->accumulatedStatus = kIOReturnUnderrun;
	}
    }

    return frStatus;
}


IOReturn
AppleEHCISplitIsochTransferDescriptor::Deallocate(AppleUSBEHCI *uim)
{
    return uim->DeallocateSITD(this);
}


void
AppleEHCISplitIsochTransferDescriptor::print(int level)
{
    EHCISplitIsochTransferDescriptorSharedPtr shared = GetSharedLogical();
    
    super::print(level);
    USBLog(level, "AppleEHCISplitIsochTransferDescriptor::print - shared.nextSITD[%x]", USBToHostLong(shared->nextSITD));
    USBLog(level, "AppleEHCISplitIsochTransferDescriptor::print - shared.routeFlags[%x]", USBToHostLong(shared->routeFlags));
    USBLog(level, "AppleEHCISplitIsochTransferDescriptor::print - shared.timeFlags[%x]", USBToHostLong(shared->timeFlags));
    USBLog(level, "AppleEHCISplitIsochTransferDescriptor::print - shared.statFlags[%x]", USBToHostLong(shared->statFlags));
    USBLog(level, "AppleEHCISplitIsochTransferDescriptor::print - shared.buffPtr0[%x]", USBToHostLong(shared->buffPtr0));
    USBLog(level, "AppleEHCISplitIsochTransferDescriptor::print - shared.buffPtr1[%x]", USBToHostLong(shared->buffPtr1));
    USBLog(level, "AppleEHCISplitIsochTransferDescriptor::print - shared.backPtr[%x]", USBToHostLong(shared->backPtr));
}

