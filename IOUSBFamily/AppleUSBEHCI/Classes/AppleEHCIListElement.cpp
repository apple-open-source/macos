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

#include "AppleEHCIListElement.h"
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

#undef super
#define super IOUSBControllerListElement
// -----------------------------------------------------------------
//		AppleEHCIQueueHead
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleEHCIQueueHead, IOUSBControllerListElement);

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
    GetSharedLogical()->nextQH = HostToUSBLong(next);
}


IOPhysicalAddress
AppleEHCIQueueHead::GetPhysicalLink(void)
{
    return USBToHostLong(GetSharedLogical()->nextQH);
}


IOPhysicalAddress 
AppleEHCIQueueHead::GetPhysicalAddrWithType(void)
{
    return _sharedPhysical | (kEHCITyp_QH << kEHCIEDNextED_TypPhase);
}



UInt8
AppleEHCIQueueHead::NormalizedPollingRate(void)
{
	UInt8 retVal;
	
	if (_speed == kUSBDeviceSpeedHigh)
	{
		retVal =  (_pollingRate / kEHCIuFramesPerFrame);				// convert from uFrames to Frames
		if (!retVal)
			retVal = 1;													// this is for when the _pollingRate is 1, 2, or 4 uFrames
	}
	else 
		retVal =  _pollingRate;
	
	return retVal;
}



