//
//  AppleUSBXHCI_IsocQueues.cpp
//  AppleUSBXHCI
//
//  Copyright 2011-2012 Apple Inc. All rights reserved.
//

#include "AppleUSBXHCI_IsocQueues.h"
#include "AppleUSBXHCIUIM.h"


#ifndef XHCI_USE_KPRINTF 
#define XHCI_USE_KPRINTF 0
#endif

#if XHCI_USE_KPRINTF
#undef USBLog
#undef USBError
void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= XHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBLogKP( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= XHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#else
// USBLogKP can only be used for kprintf builds
#define USBLogKP( LEVEL, FORMAT, ARGS... )
#endif

#if (DEBUG_REGISTER_READS == 1)
#define Read32Reg(registerPtr, ...) Read32RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define Read32RegWithFileInfo(registerPtr, function, file, line, ...) (															\
	fTempReg = Read32Reg(registerPtr, ##__VA_ARGS__),																			\
	fTempReg = (fTempReg == (typeof (*(registerPtr))) -1) ?																		\
		(kprintf("AppleUSBXHCI[%p]::%s Invalid register at %s:%d %s\n", this,function,file, line,#registerPtr), -1) : fTempReg,	\
	(typeof(*(registerPtr)))fTempReg)

#define Read64Reg(registerPtr, ...) Read64RegWithFileInfo(registerPtr, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define Read64RegWithFileInfo(registerPtr, function, file, line, ...) (															\
	fTempReg = Read64Reg(registerPtr, ##__VA_ARGS__),																			\
	fTempReg = (fTempReg == (typeof (*(registerPtr))) -1) ?																		\
		(kprintf("AppleUSBXHCI[%p]::%s Invalid register at %s:%d %s\n", this,function,file, line,#registerPtr), -1) : fTempReg,	\
	(typeof(*(registerPtr)))fTempReg)
#endif

#undef super
#define super IOUSBControllerIsochListElement
// -----------------------------------------------------------------
//		AppleXHCIIsochTransferDescriptor
// -----------------------------------------------------------------
OSDefineMetaClassAndStructors(AppleXHCIIsochTransferDescriptor, IOUSBControllerIsochListElement);

AppleXHCIIsochTransferDescriptor *
AppleXHCIIsochTransferDescriptor::ForEndpoint(AppleXHCIIsochEndpoint *ep)
{
    AppleXHCIIsochTransferDescriptor *me = OSTypeAlloc(AppleXHCIIsochTransferDescriptor);
	
    if (!me || !me->init())
		return NULL;
	
	me->_pEndpoint = ep;
	me->newFrame = false;
	
    return me;
}



void 
AppleXHCIIsochTransferDescriptor::SetPhysicalLink(IOPhysicalAddress next)
{
#pragma unused(next)
	
	USBError(1, "AppleXHCIIsochTransferDescriptor[%p]::SetPhysicalLink - not implemented", this);
}


IOPhysicalAddress
AppleXHCIIsochTransferDescriptor::GetPhysicalLink(void)
{
	USBError(1, "AppleXHCIIsochTransferDescriptor[%p]::GetPhysicalLink - not implemented", this);
    return 0;
}


IOPhysicalAddress 
AppleXHCIIsochTransferDescriptor::GetPhysicalAddrWithType(void)
{
	USBError(1, "AppleXHCIIsochTransferDescriptor[%p]::GetPhysicalAddrWithType - not implemented", this);
	return 0;
}


IOReturn 
AppleXHCIIsochTransferDescriptor::MungeXHCIStatus(UInt32 status, UInt16 *transferLen, UInt32 maxPacketSize, UInt8 direction)
{
#pragma unused (transferLen, maxPacketSize, direction)
	
	switch(status)
	{
        case kXHCITRB_CC_XActErr:
            return kIOUSBNotSent1Err;
            
		case kXHCITRB_CC_ShortPacket:
			return kIOReturnUnderrun;
			
		case kXHCITRB_CC_Success:
			return kIOReturnSuccess;
			
		case kXHCITRB_CC_STALL:
			return kIOUSBPipeStalled;
            
		default:
			return kIOReturnInternalError;
	}

#if 0
	THIS IS HOW EHCI DID IT - LEAVING HERE FOR REFERENCE
	
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
		USBTrace( kUSBTEHCIInterrupts, kTPEHCIUpdateFrameListBits, (uintptr_t)((_pEndpoint->direction << 24) | ( _pEndpoint->functionAddress << 8) | _pEndpoint->endpointNumber), 0, 0, 8);
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
#endif
	
}



int
AppleXHCIIsochTransferDescriptor::FrameForEventIndex(UInt32 eventIndex)
{
	int i;
	
	for (i=0; i < _framesInTD; i++)
	{
		if ((eventIndex >= trbIndex[i] && (eventIndex < (trbIndex[i] + numTRBs[i]))))
			return i;
	}
			
	return -1;
}



IOReturn
AppleXHCIIsochTransferDescriptor::UpdateFrameList(AbsoluteTime timeStamp)
{
#pragma unused(timeStamp)
	
	UInt64							phys = ((UInt64)USBToHostLong(eventTRB.offs0)) + (((UInt64)USBToHostLong(eventTRB.offs4)) << 32);
	XHCIRing *						ringX = ((AppleXHCIIsochEndpoint *)_pEndpoint)->ring;
	IOUSBLowLatencyIsocFrame *		pLLFrames = (IOUSBLowLatencyIsocFrame*)_pFrames;
	int								eventIndex;
	int								frameForEvent;
	UInt32							myTDIndex;
	IOReturn						ret = kIOReturnSuccess;
	int								i;
	
	//			WARNING
	//
	//	UpdateFrameList is called a primary interrupt time, so logging (except kprintf) is prohibited
	//
	
    ret = _pEndpoint->accumulatedStatus;
	
	if (phys == 0)
	{
		// this will be the case if we did not actually send this request to the hardware (i.e. it came in too late)
		USBLogKP(7, "AppleXHCIIsochTransferDescriptor[%p]::UpdateFrameList - no real event - erroring out pFrames\n", this);
		for (i=0; i < _framesInTD; i++)
		{
			if (!statusUpdated[i])
			{
				int		frIdx = _frameIndex + i;

				if (_lowLatency)
				{
					pLLFrames[frIdx].frActCount = 0;
					pLLFrames[frIdx].frStatus = kIOUSBNotSent1Err;
					pLLFrames[frIdx].frTimeStamp = timeStamp;
				}
				else
				{
					_pFrames[frIdx].frActCount = 0;
					_pFrames[frIdx].frStatus = kIOUSBNotSent1Err;
				}
				statusUpdated[i] = true;
			}
		}
		return kIOUSBNotSent1Err;
	}
	
	
	eventIndex = AppleUSBXHCI::DiffTRBIndex(phys, ringX->transferRingPhys);
	
	if ((eventIndex < 0) || (eventIndex > ringX->transferRingSize))
	{
		USBLogKP(5, "AppleXHCIIsochTransferDescriptor[%p]::UpdateFrameList - event outside my transfer ring - ignoring\n", this);
		return kIOReturnSuccess;
	}
		
	frameForEvent = FrameForEventIndex(eventIndex);
	if (frameForEvent < 0)
	{
		USBLogKP(7, "AppleXHCIIsochTransferDescriptor[%p]::UpdateFrameList - event does not match any of my frames - assuming all is good!\n", this);
		for (i=0; i < _framesInTD; i++)
		{
			if (!statusUpdated[i])
			{
				int		frIdx = _frameIndex + i;
				
				if (_lowLatency)
				{
					pLLFrames[frIdx].frActCount = pLLFrames[frIdx].frReqCount;
					pLLFrames[frIdx].frStatus = kIOReturnSuccess;
					pLLFrames[frIdx].frTimeStamp = timeStamp;
				}
				else
				{
					_pFrames[frIdx].frActCount = _pFrames[frIdx].frReqCount;
					_pFrames[frIdx].frStatus = kIOReturnSuccess;
				}
				statusUpdated[i] = true;
				USBLogKP(7, "XHCI Isoc frame (%d.%d) frIdx(%d) FULL\n", (int)((UInt32)_frameNumber & 0x7ff), i, (int)_frameIndex);
			}
		}
		return kIOReturnSuccess;
	}
	
	for (i=0; i < _framesInTD; i++)
	{
		if (statusUpdated[i])
		{
            // this will allow us to process only one TRB per microframe
			continue;
		}
		
		if (i > frameForEvent)
		{
			USBLogKP(5, "AppleXHCIIsochTransferDescriptor[%p]::UpdateFrameList - the event frame(%d) in earlier than this frame(%d) - done\n", this, frameForEvent, i);
			break;
		}
		else
		{
			
			int			frIdx = _frameIndex + i;

			if (i == frameForEvent)
			{
				// the event points to a TRB within my TD
				UInt8							condCode = 	((USBToHostLong(eventTRB.offs8) & kXHCITRB_CC_Mask) >> kXHCITRB_CC_Shift);
				UInt32							eventLen = USBToHostLong(eventTRB.offs8) & kXHCITRB_TR_Len_Mask;
				IOReturn						frStatus = MungeXHCIStatus(condCode, NULL, 0, 0);
				bool							edEvent = ((USBToHostLong(eventTRB.offsC) & kXHCITRB_ED) != 0);

				if (condCode == kXHCITRB_CC_XActErr)
				{
					AppleXHCIIsochEndpoint *		pEP = (AppleXHCIIsochEndpoint *)_pEndpoint;
					
					if ((pEP->direction == kUSBIn) && (pEP->speed == kUSBDeviceSpeedHigh) && (pEP->mult > 1))
					{
						// similar to what EHCI does here.. Some old High Speed Isoc devices issue the incorrect PID when they are doing High Bandwidth
						// transfers (more than 1 IN token in the same uFrame). They still transfer good data, so we need to try to figure out
						// exactly how many of those data actually came in
						
						// if this is a multi-TRB TD, then we will skip any event which comes in which is not an edEvent. Otherwise, we will just
						// fake the status and process things normally
						
						if ((numTRBs[i] > 1) && !edEvent)
							break;										// break out of the for loop since we know that the ed is coming
						
						frStatus = kIOReturnUnderrun;					// change this to an underrun so we keep going
					}
					
				}
				if (frStatus != kIOReturnSuccess)
				{
					if (frStatus != kIOReturnUnderrun)
					{
						USBLogKP(2, "XHCI: bad frStatus condCode(%d) eventLen(%d) edEvent(%s) frame (%d.%d) numTRBs(%d) [%08x] [%08x] [%08x] [%08x]\n", (int)condCode, (int)eventLen, edEvent ? "true" : "false", (int)((UInt32)_frameNumber & 0x7ff), i, (int)numTRBs[i], (int)eventTRB.offs0, (int)eventTRB.offs4, (int)eventTRB.offs8, (int)eventTRB.offsC);
						_pEndpoint->accumulatedStatus = frStatus;
                        eventLen = 0;                                  // this will be an XACT err, e.g. (munged to NotSent)
                        edEvent = true;                                // just to make sure that that we interpret the length correctly - it will use the eventLen
					}
					else if (_pEndpoint->accumulatedStatus == kIOReturnSuccess)
					{
						_pEndpoint->accumulatedStatus = kIOReturnUnderrun;
					}
					ret = frStatus;
				}
					
				USBLogKP(7, "AppleXHCIIsochTransferDescriptor[%p]::UpdateFrameList frame(%d.%d) frIdx(%d) frStatus(%08x) eventLen(%d) edEvent(%s)\n", this, (int)((UInt32)_frameNumber & 0x7ff), i, frIdx, (int)frStatus, (int)eventLen, edEvent ? "true" : "false");
				if (_lowLatency)
				{
					// the eventTRB will either point to an Event TRB that we put into the list or to an Isoch TRB
					// if the former, then edEvent will be T and the length will be the total length transferred
					// otherwise evenLength will be the bytes remaining from the Isoch TD
					
					if (edEvent)
						pLLFrames[frIdx].frActCount = eventLen;
					else
						pLLFrames[frIdx].frActCount = pLLFrames[frIdx].frReqCount - eventLen;
						
					pLLFrames[frIdx].frStatus = frStatus;
					pLLFrames[frIdx].frTimeStamp = timeStamp;						// update time stamp last always
					USBLogKP(7, "XHCI Isoc(LL) frame (%d.%d) frIdx (%d) frReq(%d) frAct(%d) frStat(%x)\n", (int)((UInt32)_frameNumber & 0x7ff), i, (int)frIdx, pLLFrames[frIdx].frReqCount, pLLFrames[frIdx].frActCount, pLLFrames[frIdx].frStatus);
				}
				else
				{
					if (edEvent)
						_pFrames[frIdx].frActCount = eventLen;
					else
						_pFrames[frIdx].frActCount = _pFrames[frIdx].frReqCount - eventLen;
					
					_pFrames[frIdx].frStatus = frStatus;
					
					USBLogKP(7, "XHCI Isoc frame (%d.%d) frIdx(%d) frReq(%d) frAct(%d) frStat(%x)\n", (int)((UInt32)_frameNumber & 0x7ff), i, (int)frIdx, _pFrames[frIdx].frReqCount, _pFrames[frIdx].frActCount, _pFrames[frIdx].frStatus);
				}
				statusUpdated[i] = true;
				break;						// break out of the for loop as we won't be able to process any more frames in this TD
			}
			else
			{
				USBLogKP(7, "AppleXHCIIsochTransferDescriptor[%p]::UpdateFrameList frIdx(%d) long ago - making all statii good!\n", this, frIdx);
				// the event occured after (possibly long after) my TD was processed This is a good thing in that
				// it means that there were no errors on my TD (for OUT or IN) and that there were no short packets (for IN)
				// so we can update all of the status to good and the actCount to the ReqCount
				if (_lowLatency)
				{
					pLLFrames[frIdx].frActCount = pLLFrames[frIdx].frReqCount;
					pLLFrames[frIdx].frStatus = kIOReturnSuccess;
					pLLFrames[frIdx].frTimeStamp = timeStamp;
					USBLogKP(7, "XHCI Isoc(LL) frame (%d.%d) frIdx (%d) frReq(%d) frAct(%d) frStat(%x)\n", (int)((UInt32)_frameNumber & 0x7ff), i, (int)frIdx, pLLFrames[frIdx].frReqCount, pLLFrames[frIdx].frActCount, pLLFrames[frIdx].frStatus);
				}
				else
				{
					_pFrames[frIdx].frActCount = _pFrames[frIdx].frReqCount;
					_pFrames[frIdx].frStatus = kIOReturnSuccess;					
					USBLogKP(7, "XHCI Isoc frame (%d.%d) frIdx(%d) frReq(%d) frAct(%d) frStat(%x)\n", (int)((UInt32)_frameNumber & 0x7ff), i, (int)frIdx, _pFrames[frIdx].frReqCount, _pFrames[frIdx].frActCount, _pFrames[frIdx].frStatus);
				}
				statusUpdated[i] = true;
			}
		}
	}

	USBLogKP(7, "AppleXHCIIsochTransferDescriptor[%p]::UpdateFrameList - returning(%08x)\n", this, ret);
	return ret;
}



IOReturn
AppleXHCIIsochTransferDescriptor::Deallocate(IOUSBControllerV2 *uim)
{
#pragma unused (uim)

	release();
	
    return kIOReturnSuccess;
}


void
AppleXHCIIsochTransferDescriptor::print(int level)
{
	int					listCount, i;
    
    super::print(level);
}



#undef super
#define super IOUSBControllerIsochEndpoint
OSDefineMetaClassAndStructors(AppleXHCIIsochEndpoint, IOUSBControllerIsochEndpoint);
// -----------------------------------------------------------------
//		AppleXHCIIsochEndpoint
// -----------------------------------------------------------------
bool
AppleXHCIIsochEndpoint::init()
{
	int			i;
	bool		ret;
	
	ret = super::init();
	if (ret)
	{
		wdhLock = IOSimpleLockAlloc();
		if (!wdhLock)
			ret = false;
		else
		{
			inSlot = kNumTDSlots+1;
			outSlot = kNumTDSlots + 1;
		}
	}
	return ret;
}



void									
AppleXHCIIsochEndpoint::free(void)
{
	USBLog(7, "AppleXHCIIsochEndpoint[%p]::free", this);
	if (wdhLock)
	{
		IOSimpleLockFree(wdhLock);
		wdhLock = NULL;
	}
	super::free();
}



void
AppleXHCIIsochEndpoint::print(int level)
{
	
	USBLog(level, "AppleXHCIIsochEndpoint[%p]::print - maxPacketSize(%d)", this, (int)maxPacketSize);
	USBLog(level, "AppleXHCIIsochEndpoint[%p]::print - mult(%d)", this, (int)mult);
	USBLog(level, "AppleXHCIIsochEndpoint[%p]::print - maxBurst(%d)", this, (int)maxBurst);
	USBLog(level, "AppleXHCIIsochEndpoint[%p]::print - ringSizeInPages(%d)", this, (int)ringSizeInPages);
	USBLog(level, "AppleXHCIIsochEndpoint[%p]::print - transactionsPerFrame(%d)", this, (int)transactionsPerFrame);
	USBLog(level, "AppleXHCIIsochEndpoint[%p]::print - inSlot(%d)", this, (int)inSlot);
	USBLog(level, "AppleXHCIIsochEndpoint[%p]::print - outSlot(%d)", this, (int)outSlot);
}



#undef super
#define super IOUSBControllerV3
// -----------------------------------------------------------------
//		AppleUSBXHCI
// -----------------------------------------------------------------
IOReturn		
AppleUSBXHCI::AbortIsochEP(AppleXHCIIsochEndpoint* pEP)
{
    uint64_t							timeStamp;
    IOReturn							err;
    UInt32								slot;
    AppleXHCIIsochTransferDescriptor	*pTD;


	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(6, "+AppleUSBXHCI[%p]::AbortIsochEP[%p] - start - pEP->outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP,  pEP->outSlot, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)pEP->consumerCount, (uint32_t)pEP->producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
        USBTrace_Start( kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP, pEP->outSlot, pEP->inSlot, pEP->activeTDs );
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->onToDoList, (uintptr_t)pEP->toDoList, pEP->deferredTDs, 0);
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->deferredQueue, pEP->scheduledTDs, pEP->onProducerQ, 1);
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, pEP->consumerCount, pEP->producerCount, pEP->onReversedList, 2);
        USBTrace_End( kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->onDoneQueue, (uintptr_t)pEP->doneQueue, (uintptr_t)pEP->ring->transferRing, 0 );
	}
	
	pEP->aborting = true;
    USBLog(7, "AppleUSBXHCI[%p]::AbortIsochEP (%p)", this, pEP);
	
	// XHCI is way cleaner than any of the earlier controllers, because each endpoint is serviced independently on XHCI
	// therefore, we know that by the time we get here, the endpoint is already stopped, and we do not have to worry about
	// the filter interrupt coming in

	timeStamp = mach_absolute_time();
	
    // now scavange any transactions which were already on the done queue, but don't put any new ones onto the scheduled queue, since we
    // are aborting
    USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP, 0, 0, 3);
    err = ScavengeIsocTransactions(pEP, false);

    if (err)
    {
		USBLog(1, "AppleUSBXHCI[%p]::AbortIsochEP - err (0x%x) from scavengeIsocTransactions", this, err);
		//USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uintptr_t)this, err, 0, 1 );
    }
    
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(6, "+AppleUSBXHCI[%p]::AbortIsochEP[%p] - after scavenge - pEP->outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP,  pEP->outSlot, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue,(uint32_t) pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)pEP->consumerCount, (uint32_t)pEP->producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
        USBTrace_Start( kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP, pEP->outSlot, pEP->inSlot, pEP->activeTDs );
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->onToDoList, (uintptr_t)pEP->toDoList, pEP->deferredTDs, 0);
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->deferredQueue, pEP->scheduledTDs, pEP->onProducerQ, 1);
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, pEP->consumerCount, pEP->producerCount, pEP->onReversedList, 2);
        USBTrace_End( kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->onDoneQueue, (uintptr_t)pEP->doneQueue, 0, 1 );
	}
	
	// now get all of the transactions which had already been placed on the ring for processing, but which had not yet generated an event
    
    if ((pEP->outSlot < kNumTDSlots) && (pEP->inSlot < kNumTDSlots))
    {
		bool			stopAdvancing = false;
		UInt32			stopSlot;
		
        // now scavenge the remaining transactions on the periodic list
        slot = pEP->outSlot;
		stopSlot = pEP->inSlot;
        while (slot != stopSlot)
        {
			UInt32							nextSlot;
            
			nextSlot = (slot+1) & (kNumTDSlots-1);
			pTD = pEP->tdSlots[slot];
			
			if (pTD == NULL && (nextSlot != pEP->inSlot))
				pEP->outSlot = nextSlot;
			
            if (pTD != NULL)
            {
				pTD->eventTRB.offs0 = 0;
				pTD->eventTRB.offs4 = 0;
				pTD->eventTRB.offs8 = 0;
				pTD->eventTRB.offsC = 0;
				
				USBLog(6, "AppleUSBXHCI[%p]::AbortIsochEP (%p) - removing pTD(%p) from slot(%d) pEP->ringRunning(%s)", this, pEP, pTD, slot, pEP->ringRunning ? "true" : "false");
				(void) pTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);											// TODO - accumulate the return values or force abort err
				OSDecrementAtomic( &(pEP->scheduledTDs));
				if ( pEP->scheduledTDs < 0 )
				{
					USBLog(1, "AppleUSBXHCI[%p]::AbortIsochEP (%p) - scheduledTDs is negative! (%d)", this, pEP, (uint32_t)pEP->scheduledTDs);
					// USBTrace( kUSBTEHCI, kTPEHCIAbortIsochEP, (uintptr_t)this, (uintptr_t)pEP, (uint32_t)pEP->scheduledTDs, 2 );
				}
                USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP, (UInt32)pTD->_frameNumber, pEP->scheduledTDs, 4);
				PutTDonDoneQueue(pEP, pTD, true	);
				pEP->tdSlots[slot] = NULL;
            }
            slot = nextSlot;
        }
		pEP->outSlot = kNumTDSlots+1;
		pEP->inSlot = kNumTDSlots+1;
    }
    
    // now transfer any transactions from the todo list to the done queue
    pTD = (AppleXHCIIsochTransferDescriptor*)GetTDfromToDoList(pEP);
    while (pTD)
    {
		(void) pTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);
		PutTDonDoneQueue(pEP, pTD, true);
		pTD = (AppleXHCIIsochTransferDescriptor*)GetTDfromToDoList(pEP);
    }
	
	if (pEP->scheduledTDs == 0)
	{
		// since we have no Isoch xactions on the endpoint, we can reset the counter
		pEP->firstAvailableFrame = 0;
		pEP->inSlot = kNumTDSlots + 1;    
	}
	
    
    pEP->accumulatedStatus = kIOReturnAborted;
    ReturnIsochDoneQueue(pEP);
    
    pEP->accumulatedStatus = kIOReturnSuccess;
	if (pEP->deferredQueue || pEP->toDoList || pEP->doneQueue || pEP->activeTDs || pEP->onToDoList || pEP->scheduledTDs || pEP->deferredTDs || pEP->onReversedList || pEP->onDoneQueue)
	{
		USBLog(1, "+AppleUSBXHCI[%p]::AbortIsochEP[%p] - done - pEP->outSlot (0x%x) pEP->inSlot (0x%x) activeTDs (%d) onToDoList (%d) todo (%p) deferredTDs (%d) deferred(%p) scheduledTDs (%d) onProducerQ (%d) consumer (%d) producer (%d) onReversedList (%d) onDoneQueue (%d)  doneQueue (%p)", this, pEP,  pEP->outSlot, (uint32_t)pEP->inSlot, (uint32_t)pEP->activeTDs, (uint32_t)pEP->onToDoList, pEP->toDoList, (uint32_t)pEP->deferredTDs, pEP->deferredQueue, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->onProducerQ, (uint32_t)pEP->consumerCount, (uint32_t)pEP->producerCount, (uint32_t)pEP->onReversedList, (uint32_t)pEP->onDoneQueue, pEP->doneQueue);
        USBTrace_Start( kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP, pEP->outSlot, pEP->inSlot, pEP->activeTDs );
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->onToDoList, (uintptr_t)pEP->toDoList, pEP->deferredTDs, 0);
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->deferredQueue, pEP->scheduledTDs, pEP->onProducerQ, 1);
        USBTrace(kUSBTXHCI, kTPXHCIAbortIsochEP, pEP->consumerCount, pEP->producerCount, pEP->onReversedList, 2);
        USBTrace_End( kUSBTXHCI, kTPXHCIAbortIsochEP, (uintptr_t)pEP->onDoneQueue, (uintptr_t)pEP->doneQueue, 0, 2 );
	}
	else
	{
		USBLog(6, "-AppleUSBXHCI[%p]::AbortIsochEP[%p] - done - all clean - pEP->outSlot (0x%x), pEP->inSlot (0x%x)", this, pEP, pEP->outSlot, pEP->inSlot);
	}
    
	pEP->aborting = false;
    return kIOReturnSuccess;
}