void
AppleEHCIQueueHead::print(int level)
{
    EHCIQueueHeadSharedPtr shared = GetSharedLogical();
	UInt32					flags = USBToHostLong(shared->flags);
	UInt32					splitFlags = USBToHostLong(shared->splitFlags);
	
    super::print(level);
    USBLog(level, "AppleEHCIQueueHead::print - shared.nextQH[%p]", (void*)USBToHostLong(shared->nextQH));
    USBLog(level, "AppleEHCIQueueHead::print - shared.flags[%p] (ADDR[%d] EP[%d] %s)", (void*)flags, (int)(flags & kEHCIEDFlags_FA), (int)((flags & kEHCIEDFlags_EN) >> kEHCIEDFlags_ENPhase), (flags & kEHCIEDFlags_H) ? "HEAD" : " ");
    USBLog(level, "AppleEHCIQueueHead::print - shared.splitFlags[%p](Hub[%d] Port[%d] C-mask[0x%02x] S-mask[0x%02x])", (void*)splitFlags, (int)(splitFlags  & kEHCIEDSplitFlags_HubAddr) >> kEHCIEDSplitFlags_HubAddrPhase, (int)(splitFlags  & kEHCIEDSplitFlags_Port) >> kEHCIEDSplitFlags_PortPhase, (int)(splitFlags  & kEHCIEDSplitFlags_CMask) >> kEHCIEDSplitFlags_CMaskPhase, (int)(splitFlags  & kEHCIEDSplitFlags_SMask) >> kEHCIEDSplitFlags_SMaskPhase);
    USBLog(level, "AppleEHCIQueueHead::print - shared.CurrqTDPtr[%p]", (void*)USBToHostLong(shared->CurrqTDPtr));
    USBLog(level, "AppleEHCIQueueHead::print - shared.NextqTDPtr[%p]", (void*)USBToHostLong(shared->NextqTDPtr));
    USBLog(level, "AppleEHCIQueueHead::print - shared.AltqTDPtr[%p]", (void*)USBToHostLong(shared->AltqTDPtr));
    USBLog(level, "AppleEHCIQueueHead::print - shared.qTDFlags[%p]", (void*)USBToHostLong(shared->qTDFlags));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[0][%p]", (void*)USBToHostLong(shared->BuffPtr[0]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[1][%p]", (void*)USBToHostLong(shared->BuffPtr[1]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[2][%p]", (void*)USBToHostLong(shared->BuffPtr[2]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[3][%p]", (void*)USBToHostLong(shared->BuffPtr[3]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.BuffPtr[4][%p]", (void*)USBToHostLong(shared->BuffPtr[4]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.extBuffPtr[0][%p]", (void*)USBToHostLong(shared->extBuffPtr[0]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.extBuffPtr[1][%p]", (void*)USBToHostLong(shared->extBuffPtr[1]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.extBuffPtr[2][%p]", (void*)USBToHostLong(shared->extBuffPtr[2]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.extBuffPtr[3][%p]", (void*)USBToHostLong(shared->extBuffPtr[3]));
    USBLog(level, "AppleEHCIQueueHead::print - shared.extBuffPtr[4][%p]", (void*)USBToHostLong(shared->extBuffPtr[4]));
	USBLog(level, "AppleEHCIQueueHead::print - _qTD[%p]", _qTD);
	USBLog(level, "AppleEHCIQueueHead::print - _TailTD[%p]", _TailTD);
	USBLog(level, "AppleEHCIQueueHead::print - _pSPE[%p]", _pSPE);
	USBLog(level, "AppleEHCIQueueHead::print - _maxPacketSize[%p]", (void*)_maxPacketSize);
	USBLog(level, "AppleEHCIQueueHead::print - _functionNumber[%p]", (void*)_functionNumber);
	USBLog(level, "AppleEHCIQueueHead::print - _endpointNumber[%p]", (void*)_endpointNumber);
	USBLog(level, "AppleEHCIQueueHead::print - _direction[%p]", (void*)_direction);
	USBLog(level, "AppleEHCIQueueHead::print - _responseToStall[%p]", (void*)_responseToStall);
	USBLog(level, "AppleEHCIQueueHead::print - _queueType[%p]", (void*)_queueType);
	USBLog(level, "AppleEHCIQueueHead::print - _speed[%p] (%s)", (void*)_speed, _speed == kUSBDeviceSpeedLow ? "LS" : _speed == kUSBDeviceSpeedHigh ? "HS" : "FS");
	USBLog(level, "AppleEHCIQueueHead::print - _bInterval[%p]", (void*)_bInterval);
	USBLog(level, "AppleEHCIQueueHead::print - _startFrame[%p]", (void*)_startFrame);
	USBLog(level, "AppleEHCIQueueHead::print - _pollingRate[%p]", (void*)_pollingRate);
	USBLog(level, "----------------------------------------------------");
}


#undef super
#define super IOUSBControllerIsochListElement
// -----------------------------------------------------------------
//		AppleEHCIIsochTransferDescriptor
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleEHCIIsochTransferDescriptor, IOUSBControllerIsochListElement);

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
    GetSharedLogical()->nextiTD = HostToUSBLong(next);
}


IOPhysicalAddress
AppleEHCIIsochTransferDescriptor::GetPhysicalLink(void)
{
    return USBToHostLong(GetSharedLogical()->nextiTD);
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
	
	if (active) kIOUSBNotSent1Err
	else if (DBE) if (out)kIOUSBBufferUnderrunErr else kIOUSBBufferOverrunErr
	else if (babble) kIOReturnOverrun
	else if (Xacterr) kIOReturnNotResponding
	else if (in) if (length < maxpacketsize) kIOReturnUnderrun
	else
	kIOReturnSuccess
	*/
	if ((status & (kEHCI_ITDStatus_Active | kEHCI_ITDStatus_BuffErr | kEHCI_ITDStatus_Babble)) == 0)
	{
		if (((status & kEHCI_ITDStatus_XactErr) == 0) || (direction == kUSBIn))
		{
			// Isoch IN transactions can set the Xact_Err bit when the device sent the wrong PID (DATA2/1/0)
			// for the amount of data sent. For example, a device based on the Cypress EZ-USB FX2 chip set can send up 
			// to 3072 bytes per microframe (DATA2=1024, DATA1=1024, DATA0=1024). But if the device only has 1024 bytes
			// on a particular microframe, it sends it with a DATA2 PID. It then ignores the subsequent
			// IN PID - which it should not do, and which is a XactERR in the controller. However
			// the first 1024 bytes was transferred correctly, so we need to count that as an Underrun instead
			// of the XActErr. So this works around a bug in that Cypress chip set. (3915817)
			*transferLen = (status & kEHCI_ITDTr_Len) >> kEHCI_ITDTr_LenPhase;
			if ( (direction == kUSBIn) && (maxPacketSize != *transferLen) )
			{
				return(kIOReturnUnderrun);
			}
			return(kIOReturnSuccess);
		}
	}
	*transferLen = 0;
	
	if ( (status & kEHCI_ITDStatus_Active) != 0)
	{
		return(kIOUSBNotSent1Err);
	}
	else if ( (status & kEHCI_ITDStatus_BuffErr) != 0)
	{
		if (direction == kUSBOut)
		{
			return(kIOUSBBufferUnderrunErr);
		}
		else
		{
			return(kIOUSBBufferOverrunErr);
		}
	}
	else if ( (status & kEHCI_ITDStatus_Babble) != 0)
	{
		return(kIOReturnOverrun);
	}
	else // if ( (status & kEHCI_ITDStatus_XactErr) != 0)
	{
		return(kIOReturnNotResponding);
	}
}


IOReturn
AppleEHCIIsochTransferDescriptor::UpdateFrameList(AbsoluteTime timeStamp)
{
	// Do not use any USBLogs in this routine, as it's called from Filter Interrupt time
	//
    UInt32							*TransactionP, statusWord;
    IOUSBIsocFrame					*pFrames;    
    IOUSBLowLatencyIsocFrame		*pLLFrames;    
    IOReturn						ret, frStatus;
    int								i,j;
	UInt16							*pActCount;
	UInt8							framesInTD;
	
    ret = _pEndpoint->accumulatedStatus;
	
    TransactionP = &GetSharedLogical()->Transaction0;
    pFrames = _pFrames;
	framesInTD = _framesInTD;
	if (!pFrames || !framesInTD)							// this will be the case for the dummy TD
		return kIOReturnSuccess;
	
    pLLFrames = (IOUSBLowLatencyIsocFrame*)_pFrames;
    for(i=0, j=0; i < 8; i+= _pEndpoint->interval, j++)
    {
		if (!framesInTD)
			break;
		
	    statusWord = USBToHostLong(TransactionP[i]);

		if (_lowLatency)
			pActCount = &(pLLFrames[_frameIndex + j].frActCount);
		else
			pActCount = &(pFrames[_frameIndex + j].frActCount);
	    frStatus = mungeEHCIStatus(statusWord, pActCount,  _pEndpoint->maxPacketSize,  _pEndpoint->direction);

	    if (frStatus != kIOReturnSuccess)
	    {
		    if (frStatus != kIOReturnUnderrun)
		    {
			    ret = frStatus;
		    }
		    else if (ret == kIOReturnSuccess)
		    {
			    ret = kIOReturnUnderrun;
		    }
	    }
	    if (_lowLatency)
	    {
			if ( _requestFromRosettaClient )
			{
				pLLFrames[_frameIndex + j].frActCount = OSSwapInt16(pLLFrames[_frameIndex + j].frActCount);
				pLLFrames[_frameIndex + j].frReqCount = OSSwapInt16(pLLFrames[_frameIndex + j].frReqCount);
				AbsoluteTime_to_scalar(&pLLFrames[_frameIndex + j].frTimeStamp) 
				= OSSwapInt64(AbsoluteTime_to_scalar(&timeStamp));
				pLLFrames[_frameIndex + j].frStatus = OSSwapInt32(frStatus);
			}
			else
			{
				pLLFrames[_frameIndex + j].frStatus = frStatus;
				pLLFrames[_frameIndex + j].frTimeStamp = timeStamp;
			}
	    }
	    else
	    {
			if ( _requestFromRosettaClient )
			{
				pFrames[_frameIndex + j].frActCount = OSSwapInt16(pFrames[_frameIndex + j].frActCount);
				pFrames[_frameIndex + j].frReqCount = OSSwapInt16(pFrames[_frameIndex + j].frReqCount);
				pFrames[_frameIndex + j].frStatus = OSSwapInt32(frStatus);
			}
			else
			{
				pFrames[_frameIndex + j].frStatus = frStatus;
			}
	    }
		framesInTD--;
    }
    _pEndpoint->accumulatedStatus = ret;
    return ret;
}



IOReturn
AppleEHCIIsochTransferDescriptor::Deallocate(IOUSBControllerV2 *uim)
{
    return ((AppleUSBEHCI*)uim)->DeallocateITD(this);
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
OSDefineMetaClassAndStructors(AppleEHCISplitIsochTransferDescriptor, IOUSBControllerIsochListElement);
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
    GetSharedLogical()->nextSITD = HostToUSBLong(next);
}


IOPhysicalAddress
AppleEHCISplitIsochTransferDescriptor::GetPhysicalLink(void)
{
    return USBToHostLong(GetSharedLogical()->nextSITD);
}


IOPhysicalAddress 
AppleEHCISplitIsochTransferDescriptor::GetPhysicalAddrWithType(void)
{
    return _sharedPhysical | (kEHCITyp_siTD << kEHCIEDNextED_TypPhase);
}


IOReturn
AppleEHCISplitIsochTransferDescriptor::UpdateFrameList(AbsoluteTime timeStamp)
{
    UInt32						statFlags;
    IOUSBIsocFrame 				*pFrames;    
    IOUSBLowLatencyIsocFrame 	*pLLFrames;    
    IOReturn					frStatus = kIOReturnSuccess;
    UInt16						frActualCount = 0;
    UInt16						frReqCount;
    
    statFlags = USBToHostLong(GetSharedLogical()->statFlags);
    // warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
    // USBLog(7, "AppleEHCISplitIsochTransferDescriptor[%p]::UpdateFrameList statFlags (%x)", this, statFlags);
    pFrames = _pFrames;
	if (!pFrames || _isDummySITD)							// this will be the case for the dummy TD
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
	
    if ((statFlags & kEHCIsiTDStatStatusActive) && !_isDummySITD)
    {
		frStatus = kIOUSBNotSent2Err;
    }
    else if (statFlags & kEHCIsiTDStatStatusERR)
    {
		frStatus = kIOReturnNotResponding;
    }
    else if (statFlags & kEHCIsiTDStatStatusDBE)
    {
		if (_pEndpoint->direction == kUSBOut)
			frStatus = kIOUSBBufferUnderrunErr;
		else
			frStatus = kIOUSBBufferOverrunErr;
    }
    else if (statFlags & kEHCIsiTDStatStatusBabble)
    {
		if (_pEndpoint->direction == kUSBOut)
			frStatus = kIOReturnNotResponding;							// babble on OUT. this should never happen
		else
			frStatus = kIOReturnOverrun;
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
			if (_pEndpoint->direction == kUSBOut)
			{
				// warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
				// USBLog(7, "AppleEHCISplitIsochTransferDescriptor[%p]::UpdateFrameList - (OUT) reqCount (%d) actCount (%d)", this, frReqCount, frActualCount);
				frStatus = kIOUSBBufferUnderrunErr;
			}
			else if (_pEndpoint->direction == kUSBIn)
			{
				// warning - this method can run at primary interrupt time, which can cause a panic if it logs too much
				// USBLog(7, "AppleEHCISplitIsochTransferDescriptor[%p]::UpdateFrameList - (IN) reqCount (%d) actCount (%d)", this, frReqCount, frActualCount);
				frStatus = kIOReturnUnderrun;
			}
		}
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
			
#ifdef __LP64__
			USBTrace( kUSBTUHCIInterrupts, kTPUHCIUpdateFrameList , (uintptr_t)((_pEndpoint->direction << 24) | ( _pEndpoint->functionAddress << 8) | _pEndpoint->endpointNumber), (uintptr_t)&pLLFrames[_frameIndex], (uintptr_t)frActualCount, (uintptr_t)timeStamp );
#else
			USBTrace( kUSBTEHCIInterrupts, kTPEHCIUpdateFrameList , (uintptr_t)((_pEndpoint->direction << 24) | ( _pEndpoint->functionAddress << 8) | _pEndpoint->endpointNumber), (uintptr_t)&pLLFrames[_frameIndex], (uintptr_t)(timeStamp.hi), (uintptr_t)timeStamp.lo );
#endif
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
	
    if (frStatus != kIOReturnSuccess)
    {
		if (frStatus != kIOReturnUnderrun)
		{
			_pEndpoint->accumulatedStatus = frStatus;
		}
		else if (_pEndpoint->accumulatedStatus == kIOReturnSuccess)
		{
			_pEndpoint->accumulatedStatus = kIOReturnUnderrun;
		}
    }
	
    return frStatus;
}



IOReturn
AppleEHCISplitIsochTransferDescriptor::Deallocate(IOUSBControllerV2 *uim)
{
    return ((AppleUSBEHCI*)uim)->DeallocateSITD(this);
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



#undef super
#define super IOUSBControllerIsochEndpoint
OSDefineMetaClassAndStructors(AppleEHCIIsochEndpoint, IOUSBControllerIsochEndpoint);
// -----------------------------------------------------------------
//		AppleEHCIIsochEndpoint
// -----------------------------------------------------------------
bool
AppleEHCIIsochEndpoint::init()
{
	int			i;
	bool		ret;
	
	ret = super::init();
	if (ret)
	{
		ttiPtr = NULL;
		oneMPS = 0;
		highSpeedHub = highSpeedPort = 0;
		useBackPtr = false;
		pSPE = NULL;
	}
	return ret;
}



void
AppleEHCIIsochEndpoint::print(int level)
{
	USBLog(level, "AppleEHCIIsochEndpoint[%p]::print - pSPE(%p)", this, pSPE);
	USBLog(level, "AppleEHCIIsochEndpoint[%p]::print - ttiPtr(%p)", this, ttiPtr);
	USBLog(level, "AppleEHCIIsochEndpoint[%p]::print - oneMPS(%d)", this, (int)oneMPS);
	USBLog(level, "AppleEHCIIsochEndpoint[%p]::print - highSpeedHub(%d)", this, (int)highSpeedHub);
	USBLog(level, "AppleEHCIIsochEndpoint[%p]::print - highSpeedPort(%d)", this, highSpeedPort);
	USBLog(level, "AppleEHCIIsochEndpoint[%p]::print - useBackPtr(%s)", this, useBackPtr ? "true" : "false");
	USBLog(level, "AppleEHCIIsochEndpoint[%p]::print - _speed(%d)", this, (int)_speed);
	USBLog(level, "AppleEHCIIsochEndpoint[%p]::print - _startFrame(%d)", this, (int)_startFrame);
}