IOReturn		
AppleUSBXHCI::DeleteIsochEP(AppleXHCIIsochEndpoint* pEP)
{
    IOUSBControllerIsochEndpoint* 		curEP, *prevEP;
    // UInt32								currentMaxPacketSize;
	
    USBTrace_Start(kUSBTXHCI, kTPXHCIDeleteIsochEP,  (uintptr_t)this, 0, 0, 0);
	
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
	// currentMaxPacketSize = pEP->maxPacketSize;
	kprintf("DeleteIsochEP - releasing pEP(%p)\n", pEP);
	pEP->release();
	
   	kprintf("DeleteIsochEP - returning success\n");
	
    USBTrace_End(kUSBTXHCI, kTPXHCIDeleteIsochEP,  (uintptr_t)this, kIOReturnSuccess, 0, 0);
    return kIOReturnSuccess;
}



IOReturn 
AppleUSBXHCI::ScavengeAnIsocTD(AppleXHCIIsochEndpoint *pEP,  AppleXHCIIsochTransferDescriptor *pTD)
{
    IOReturn						ret;


	// in EHCI and the other UIMs, we would update the frame list (pTD->UpdateFrameList) here.
	// XHCI has already done so in the filter interrupt routine

	PutTDonDoneQueue(pEP, pTD, true);

    return kIOReturnSuccess;
}



IOReturn 
AppleUSBXHCI::ScavengeIsocTransactions(AppleXHCIIsochEndpoint *pEP, bool reQueueTransactions)
{
    AppleXHCIIsochTransferDescriptor 	*pDoneTD;
    UInt32								cachedProducer;
    UInt32								cachedConsumer;
    AppleXHCIIsochTransferDescriptor	*prevTD;
    AppleXHCIIsochTransferDescriptor	*nextTD;
    IOInterruptState					intState;
	
    // Get the values of the Done Queue Head and the producer count.  We use a lock and disable interrupts
    // so that the filter routine does not preempt us and updates the values while we're trying to read them.
    //
    intState = IOSimpleLockLockDisableInterrupt( pEP->wdhLock );
    
    pDoneTD = (AppleXHCIIsochTransferDescriptor*)pEP->savedDoneQueueHead;
    cachedProducer = pEP->producerCount;
    
    IOSimpleLockUnlockEnableInterrupt( pEP->wdhLock, intState );
    
    cachedConsumer = pEP->consumerCount;
	
    USBTrace(kUSBTXHCI, kTPXHCIScavengeIsocTransactions, (uintptr_t)pEP, cachedConsumer, cachedProducer, 0);
    if (pDoneTD && (cachedConsumer != cachedProducer))
    {
		// there is real work to do - first reverse the list
		prevTD = NULL;
		USBLog(7, "AppleUSBXHCI[%p]::scavengeIsocTransactions - before reversal, cachedConsumer = 0x%x", this, (uint32_t)cachedConsumer);
		while (true)
		{
			pDoneTD->_logicalNext = prevTD;
			prevTD = pDoneTD;
			cachedConsumer++;
			OSDecrementAtomic( &(pEP->onProducerQ));
			pEP->onReversedList++;
			if ( cachedProducer == cachedConsumer)
				break;
			
			pDoneTD = (AppleXHCIIsochTransferDescriptor*)pDoneTD->_doneQueueLink;
		}
		
		// update the consumer count
		pEP->consumerCount = cachedConsumer;
		
		USBLog(7, "AppleUSBXHCI[%p]::scavengeIsocTransactions - after reversal, cachedConsumer[0x%x]", this, (uint32_t)cachedConsumer);
		// now cachedDoneQueueHead points to the head of the done queue in the right order
		while (pDoneTD)
		{
			nextTD = (AppleXHCIIsochTransferDescriptor*)pDoneTD->_logicalNext;
			pDoneTD->_logicalNext = NULL;
			pEP->onReversedList--;
			USBLog(7, "AppleUSBXHCI[%p]::scavengeIsocTransactions - about to scavenge TD %p", this, pDoneTD);
			ScavengeAnIsocTD(pEP, pDoneTD);
			pDoneTD = nextTD;
		}
    }
    
    USBTrace(kUSBTXHCI, kTPXHCIScavengeIsocTransactions, (uintptr_t)pEP, reQueueTransactions, 0, 1);
    if ( reQueueTransactions )
    {
        AppleXHCIIsochTransferDescriptor *  iTD = OSDynamicCast(AppleXHCIIsochTransferDescriptor, pEP->doneEnd);
        
        if (iTD && iTD->_completion.action)
        {
            // this is a callback ITD
            UInt64      curFrame = GetFrameNumber();
            if ((!_lostRegisterAccess) && (iTD->_frameNumber == (curFrame + 1)))
            {
                UInt32      mfindex = Read32Reg(&_pXHCIRuntimeReg->MFINDEX);
				if (!_lostRegisterAccess)
				{
					// the XHCI will finish things early because of the HS Bus Frame/Host Frame paradigm (EHCI spec section 4.5)
					if ((mfindex & 7) == 7)              // we are in the last uFrame of the previous frame (as expected)
					{
						USBTrace(kUSBTXHCI, kTPXHCIScavengeIsocTransactions, (uintptr_t)pEP, (int)curFrame, (int)iTD->_frameNumber, 2);
						IODelay(125);               // need to stall the callback until the correct frame is up
					}
				}
            }
        }
        
		USBLog(7, "AppleUSBXHCI[%p]::scavengeIsocTransactions - calling ReturnIsochDoneQueue for pEP(%p)", this, pEP);
		ReturnIsochDoneQueue(pEP);
		AddIsocFramesToSchedule(pEP);
    }
    
    return kIOReturnSuccess;
	
}



void			
AppleUSBXHCI::AddIsocFramesToSchedule(AppleXHCIIsochEndpoint* pEP)
{
    UInt64										currFrame, startFrame, finFrame;
    IOUSBControllerIsochListElement	*			pTD = NULL;
	AppleXHCIIsochTransferDescriptor *			pXTD = NULL;
    UInt16										nextSlot;
    uint64_t									timeStamp;
    bool                                        ringFullAndEmpty = false;
	UInt64										runningOffset;
	int											i;
	bool										deviceRemoved = false;
	
    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)this, (uintptr_t)pEP, (uintptr_t)pEP->toDoList, 11);
    if (_lostRegisterAccess)
    {
        USBLog(6, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - lost register access", this);
        return;
    }
    
    if (pEP->toDoList == NULL)
    {
		USBLog(7, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - no frames to add fn:%d EP:%d", this, pEP->functionAddress, pEP->endpointNumber);
		return;
    }
	
    if (pEP->aborting)
    {
		USBLog(1, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - EP (%p) is aborting - not adding", this, pEP);
        USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)this, (uintptr_t)pEP, 0, 5);
		return;
    }
	
	if (pEP->waitForRingToRunDry)
	{
		USBLog(2, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - EP (%p) needs to run dry - not adding", this, pEP);
        USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)this, (uintptr_t)pEP, 0, 4);
		return;
	}
	
	// rdar://6693796 Test to see if the pEP is inconsistent at this point. If so, log a message..
	if ((pEP->doneQueue != NULL) && (pEP->doneEnd == NULL))
	{
		USBError(1, "AppleUSBXHCI::AddIsocFramesToSchedule - inconsistent EP queue. pEP[%p] doneQueue[%p] doneEnd[%p] doneQueue->_logicalNext[%p] onDoneQueue[%d] deferredTDs[%d]", pEP, pEP->doneQueue, pEP->doneEnd, pEP->doneQueue->_logicalNext, (int)pEP->onDoneQueue, (int)pEP->deferredTDs);
		IOSleep(1);			// to try to flush the syslog - time is of the essence here, though, but this is an error case
		// the inconsistency should be taken care of inside of the PutTDOnDoneQueue method
	}
	
	USBLog(2, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule pEP(%p)- top TD(%p) frame(%lld) scheduledTDs = %d, deferredTDs = %d", this, pEP, pEP->toDoList, pEP->toDoList->_frameNumber, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->deferredTDs);
	
	// 4211382 - This routine is already non-reentrant, since it runs on the WL.
	// However, we also need to disable preemption while we are in here, since we have to get everything
	// done within a couple of milliseconds, and if we are preempted, we may come back long after that
	// point. So take a SimpleLock to prevent preemption
	
	if (!IOSimpleLockTryLock(_isochScheduleLock))
	{
		// This would indicate reentrancy, which should never ever happen
		USBError(1, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - could not obtain scheduling lock", this);
		return;
	}
	//*******************************************************************************************************
	// ************* WARNING WARNING WARNING ****************************************************************
	// Preemption is now off, which means that we cannot make any calls which may block
	// So don't call USBLog for example
	//*******************************************************************************************************
	
    // Don't get GetFrameNumber() unless we're going to use it
    //
    currFrame = GetMicroFrameNumber();
	// startFrame = currFrame;
    
    USBLogKP(5, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - fn:%d EP:%d inSlot (0x%x), currFrame: 0x%qx", this, pEP->functionAddress, pEP->endpointNumber, pEP->inSlot, currFrame);
	if ((currFrame & 0x07) == 7)
	{
		// if we were in uFrame 7 when we read the value, then we might be in the next frame by now
		// shift out the uFrame bits and add 1
		currFrame = (currFrame >> 3) + 1;
	}
	else
	{
		// otherwise just shift out the uFrame bits
		currFrame = currFrame >> 3;
	}
	timeStamp = mach_absolute_time();
	if (!pEP->continuousStream)
	{
		while (pEP->toDoList->_frameNumber <= (currFrame + _istKeepAwayFrames))		// Add keepaway, and use <= so you never put in a new frame 
                                                                                    // at less than 2 ahead of now. (EHCI spec, 7.2.1)
		{
			IOReturn	ret;
			UInt64		newCurrFrame;
			
			// this transaction is old before it began, move to done queue
			pTD = GetTDfromToDoList(pEP);
			pXTD = (AppleXHCIIsochTransferDescriptor *)pTD;
			
			pXTD->eventTRB.offs0 = 0;
			pXTD->eventTRB.offs4 = 0;
			pXTD->eventTRB.offs8 = 0;
			pXTD->eventTRB.offsC = 0;
			
			ret = pTD->UpdateFrameList(*(AbsoluteTime*)&timeStamp);		// TODO - accumulate the return values
			if (pEP->scheduledTDs > 0)
			{
				USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uintptr_t)pTD, (u_int32_t)(pTD->_frameNumber), 0);
				PutTDonDeferredQueue(pEP, pTD);
			}
			else
			{
				USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uintptr_t)pTD, (u_int32_t)(pTD->_frameNumber), 1);
				PutTDonDoneQueue(pEP, pTD, true);
			}
			
			if (pEP->toDoList == NULL)
			{	
				IOSimpleLockUnlock(_isochScheduleLock);
				// OK to call USBLog, now that preemption is reenabled
				USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)pEP->scheduledTDs, (uint32_t)pEP->deferredTDs, 2);
				bool alreadyQueued = thread_call_enter1(_returnIsochDoneQueueThread, (thread_call_param_t) pEP);
				if ( alreadyQueued )
				{
					USBLog(1, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - thread_call_enter1(_returnIsochDoneQueueThread) was NOT scheduled.  That's not good", this);
					// USBTrace( kUSBTEHCI, kTPEHCIAddIsocFramesToSchedule, (uintptr_t)this, 0, 0, 2 );
				}
				return;
			}
			newCurrFrame = GetFrameNumber();
			if (newCurrFrame == 0)
			{
				_lostRegisterAccess = true;						// we lost access to our registers
				break;
			}
			
            if (newCurrFrame == 0)
            {
                deviceRemoved = true;
            }
            
			if (newCurrFrame > currFrame)
			{
				USBLogKP(1, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - Current frame moved (0x%qx->0x%qx) resetting", this, currFrame, newCurrFrame);
				currFrame = newCurrFrame;
			}		
		}
	}

	if (!_lostRegisterAccess && !deviceRemoved && pEP->toDoList)
	{
		// this code will now grab TDs from the ToDo list and assign them to a list of "slots" which 
		// are indexed by pEP->inSlot and pEP->outslot This routine will only change the value of inSlot
		// and the primary interrupt routine is allowed to change the value of outslot. This shadows the TRBs
		// which are on the Transfer Ring, with one TD representing one or more TRBs. Also, this TD (as opposed
		// to an XHCI TD represents one full frame's worth of transfers
		
		currFrame = pEP->toDoList->_frameNumber;										// start looking at the first available number
		
        if (pEP->inSlot > kNumTDSlots)
        {
            pEP->inSlot = 0;
            
        }
		nextSlot = pEP->inSlot;
        
        if (nextSlot == pEP->outSlot)
        {
            USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)pEP->outSlot, (uint32_t)pEP->inSlot, 9);
        }
		else
        {
            bool                        firstMicroFrame = true;

            do
            {
                UInt32						offsC;
                UInt16						hwFrame;
                UInt32						spaceAvailable;

                USBLogKP(7, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule pEP[%p] pEP->inSlot(%d) pEP->outSlot(%d) pTD@inSlot[%p]\n", this, pEP, (int)pEP->inSlot, (int)pEP->outSlot, pEP->tdSlots[pEP->inSlot]);

                // XHCI has a problem if consecutive TRBs have FrameIDs which are inconsistent with the interval of the endpoint
                // for example, the Audio driver uses an Isoc feedback endpoint which has an interval of 1ms, but they only 
                // schedule a single IN transaction on that endpoint once every 8ms. If we put a second TRB on the ring while
                // the first one is still active on the ring, then the XHCI will not look at the FrameID and will assume that the
                // new TRB belongs in the next interval's frame. To get around that, we wait for the ring to "run dry" which causes
                // the XHCI to remove it from the schedule. Once that happens, and we receive a RingUnderrun or RingOverrun, then 
                // we will call into here again, ringRunning will be false, and we can add the new TD.
                if (!pEP->continuousStream && (pEP->lastScheduledFrame > 0) && pEP->ringRunning && ((pEP->lastScheduledFrame + pEP->msBetweenTDs) < currFrame))
                {
                    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)currFrame, (uint32_t)pEP->lastScheduledFrame, 3);
                    pEP->waitForRingToRunDry = true;
                    break;
                }
                
                if (pEP->inSlot == pEP->outSlot)
                {
                    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)pEP->outSlot, (uint32_t)pEP->inSlot, 8);
                    ringFullAndEmpty = true;
                    break;
                }

                // in case the period of the endpoint is not 1ms, we need to make sure we don't jump over the outSlot
                for (i=0; i < pEP->msBetweenTDs; i++)
                {
                    nextSlot = (pEP->inSlot + 1) & (kNumTDSlots-1);
                    if ( nextSlot == pEP->outSlot) 							// weve caught up with our tail
                        break;                                              // break out of the mini loop    
                }
                
                if ( nextSlot == pEP->outSlot) 								// weve caught up with our tail
                {
                    USBLogKP(2, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - caught up nextSlot (0x%x) pEP->outSlot (0x%x)", this, nextSlot, pEP->outSlot);
                    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)pEP->outSlot, (uint32_t)pEP->inSlot, 9);
                    break;
                }
                
                if (pEP->tdSlots[pEP->inSlot])
                {
                    // this can happen because not every TD generates an interrupt, and so sometimes we can end up a few milliseconds
                    // behind the hardware. If this is not NULL, then we haven't scavenged this TD yet and we need to wait until 
                    // we do
                    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)pEP->inSlot, 0, 10);
                    break;
                }

                spaceAvailable = FreeSlotsOnRing(pEP->ring);
                
                USBLogKP(7, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - spaceAvailable(%d)\n", this, (int)spaceAvailable);
                
                if (pEP->maxTRBs > spaceAvailable)
                {
                    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)pEP->maxTRBs, (uint32_t)spaceAvailable, 6);
                    break;
                }
                
                pTD = GetTDfromToDoList(pEP);
                USBLogKP(7, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - got TD (%p) for frame (%d)", this, pTD, (int)pTD->_frameNumber);
                // pTD->print(2);
                if (pEP->outSlot > kNumTDSlots)
                {
                    pEP->outSlot = 0;								// this is the only time this routine is allowed to change outslot
                    USBLogKP(7, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - changed outSlot for pEP(%p) to (%d)\n", this, pEP, (int)pEP->outSlot);
                }
                pXTD = (AppleXHCIIsochTransferDescriptor*)pTD;
                
                if (pEP->continuousStream)
                    hwFrame = GetFrameNumber() + _istKeepAwayFrames + 10;           // continuous streams will start ASAP
                else
                    hwFrame = (UInt16)pXTD->_frameNumber;
				
                USBLogKP(1, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule pEP(%p) adding pTD(%p) to slot(%d)", this, pEP, pXTD, pEP->inSlot);
                pEP->tdSlots[pEP->inSlot] = pXTD;
				
                currFrame += pEP->msBetweenTDs;
                pEP->inSlot = nextSlot;
                runningOffset = ((AppleXHCIIsochTransferDescriptor*)pTD)->bufferOffset;

                // hand the TRBs over to the hardware
                OSIncrementAtomic(&(pEP->scheduledTDs));
                pEP->lastScheduledFrame = pTD->_frameNumber;
                for (i=0; i < pTD->_framesInTD; i++)
                {
                    IOUSBIsocFrame *			pFrames = pTD->_pFrames;    
                    IOUSBLowLatencyIsocFrame *	pLLFrames = pTD->_lowLatency ? (IOUSBLowLatencyIsocFrame *)pFrames : NULL;
                    IOByteCount					thisReq = pLLFrames ? pLLFrames[pTD->_frameIndex + i].frReqCount : (pFrames ? pFrames[pTD->_frameIndex + i].frReqCount : 0);
                    IOReturn					err;
                    UInt32						TLBPC, TBC;
                    UInt32                      frameID;
                    UInt32                      TDPC;                   // Transfer Descriptor Packet Count - see section 4.14.1 and 4.11.2.3
                    UInt32                      IsochBurstResiduePackets;
                    
                    // set up the initial offset 0x0c to be an Isoc TRB and starting on a new frame if needed
                    offsC = (kXHCITRB_Isoc << kXHCITRB_Type_Shift);
                    
                    if (pEP->continuousStream)
                    {
                        if (firstMicroFrame && !pEP->ringRunning)
                        {
                            frameID = (hwFrame << kXHCITRB_FrameID_Shift) & kXHCITRB_FrameID_Mask;
                            firstMicroFrame = false;
                        }
                        else
                            frameID = kXHCITRB_SIA;
                        
                    }
                    else
                    {
                        if (i == 0)
                            frameID = (hwFrame << kXHCITRB_FrameID_Shift) & kXHCITRB_FrameID_Mask;
                        else
                            frameID = kXHCITRB_SIA;
                   }
                    
                    offsC |= HostToUSBLong(frameID);
                    // Calculate the Trasfer Burst Count and Transfer Last Busrt Packet Count
                    // See 4.11.2.3
                    // note that pEP->maxBurst has already been converted to one-based (i.e. is Max Burst Size + 1)
                    
                    TDPC = ((UInt32)thisReq + pEP->maxPacketSize - 1) / pEP->maxPacketSize;                                     // get the total packet count for this TD (1 based)
                    if (!TDPC)
                        TDPC = 1;                                                                                               // zero length packets count
                    
                    // calculate the Transfer Burst Size: how many total bursts are in this request (0 based answer)
                    TBC = ((TDPC + pEP->maxBurst - 1) / pEP->maxBurst)-1;                                                       // round up to the nearest number of bursts (4.11.2.3)
                    
                    // now calculate how many packets (oneMPS packets) are in the last burst of this request
                    IsochBurstResiduePackets = TDPC % pEP->maxBurst;															// remember that maxBurst is 1 based
                    
                    if (IsochBurstResiduePackets == 0)																			// section 4.11.2.3
                        TLBPC = pEP->maxBurst - 1;
                    else
                        TLBPC = IsochBurstResiduePackets - 1;
                    
                    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)pEP->maxPacketSize, (uint32_t)pEP->maxBurst, 12);
                    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uint32_t)TDPC, (uint32_t)IsochBurstResiduePackets, 13);

                    offsC |= HostToUSBLong( (TBC << kXHCITRB_TBC_Shift) & kXHCITRB_TBC_Mask);
                    offsC |= HostToUSBLong( (TLBPC << kXHCITRB_TLBPC_Shift) & kXHCITRB_TLBPC_Mask);
                    
                    USBLogKP(7, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - calling _createTransfer offsC(%08x) interruptThisTD(%s)\n", this, (int)offsC, pXTD->interruptThisTD ? "true" : "false");
                    pXTD->statusUpdated[i] = false;
                    
                    err = _createTransfer(pXTD, true, thisReq, offsC,  runningOffset, (pXTD->interruptThisTD && (i == (pXTD->_framesInTD -1))), false, &pXTD->trbIndex[i], &pXTD->numTRBs[i], true);
                    if ( err != kIOReturnSuccess)
                    {
                        USBLogKP(1, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - _createTransfer returned 0x%x", this, (uint32_t)err);
                    }
                    runningOffset += thisReq;

                }
                
                
            } while (pEP->toDoList != NULL);
        }
		
		// finFrame = GetFrameNumber();
	}

	// Unlock, reenable preemption, so we can log
	IOSimpleLockUnlock(_isochScheduleLock);

	if (ringFullAndEmpty)
    {
        USBLog(1, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - caught up pEP->inSlot (0x%x) pEP->outSlot (0x%x) - Ring is Full and Empty!", this, pEP->inSlot, pEP->outSlot);

    }
	
	StartEndpoint(GetSlotID(pEP->functionAddress), GetEndpointID(pEP->endpointNumber, pEP->direction));

	// there should not be a race here, because we know that anything we have just placed on the ring is more than 1ms in the future
	// and so we will not have processed a STOPPED, OVERRUN, or UNDERRUN at this point
	if (!pEP->ringRunning)
	{
		USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, 0, 0, 14);
		pEP->ringRunning = true;
	}
	
    USBTrace(kUSBTXHCI, kTPXHCIAddIsochFramesToSchedule, (uintptr_t)pEP, (uintptr_t)pEP->toDoList, (uint32_t)pEP->onDoneQueue, 7);
	USBLog(2, "AppleUSBXHCI[%p]::AddIsocFramesToSchedule - finished,  currFrame: %qx, deferred TDs(%d) onDoneQueue(%d)", this, GetFrameNumber(), (int)pEP->deferredTDs, (int)pEP->onDoneQueue );
}



//	AllocateIsochEP
//	Virtual method which is called from the IOUSBControllerV3 class
IOUSBControllerIsochEndpoint*			
AppleUSBXHCI::AllocateIsochEP()
{
	AppleXHCIIsochEndpoint		*pEP;
	
	pEP = OSTypeAlloc(AppleXHCIIsochEndpoint);
	if (pEP)
	{
		if (!pEP->init())
		{
			pEP->release();
			pEP = NULL;
		}
	}
	return pEP;
}
