/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright © 1998-2009 Apple Inc.  All rights reserved.
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

//================================================================================================
//
//   Includes
//
//================================================================================================
//
#include <IOKit/IOTypes.h>

#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBEHCI.h"
#include "AppleUSBEHCIHubInfo.h"
#include "AppleEHCIListElement.h"
#include "USBTracepoints.h"

//================================================================================================
//
//   External Definitions
//
//================================================================================================
//
extern KernelDebugLevel	    gKernelDebugLevel;

//================================================================================================
//
//   Local Definitions
//
//================================================================================================
//
// Convert USBLog to use kprintf debugging
// The switch is in the header file, but the work is done here because the header is included by the companion controllers
#if EHCI_USE_KPRINTF
	#undef USBLog
	#undef USBError
	void kprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
	#define USBLog( LEVEL, FORMAT, ARGS... )  if ((LEVEL) <= EHCI_USE_KPRINTF) { kprintf( FORMAT "\n", ## ARGS ) ; }
	#define USBError( LEVEL, FORMAT, ARGS... )  { kprintf( FORMAT "\n", ## ARGS ) ; }
#endif


OSDefineMetaClassAndStructors(AppleUSBEHCIHubInfo, OSObject)

AppleUSBEHCIHubInfo*
AppleUSBEHCIHubInfo::AddHubInfo	(AppleUSBEHCIHubInfo **hubListPtr, USBDeviceAddress hubAddr, UInt32 flags)
{
    AppleUSBEHCIHubInfo 	*hiList = *hubListPtr;
    AppleUSBEHCIHubInfo 	*hiPtr = new AppleUSBEHCIHubInfo;
	
	USBLog(5, "AppleUSBEHCIHubInfo::NewHubInfo -  new hiPtr[%p] for hubAddr[%d]", hiPtr, hubAddr);
	if (hiPtr)
	{
		if (flags & kUSBEHCIFlagsMuliTT)
			hiPtr->multiTT = true;
		else
			hiPtr->multiTT = false;

		hiPtr->hubAddr = hubAddr;
		hiPtr->next = hiList;
		hiPtr->ttList = NULL;
		hiList = hiPtr;
		*hubListPtr = hiList;
	}
	return hiPtr;
}	



AppleUSBEHCIHubInfo*
AppleUSBEHCIHubInfo::FindHubInfo(AppleUSBEHCIHubInfo *hiPtr, USBDeviceAddress hubAddr)
{    
    while (hiPtr)
    {
		if (hiPtr->hubAddr == hubAddr)
			break;
		hiPtr = hiPtr->next;
    }
    
    USBLog(5, "AppleUSBEHCIHubInfo::FindHubInfo for hubAddr[%d], returning hiPtr[%p]", hubAddr, hiPtr);
    
    return hiPtr;
}



IOReturn
AppleUSBEHCIHubInfo::DeleteHubInfo(AppleUSBEHCIHubInfo **hubListPtr, USBDeviceAddress hubAddress)
{
    AppleUSBEHCIHubInfo 	*hiList = *hubListPtr;
    AppleUSBEHCIHubInfo 	*hiPtr, *tempPtr;
	AppleUSBEHCITTInfo		*ttiPtr, *tempTTPtr;

	if (!hiList)
		return kIOReturnInternalError;

	// first remove any at the beginning
	if (hiList->hubAddr == hubAddress)
	{
		ttiPtr = hiList->ttList;
		while (ttiPtr)
		{
			tempTTPtr = ttiPtr->next;
			USBLog(5, "AppleUSBEHCIHubInfo::DeleteHubInfo - hiList[%p] releasing ttiPtr[%p]", hiList, ttiPtr);
			ttiPtr->release();
			ttiPtr = tempTTPtr;
		}
		hiPtr = hiList;
		hiList = hiList->next;
		USBLog(5, "AppleUSBEHCIHubInfo::DeleteHubInfo- hiList[%p] releasing hiPtr[%p] from head of list", hiList, hiPtr);
		hiPtr->release();
	}
	else
	{
		hiPtr = hiList;
		while (hiPtr && (hiPtr->next))
		{
			if (hiPtr->next->hubAddr == hubAddress)
			{
				tempPtr = hiPtr->next;
				hiPtr->next = tempPtr->next;
				ttiPtr = tempPtr->ttList;
				while (ttiPtr)
				{
					tempTTPtr = ttiPtr->next;
					USBLog(5, "AppleUSBEHCIHubInfo::DeleteHubInfo - hiPtr[%p] releasing ttiPtr[%p]", tempPtr, ttiPtr);
					ttiPtr->release();
					ttiPtr = tempTTPtr;
				}
				USBLog(5, "AppleUSBEHCIHubInfo::DeleteHubInfo- hiPtr[%p] releasing hiPtr[%p] from middle of list", tempPtr, tempPtr);
				tempPtr->release();
			}
			else
				hiPtr = hiPtr->next;
		}
	}
	*hubListPtr = hiList;								// fix the master pointer
	return kIOReturnSuccess;
}



AppleUSBEHCITTInfo	*
AppleUSBEHCIHubInfo::GetTTInfo(int portAddress)
{
	AppleUSBEHCITTInfo	*ttiPtr = ttList;
	
	// if this is a multiTT hub, then we have to find a ttiPtr with the correct address
	// otherwise, we just go with the ttList (if it already exists)
	if (multiTT)
	{
		while (ttiPtr && (ttiPtr->hubPort != portAddress))
			ttiPtr = ttiPtr->next;
	}
	
	if (!ttiPtr)
	{
		ttiPtr = AppleUSBEHCITTInfo::NewTTInfo(multiTT ? portAddress : 0);
		if (ttiPtr)
		{
			USBLog(5, "AppleUSBEHCIHubInfo[%p]::GetTTInfo - Adding ttiPtr[%p] to ttList", this, ttiPtr);
			ttiPtr->next = ttList;
			ttList = ttiPtr;
		}
	}
	
	return ttiPtr;
}


#pragma mark AppleUSBEHCITTInfo
OSDefineMetaClassAndStructors(AppleUSBEHCITTInfo, OSObject)


// CompareSPEs - used when creating an ordered set of SPEs to move when adding or subtracting
// A comparison result of the object:
//		a positive value if obj2 should precede obj1,</li>
//		a negative value if obj1 should precede obj2,</li>
//		and 0 if obj1 and obj2 have an equivalent ordering.</li>
static SInt32
CompareSPEs(const AppleUSBEHCISplitPeriodicEndpoint *pSPE1, const AppleUSBEHCISplitPeriodicEndpoint *pSPE2, const AppleUSBEHCITTInfo *forTT)
{
	USBLog(5, "AppleUSBEHCITTInfo[%p]::CompareSPEs - pSPE1[%p] pSPE2[%p]", forTT, pSPE1, pSPE2);
	
	if (!pSPE1 || !pSPE2)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::CompareSPEs - one or more objects NULL - returning 0", forTT);
		return 0;
	}
	
	((AppleUSBEHCISplitPeriodicEndpoint *)pSPE1)->print(5);
	((AppleUSBEHCISplitPeriodicEndpoint *)pSPE2)->print(5);
	
	// first get the two obvious cases out of the way
	if ((pSPE1->_epType == kUSBIsoc) && (pSPE2->_epType == kUSBInterrupt))
	{
		return 1;
	}
	
	if ((pSPE1->_epType == kUSBInterrupt) && (pSPE2->_epType == kUSBIsoc))
	{
		return -1;
	}
	
	// so both are either interrupt or isoc. sort based on startTime
	if (pSPE2->_startTime > pSPE1->_startTime)
	{
		return 1;
	}
	if (pSPE1->_startTime > pSPE2->_startTime)
	{
		return -1;
	}
	
	USBLog(5, "AppleUSBEHCITTInfo[%p]::CompareSPEs - EQUAL", forTT);
	return 0;
}



AppleUSBEHCITTInfo	*
AppleUSBEHCITTInfo::NewTTInfo(int portAddress)
{
	AppleUSBEHCITTInfo					*ttiPtr = new AppleUSBEHCITTInfo;
	AppleUSBEHCISplitPeriodicEndpoint	*pSPE;
	IOReturn							err = kIOReturnSuccess;
	int									i;
	
	if (ttiPtr)
	{
		ttiPtr->next = NULL;
		ttiPtr->hubPort = portAddress;
		ttiPtr->_thinkTime = kEHCISplitTTThinkTime;
		// I need to put a dummy SplitPeriodicEndpoint in each Interrupt and each Isoch queue to account for the SOF and the hub slop time 
		// this also makes the algorithms easier since there is always an endpoint in each queue
		// first make sure all is NULL so we can clean up if needed
		for (i=0; i < kEHCIMaxPollingInterval; i++)
		{
			ttiPtr->_interruptQueue[i] = ttiPtr->_isochQueue[i] = NULL;
		}
		for (i=0; i < kEHCIMaxPollingInterval; i++)
		{
			// the following will instantiate an AppleUSBEHCISplitPeriodicEndpoint, which will hold a reference back to the new ttiPtr
			pSPE = AppleUSBEHCISplitPeriodicEndpoint::NewSplitPeriodicEndpoint(ttiPtr, kUSBInterrupt, NULL, kEHCIFSSOFBytesUsed + kEHCIFSHubAdjustBytes, kEHCIMaxPollingInterval);
			if (!pSPE)
			{
				USBLog(1, "AppleUSBEHCITTInfo::NewTTInfo - unable to get interrupt SPE for index(%d)", i);
				err = kIOReturnInternalError;
				break;
			}
			pSPE->SetStartFrameAndStartTime(i, 0);
			ttiPtr->_interruptQueue[i] = pSPE;
			
			// the following will instantiate an AppleUSBEHCISplitPeriodicEndpoint, which will hold a reference back to the new ttiPtr
			pSPE = AppleUSBEHCISplitPeriodicEndpoint::NewSplitPeriodicEndpoint(ttiPtr, kUSBIsoc, NULL, kEHCIFSSOFBytesUsed + kEHCIFSHubAdjustBytes, kEHCIMaxPollingInterval);
			if (!pSPE)
			{
				USBLog(1, "AppleUSBEHCITTInfo::NewTTInfo - unable to get isoch SPE for index(%d)", i);
				err = kIOReturnInternalError;
				break;
			}
			pSPE->SetStartFrameAndStartTime(i, 0);
			ttiPtr->_isochQueue[i] = pSPE;
			ttiPtr->_FStimeUsed[i] = kEHCIFSSOFBytesUsed + kEHCIFSHubAdjustBytes;
		}
		ttiPtr->_pSPEsToAdjust = OSOrderedSet::withCapacity(kAppleEHCITTInfoInitialOrderedSetSize, (OSOrderedSet::OSOrderFunction)CompareSPEs, ttiPtr);
	}
	
	if (err != kIOReturnSuccess)
	{
		for (i=0; i < kEHCIMaxPollingInterval; i++)
		{
			pSPE = ttiPtr->_interruptQueue[i];
			if (pSPE)
				pSPE->release();
			pSPE = ttiPtr->_isochQueue[i];
			if (pSPE)
				pSPE->release();
		}
		if (ttiPtr->_pSPEsToAdjust)
			ttiPtr->_pSPEsToAdjust->release();
		ttiPtr->release();
		ttiPtr = NULL;
	}
	
	return ttiPtr;
}



IOReturn
AppleUSBEHCITTInfo::ReserveHSSplitINBytes(int frame, int uFrame, UInt16 bytesToReserve)
{
	
	// validate each of the params
	if (!bytesToReserve)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReserveHSSplitINBytes - 0 bytesToReserve - nothing to do", this);
		return kIOReturnSuccess;
	}
	
	if (bytesToReserve > kEHCIFSBytesPeruFrame)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReserveHSSplitINBytes - ERROR: bytesToReserve(%d) too big", this, (int)bytesToReserve);
		return kIOReturnBadArgument;
	}
	
	if ((frame < 0) || (frame > kEHCIMaxPollingInterval))
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReserveHSSplitINBytes - ERROR: invalid frame(%d)", this, frame);
		return kIOReturnBadArgument;
	}
	if ((uFrame < 0) || (uFrame > kEHCIuFramesPerFrame))
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReserveHSSplitINBytes - ERROR: invalid uFrame(%d)", this, uFrame);
		return kIOReturnBadArgument;
	}
	
	// it is OK to go over the limit on the aggregate..
	
	_HSSplitINBytesUsed[frame][uFrame] += bytesToReserve;

	USBLog(7, "AppleUSBEHCITTInfo[%p]::ReserveHSSplitINBytes - frame[%d] uFrame[%d] reserved _HSSplitINBytesUsed(%d)", this, frame, uFrame, _HSSplitINBytesUsed[frame][uFrame]);
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCITTInfo::ReleaseHSSplitINBytes(int frame, int uFrame, UInt16 bytesToRelease)
{
	// validate each of the params
	if (!bytesToRelease)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseHSSplitINBytes - 0 bytesToRelease - nothing to do", this);
		return kIOReturnSuccess;
	}
	
	if (bytesToRelease > kEHCIFSBytesPeruFrame)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseHSSplitINBytes - ERROR: bytesToRelease(%d) too big", this, (int)bytesToRelease);
		return kIOReturnBadArgument;
	}
	
	if ((frame < 0) || (frame > kEHCIMaxPollingInterval))
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseHSSplitINBytes - ERROR: invalid frame(%d)", this, frame);
		return kIOReturnBadArgument;
	}
	if ((uFrame < 0) || (uFrame > kEHCIuFramesPerFrame))
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseHSSplitINBytes - ERROR: invalid uFrame(%d)", this, uFrame);
		return kIOReturnBadArgument;
	}
	
	if (bytesToRelease > _HSSplitINBytesUsed[frame][uFrame])
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseHSSplitINBytes - frame(%d) uFrame(%d) - trying to release more bytes(%d) than reserved(%d)", this, frame,  uFrame, bytesToRelease, _HSSplitINBytesUsed[frame][uFrame]);
		_HSSplitINBytesUsed[frame][uFrame] = 0;
	}
	else
	{
		_HSSplitINBytesUsed[frame][uFrame] -= bytesToRelease;
	}
	
	USBLog(7, "AppleUSBEHCITTInfo[%p]::ReleaseHSSplitINBytes - frame[%d] uFrame[%d] reserved _HSSplitINBytesUsed(%d)", this, frame, uFrame, _HSSplitINBytesUsed[frame][uFrame]);
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCITTInfo::ReserveFSBusBytes(int frame, UInt16 bytesToReserve)
{
	
	// validate each of the params
	if (!bytesToReserve)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReserveFSBusBytes - 0 bytesToReserve - nothing to do", this);
		return kIOReturnSuccess;
	}
	
	if (bytesToReserve > kEHCIFSMaxFrameBytes)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReserveFSBusBytes - ERROR: bytesToReserve(%d) too big", this, (int)bytesToReserve);
		return kIOReturnBadArgument;
	}
	
	if ((frame < 0) || (frame > kEHCIMaxPollingInterval))
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReserveFSBusBytes - ERROR: invalid frame(%d)", this, frame);
		return kIOReturnBadArgument;
	}
	
	
	_FStimeUsed[frame] += bytesToReserve;
	if (_FStimeUsed[frame] > kEHCIFSMaxFrameBytes)
	{
		USBLog(5, "AppleUSBEHCITTInfo[%p]::ReserveFSBusBytes - frame[%d] reserved space(%d) over the limit - could be OK", this, frame, _FStimeUsed[frame]);
		return kIOReturnNoBandwidth;
	}
	
	USBLog(7, "AppleUSBEHCITTInfo[%p]::ReserveFSBusBytes - frame[%d] reserved _FStimeUsed(%d)", this, frame, _FStimeUsed[frame]);
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCITTInfo::ReleaseFSBusBytes(int frame, UInt16 bytesToRelease)
{
	
	// validate each of the params
	if (!bytesToRelease)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseFSBusBytes - 0 bytesToRelease - nothing to do", this);
		return kIOReturnSuccess;
	}
	
	if (bytesToRelease > kEHCIFSMaxFrameBytes)
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseFSBusBytes - ERROR: bytesToRelease(%d) too big", this, (int)bytesToRelease);
		return kIOReturnBadArgument;
	}
	
	if ((frame < 0) || (frame > kEHCIMaxPollingInterval))
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseFSBusBytes - ERROR: invalid frame(%d)", this, frame);
		return kIOReturnBadArgument;
	}
	

	if (bytesToRelease > _FStimeUsed[frame])
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::ReleaseFSBusBytes - frame(%d) - trying to release more bytes(%d) than reserved(%d)", this, frame,  bytesToRelease, _FStimeUsed[frame]);
		_FStimeUsed[frame] = 0;
	}
	else
	{
		_FStimeUsed[frame] -= bytesToRelease;
	}
	
	USBLog(7, "AppleUSBEHCITTInfo[%p]::ReleaseFSBusBytes - frame[%d] reserved _FStimeUsed(%d)", this, frame, _FStimeUsed[frame]);
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCITTInfo::AllocatePeriodicBandwidth(AppleUSBEHCISplitPeriodicEndpoint *pSPE)
{
	IOReturn								err;
	int										frameIndex;
	AppleUSBEHCISplitPeriodicEndpoint		*curSPE;
	AppleUSBEHCISplitPeriodicEndpoint		*prevSPE;
	AppleUSBEHCISplitPeriodicEndpoint		*tempSPE;
	
	USBLog(3, "AppleUSBEHCITTInfo[%p]::AllocatePeriodicBandwidth: pSPE[%p]", this, pSPE);
	ShowPeriodicBandwidthUsed(5, "+AllocatePeriodicBandwidth");
	if (!pSPE)
		return kIOReturnInternalError;
	
	err = pSPE->FindStartFrameAndStartTime();
	if (err)
	{
		USBLog(3, "AppleUSBEHCITTInfo[%p]::AllocatePeriodicBandwidth - pSPE->FindStartFrameAndStartTime returned err[%p]", this, (void*)err);
		return err;
	}
	
	for (frameIndex=pSPE->_startFrame; frameIndex < kEHCIMaxPollingInterval; frameIndex += pSPE->_period)
	{
		if (pSPE->_epType == kUSBIsoc)
		{
			prevSPE = _isochQueue[frameIndex];				// there is always at least a dummy
			if (!prevSPE)
			{
				USBLog(1, "AppleUSBEHCITTInfo[%p]::AllocatePeriodicBandwidth - invalid isoch queue head at frame (%d)", this, (int)frameIndex);
				return kIOReturnInternalError;
			}
			curSPE = prevSPE->_nextSPE;
			tempSPE = curSPE;
			
			while(curSPE)
			{
				if (pSPE->CheckPlacementBefore(curSPE) == kIOReturnSuccess)
					break;
				prevSPE = curSPE;
				curSPE = curSPE->_nextSPE;
			}
			if  (curSPE != pSPE)
			{
				if (pSPE->_FSBytesUsed > kEHCIFSLargeIsochPacket)
					_largeIsoch[frameIndex] = pSPE;
				else
				{
					USBLog(5, "AppleUSBEHCITTInfo[%p]::AllocatePeriodicBandwidth - inserting pSPE(%p) into frame (%d) between (%p) and (%p)", this, pSPE, frameIndex, prevSPE, curSPE);
					if (frameIndex == pSPE->_startFrame)
						pSPE->_nextSPE = curSPE;								// only do this for the primary harmonic
					
					// check to make sure that we are not already in the isoch list
					while (tempSPE)
					{
						if (tempSPE == pSPE)
							break;
						tempSPE = tempSPE->_nextSPE;
						
					}
					if (tempSPE != pSPE)
						prevSPE->_nextSPE = pSPE;
				}

			}
		}
		else
		{
			prevSPE = _interruptQueue[frameIndex];				// there is always at least a dummy
			if (!prevSPE)
			{
				USBLog(1, "AppleUSBEHCITTInfo[%p]::AllocatePeriodicBandwidth - invalid interrupt queue head at frame (%d)", this, (int)frameIndex);
				return kIOReturnInternalError;
			}
			curSPE = prevSPE->_nextSPE;
			while(curSPE)
			{
				if (pSPE->CheckPlacementBefore(curSPE) == kIOReturnSuccess)
					break;
				prevSPE = curSPE;
				curSPE = curSPE->_nextSPE;
			}
			if (curSPE != pSPE)
			{
				USBLog(7, "AppleUSBEHCITTInfo[%p]::AllocatePeriodicBandwidth - inserting pSPE(%p) into frame (%d) between (%p) and (%p)", this, pSPE, frameIndex, prevSPE, curSPE);
				if (frameIndex == pSPE->_startFrame)
					pSPE->_nextSPE = curSPE;								// only do this for the primary harmonic
				prevSPE->_nextSPE = pSPE;
			}
		}
		ReserveFSBusBytes(frameIndex, pSPE->_FSBytesUsed);
	}
	
	// TODO - Adjust all of the starting times as needed..
	
	ShowPeriodicBandwidthUsed(5, "-AllocatePeriodicBandwidth");
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCITTInfo::DeallocatePeriodicBandwidth(AppleUSBEHCISplitPeriodicEndpoint *pSPE)
{
	IOReturn								err;
	int										frameIndex;
	AppleUSBEHCISplitPeriodicEndpoint		*curSPE;
	AppleUSBEHCISplitPeriodicEndpoint		*prevSPE;
	AppleUSBEHCISplitPeriodicEndpoint		*tempSPE;
	
	USBLog(3, "AppleUSBEHCITTInfo[%p]::DeallocatePeriodicBandwidth: pSPE[%p]", this, pSPE);
	if (!pSPE)
		return kIOReturnInternalError;
	
	for (frameIndex=pSPE->_startFrame; frameIndex < kEHCIMaxPollingInterval; frameIndex += pSPE->_period)
	{
		if (_largeIsoch[frameIndex] == pSPE)
		{
			USBLog(5, "AppleUSBEHCITTInfo[%p]::DeallocatePeriodicBandwidth - removing large Isoch xaction (%p) from frame (%d)", this, pSPE, (int)frameIndex);
			_largeIsoch[frameIndex] = NULL;
		}
		else
		{
			if (pSPE->_epType == kUSBIsoc)
			{
				prevSPE = _isochQueue[frameIndex];					// there is always at least a dummy
			}
			else
			{
				prevSPE = _interruptQueue[frameIndex];				// there is always at least a dummy
			}

			if (!prevSPE)
			{
				USBLog(1, "AppleUSBEHCITTInfo[%p]::DeallocatePeriodicBandwidth - invalid isoch queue head at frame (%d)", this, (int)frameIndex);
				return kIOReturnInternalError;
			}
			curSPE = prevSPE->_nextSPE;
			tempSPE = curSPE;
			
			while(curSPE && (curSPE != pSPE))
			{
				prevSPE = curSPE;
				curSPE = curSPE->_nextSPE;
			}
			if  (curSPE == pSPE)
			{
				USBLog(5, "AppleUSBEHCITTInfo[%p]::DeallocatePeriodicBandwidth - removing pSPE(%p) from frame (%d) between (%p) and (%p)", this, pSPE, frameIndex, prevSPE, curSPE->_nextSPE);
				prevSPE->_nextSPE = curSPE->_nextSPE;
			}
			else
			{
				USBLog(5, "AppleUSBEHCITTInfo[%p]::DeallocatePeriodicBandwidth - could not find pSPE(%p) in frame(%d) often this is fine", this, pSPE, frameIndex);
			}			
		}
		
		// but adjust the time for every harmonic
		ReleaseFSBusBytes(frameIndex, pSPE->_FSBytesUsed);
	}
	
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCITTInfo::CalculateSPEsToAdjustAfterChange(AppleUSBEHCISplitPeriodicEndpoint *pSPEChanged, bool added)
{
	int										index;
	AppleUSBEHCISplitPeriodicEndpoint		*pSPE;
	
	USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - pSPEChanged(%p) %s", this, pSPEChanged, added ? "ADDED" : "REMOVED");
	if (_pSPEsToAdjust->getCount())
	{
		USBLog(1, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - ordered set already exists. error", this);
		return kIOReturnInternalError;
	}
	if (pSPEChanged->_FSBytesUsed >= kEHCIFSLargeIsochPacket)
	{
		USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - %s packet is a large one. everyone needs to move", this, added ? "new" : "old");
		for (index = 0; index < kEHCIMaxPollingInterval; index++)
		{
			USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - looking at Isoch queue index(%d)[%p]", this, index, _isochQueue[index]);
			pSPE = _isochQueue[index]->_nextSPE;						// skip past the dummy
			while (pSPE)
			{
				if (!_pSPEsToAdjust->containsObject(pSPE))
				{
					USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) being added to adjustment set", this, index, pSPE);
					_pSPEsToAdjust->setObject(pSPE);
				}
				else
				{
					USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) already in adjust set", this, index, pSPE);
				}
				pSPE = pSPE->_nextSPE;
			}
			USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - looking at interrupt queue index(%d)[%p]", this, index, _interruptQueue[index]);
			pSPE = _interruptQueue[index]->_nextSPE;					// skip past the dummy
			while (pSPE)
			{
				if (!_pSPEsToAdjust->containsObject(pSPE))
				{
					USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) being added to adjustment set", this, index, pSPE);
					_pSPEsToAdjust->setObject(pSPE);
				}
				else
				{
					USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) already in adjust set", this, index, pSPE);
				}
				pSPE = pSPE->_nextSPE;
			}
		}
	}
	else if (pSPEChanged->_epType == kUSBIsoc)
	{
		USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - %s packet is a ISOCH. some Isoch and all Int will move", this, added ? "new" : "old");
		for (index = 0; index < kEHCIMaxPollingInterval; index++)
		{
			USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - looking at Isoch queue index(%d)[%p]", this, index, _isochQueue[index]);
			pSPE = _isochQueue[index]->_nextSPE;						// skip past the dummy
			if (!added)
			{
				// do this when an endpoint has been removed
				while (pSPE)
				{
					if (pSPE != pSPEChanged)
					{
						if (pSPE->_startTime >= pSPEChanged->_startTime)
						{
							if (!_pSPEsToAdjust->containsObject(pSPE))
							{
								USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) being added to adjustment set", this, index, pSPE);
								_pSPEsToAdjust->setObject(pSPE);
							}
						}
						else
						{
							USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) not a candidate because of a lower start time", this, index, pSPE);
						}
					}
					pSPE = pSPE->_nextSPE;
				}
			}
			else
			{
				// do this when an endpoint has been added
				while (pSPE && (pSPEChanged->CheckPlacementBefore(pSPE) == kIOReturnSuccess))
				{
					pSPE = pSPE->_nextSPE;
				}
				while (pSPE)
				{
					if (!_pSPEsToAdjust->containsObject(pSPE))
					{
						USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) being added to adjustment set", this, index, pSPE);
						_pSPEsToAdjust->setObject(pSPE);
					}
					pSPE = pSPE->_nextSPE;
				}
			}
			USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - looking at interrupt queue index(%d)[%p]", this, index, _interruptQueue[index]);
			pSPE = _interruptQueue[index]->_nextSPE;					// skip past the dummy
			while (pSPE)
			{
				if (!_pSPEsToAdjust->containsObject(pSPE))
				{
					USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) being added to adjustment set", this, index, pSPE);
					_pSPEsToAdjust->setObject(pSPE);
				}
				USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - looking at interrupt queue _nextSPE[%p]", this, pSPE->_nextSPE);
				pSPE = pSPE->_nextSPE;
			}
		}
	}
	else
	{
		USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - %s packet is INTERRUPT - some  Int will move", this, added ? "new" : "old");
		for (index = 0; index < kEHCIMaxPollingInterval; index++)
		{
			pSPE = _interruptQueue[index]->_nextSPE;					// skip past the dummy
			if (!added)
			{
				// do this when an endpoint has been removed
				while (pSPE)
				{
					if (pSPE != pSPEChanged)
					{
						if (pSPE->_startTime >= pSPEChanged->_startTime)
						{
							if (!_pSPEsToAdjust->containsObject(pSPE))
							{
								USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) being added to adjustment set", this, index, pSPE);
								_pSPEsToAdjust->setObject(pSPE);
							}
						}
						else
						{
							USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) not a candidate because of a lower start time", this, index, pSPE);
						}
					}
					pSPE = pSPE->_nextSPE;
				}
			}
			else 
			{
				// do this when an endpoint has been added (this is the opposite of Isoc)
				while (pSPE && (pSPEChanged->CheckPlacementBefore(pSPE) != kIOReturnSuccess))
				{
					pSPE = pSPE->_nextSPE;
				}
				while (pSPE)
				{
					if (pSPE != pSPEChanged)
					{
						if (!_pSPEsToAdjust->containsObject(pSPE))
						{
							USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - index[%d] pSPE(%p) being added to adjustment set", this, index, pSPE);
							_pSPEsToAdjust->setObject(pSPE);
						}
					}
					pSPE = pSPE->_nextSPE;
				}
			}
		}
	}
	USBLog(5, "AppleUSBEHCITTInfo[%p]::CalculateSPEsToAdjustAfterChange - %d candidates to consider", this, _pSPEsToAdjust->getCount());
	return kIOReturnSuccess;
}



void 
AppleUSBEHCITTInfo::release() const
{
	int			i;
	int			oldCount = getRetainCount();
	
	// We get the "old count" before we do this release, because I am not sure whether or not it is OK to call getRetainCount once the count
	// gets to 0. However, we use the "old count" AFTER we call into the superclass to see if the call is the "last call" before the 64 dummy
	// SPEs which we got when we were instantiated.
	
	// The reason that we don't release the 64 SPEs BEFORE we call into the superclass, on say the 65th call, is that a call to SPE->release()
	// well end up calling back into this method, since the SPE will have a retain on us. Thus we will recurse, and if we recurse at the magic 65 
	// number, we will never get lower than that number.
	
	OSObject::release();
	
	// When the AppleUSBEHCITTInfo was instantiated (in NewTTInfo) we instantiated dummy SPEs for the interruptQueue and the isochQueue
	// which are in two arrays of kEHCIMaxPollingInterval each. Now that we have been released to that level, we need to release
	// each of those SPEs, which will in turn release me, so that when we are completely done with the loop below, my count will be 0.
	
	if ((oldCount-1) == (2 * kEHCIMaxPollingInterval))
	{
		USBLog(5, "AppleUSBEHCITTInfo[%p]::release - retainCount is now %d", this, getRetainCount());
		if (_pSPEsToAdjust->getCount() > 0)
		{
			USBLog(1, "AppleUSBEHCITTInfo[%p]::release - _pSPEsToAdjust has a count of %d", this, _pSPEsToAdjust->getCount());
		}
		
		_pSPEsToAdjust->release();
		for (i=0; i < kEHCIMaxPollingInterval; i++)
		{
			if (_interruptQueue[i])
				_interruptQueue[i]->release();
			if (_isochQueue[i])
				_isochQueue[i]->release();
		}
	}
}



#pragma mark Debugging methods
IOReturn
AppleUSBEHCITTInfo::ShowPeriodicBandwidthUsed(int level, const char *fromStr)
{
	int		frame;
	
	USBLog(level, "AppleUSBEHCITTInfo[%p]::ShowPeriodicBandwidthUsed called from %s", this, fromStr);

	for (frame = 0; frame < kEHCIMaxPollingInterval; frame++)
		USBLog(level, "AppleUSBEHCITTInfo[%p]::ShowPeriodicBandwidthUsed - Frame %2.2d: [%3.3d]", this, frame, _FStimeUsed[frame]);
	
	return kIOReturnSuccess;
	
}



IOReturn
AppleUSBEHCITTInfo::ShowHSSplitTimeUsed(int level, const char *fromStr)
{
	int		frame;
	
	USBLog(level, "AppleUSBEHCITTInfo[%p]::ShowHSSplitTimeUsed called from %s", this, fromStr);
	for (frame = 0; frame < kEHCIMaxPollingInterval; frame++)
		USBLog(level, "AppleUSBEHCITTInfo[%p]::ShowHSSplitTimeUsed - Frame %2.2d: [%3.3d] [%3.3d] [%3.3d] [%3.3d] [%3.3d] [%3.3d] [%3.3d] [%3.3d]",
			   this, frame,
			   _HSSplitINBytesUsed[frame][0],
			   _HSSplitINBytesUsed[frame][1],
			   _HSSplitINBytesUsed[frame][2],
			   _HSSplitINBytesUsed[frame][3],
			   _HSSplitINBytesUsed[frame][4],
			   _HSSplitINBytesUsed[frame][5],
			   _HSSplitINBytesUsed[frame][6],
			   _HSSplitINBytesUsed[frame][7]);
	return kIOReturnSuccess;
}


void
AppleUSBEHCITTInfo::print(int level, const char *fromStr)
{
	int		i;
	
	USBLog(level, "AppleUSBEHCITTInfo[%p]::print called from %s", this, fromStr);
	USBLog(level, "AppleUSBEHCITTInfo[%p]::print - next(%p)", this, next);
	for (i=0; i < kEHCIMaxPollingInterval; i++)
	{
		if (_largeIsoch[i])
		{
			USBLog(level, "AppleUSBEHCITTInfo[%p]::print - _largeIsoch[%d](%p)", this, i, _largeIsoch[i]);
			_largeIsoch[i]->print(level);
		}
	}
	USBLog(level, "---------------------------------------------------------");
	for (i=0; i < kEHCIMaxPollingInterval; i++)
	{
		AppleUSBEHCISplitPeriodicEndpoint	*curSPE = _isochQueue[i];
		if (curSPE && (curSPE->_nextSPE))
		{
			USBLog(level, "AppleUSBEHCITTInfo[%p]::print - _isochQueue[%d](%p)", this, i, curSPE);
			while (curSPE)
			{
				curSPE->print(level);
				curSPE=curSPE->_nextSPE;
			}
		}
	}
	USBLog(level, "---------------------------------------------------------");
	for (i=0; i < kEHCIMaxPollingInterval; i++)
	{
		AppleUSBEHCISplitPeriodicEndpoint	*curSPE = _interruptQueue[i];
		if (curSPE && (curSPE->_nextSPE))
		{
			USBLog(level, "AppleUSBEHCITTInfo[%p]::print - _interruptQueue[%d](%p)", this, i, curSPE);
			while (curSPE)
			{
				curSPE->print(level);
				curSPE=curSPE->_nextSPE;
			}
		}
	}
}

#pragma mark AppleUSBEHCISplitPeriodicEndpoint 

OSDefineMetaClassAndStructors(AppleUSBEHCISplitPeriodicEndpoint, OSObject)
AppleUSBEHCISplitPeriodicEndpoint*
AppleUSBEHCISplitPeriodicEndpoint::NewSplitPeriodicEndpoint(	AppleUSBEHCITTInfo *whichTT,
																UInt16 epType, OSObject *pEP,
																UInt16 FSBytesUsed, UInt8 period)
{
	AppleUSBEHCISplitPeriodicEndpoint	*pSPE = new AppleUSBEHCISplitPeriodicEndpoint;
	
	if (pSPE)
	{
		USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::NewSplitPeriodicEndpoint - retaining TT (%p)", pSPE, whichTT);
		
		// The SPE will hold a reference to the TT, which will need to be released when the SPE has been released to 0
		whichTT->retain();											// make sure we have a reference to this TT
		pSPE->_myTT = whichTT;
		pSPE->_epType = epType;
		pSPE->_FSBytesUsed = FSBytesUsed;
		pSPE->_period = period;
		
		if (epType == kUSBInterrupt)
		{
			pSPE->_intEP = OSDynamicCast(AppleEHCIQueueHead, pEP);
			if (pSPE->_intEP)
				pSPE->_direction = pSPE->_intEP->_direction;
			pSPE->_isochEP = NULL;
		}
		else if (epType == kUSBIsoc)
		{
			pSPE->_isochEP = OSDynamicCast(AppleEHCIIsochEndpoint, pEP);
			if (pSPE->_isochEP)
				pSPE->_direction = pSPE->_isochEP->direction;
			pSPE->_intEP = NULL;
		}
		else
		{
			USBLog(1, "AppleUSBEHCISplitPeriodicEndpoint::NewSplitPeriodicEndpoint - invalid epType(%d)", (int)epType);
			whichTT->release();
			pSPE->release();
			return NULL;
		}
		
		// default values
		pSPE->_nextSPE = NULL;
		pSPE->_startFrame = 0;
		pSPE->_startTime = 0;
		pSPE->_numSS = 0;
		pSPE->_numCS = 0;
		pSPE->_SSflags = 0;
		pSPE->_CSflags = 0;
		pSPE->_wraparound = false;
	}
	
	return pSPE;
	
}



void 
AppleUSBEHCISplitPeriodicEndpoint::release() const
{
	// if we are about to go to a retain count of 0, then we are done, and we need to release the TT to which we are tied.
	if (getRetainCount() == 1)
	{
		USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::release - down to the last one. releasing TT[%p]", this, _myTT);
		_myTT->release();
	}
	OSObject::release();
}



void
AppleUSBEHCISplitPeriodicEndpoint::print(int level)
{
	if (gKernelDebugLevel >= (KernelDebugLevel)level)
	{
		if (_intEP || _isochEP || _startTime)
		{
			int			functionNum = 0;
			int			endpointNum = 0;
			void		*pEP = NULL;
			
			if (_epType == kUSBInterrupt)
			{
				pEP = _intEP;
				if (_intEP)
				{
					functionNum = _intEP->_functionNumber;
					endpointNum = _intEP->_endpointNumber;
				}
			}
			else if (_epType == kUSBIsoc)
			{
				pEP = _isochEP;
				if (_isochEP)
				{
					functionNum = _isochEP->functionAddress;
					endpointNum = _isochEP->endpointNumber;
				}
			}
			
	#if __LP64__
			USBLog(level, "EHCISPE[%p]::print - _nextSPE[0x%8.8qx] EP(%d:%d)[%p] _myTT[%p] _epType[%d](%s) _startTime[%3.3d] _FSBytesUsed[%3.3d] _period[%2.2d] _startFrame[%d] _numSS[%d] _numCS[%d] _SSflags[%p] _CSflags[%p] _wraparound[%s]", this, (uint64_t)_nextSPE, functionNum, endpointNum, pEP, _myTT, (int)_epType, _epType == kUSBIsoc ? "Isoc" : "Interrupt", (int)_startTime, (int)_FSBytesUsed, (int)_period, (int)_startFrame, (int)_numSS, (int)_numCS, (void*)_SSflags, (void*)_CSflags, _wraparound ? "true" : "false");
	#else
			USBLog(level, "EHCISPE[%p]::print - _nextSPE[0x%8.8x] EP(%d:%d)[%p] _myTT[%p] _epType[%d](%s) _startTime[%3.3d] _FSBytesUsed[%3.3d] _period[%2.2d] _startFrame[%d] _numSS[%d] _numCS[%d] _SSflags[%p] _CSflags[%p] _wraparound[%s]", this, (uint32_t)_nextSPE, functionNum, endpointNum, pEP,  _myTT, (int)_epType, _epType == kUSBIsoc ? "Isoc" : "Interrupt", (int)_startTime, (int)_FSBytesUsed, (int)_period, (int)_startFrame, (int)_numSS, (int)_numCS, (void*)_SSflags, (void*)_CSflags, _wraparound ? "true" : "false");
	#endif
				IOSleep(1);
		}
	}
}



IOReturn
AppleUSBEHCISplitPeriodicEndpoint::SetStartFrameAndStartTime(UInt8 startFrame, UInt16 startTime)
{
	_startFrame = startFrame;
	_startTime = startTime;
	
	return kIOReturnSuccess;
}




//
// FindStartFrameAndStartTime
// This method is common to both Interrupt and Isoch endpoints
// It determines the start_frame (in our 32 frame overall scedule) and start_time (measured in FS bytes) this transaction EP will live
//
IOReturn
AppleUSBEHCISplitPeriodicEndpoint::FindStartFrameAndStartTime(void)
{
	UInt16			tempStartTimes[kEHCIMaxPollingInterval];
	int				frameIndex, bestFrame;
	UInt16			bestStartTimeFound = kEHCIFSMinStartTime;
	UInt16			bestTimeUsedFound = kEHCIFSMinStartTime;
	UInt16			bestStartTimeFrame = kEHCIMaxPollingInterval;
	UInt16			bestTimeUsedFrame = kEHCIMaxPollingInterval;
	UInt32			timeUsedForBestStartTimeFrame = 0;
	UInt32			timeUsedForBestTimeUsedFrame = 0;
	bool			foundStartTimeCandidate = false;
	bool			foundTimeUsedCandidate = false;
	
	USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::FindStartFrameAndStartTime - _FSBytesUsed (%d)", this, _FSBytesUsed);
	// first, calculate where I would fit into each frame based on where I would be inserted in that frame
	CalculateAllFrameStartTimes(tempStartTimes);
	
	for (frameIndex=0; frameIndex < _period; frameIndex++)
	{
		UInt16			totalTimeUsedThisHarmonic = 0;								// the total of all time used for all EPs in this harmonic
		UInt16			startTimeThisFrame = kEHCIFSMinStartTime;
		int				frameIndex2;
		bool			epWillFit = true;

		// check all of the frames in the overall array which would be on the same harmonic as the earliest one
		for (frameIndex2 = frameIndex; frameIndex2 < kEHCIMaxPollingInterval; frameIndex2 += _period)
		{
			// see if I will fit in this harmonic based on total time used
			UInt16		startTimeThisHarmonic;
			UInt16		tempTimeUsed = _myTT->_FStimeUsed[frameIndex2] + _FSBytesUsed;

			if ((_FSBytesUsed > kEHCIFSLargeIsochPacket) && (_myTT->_largeIsoch[frameIndex]))
			{
				USBLog(1, "AppleUSBEHCISplitPeriodicEndpoint[%p]::FindStartFrameAndStartTime - second large isoc not allowed in frame %d", this, frameIndex2);
				epWillFit = false;
				break;				// we won't fit on this harmonic
			}
			
			if (tempTimeUsed > kEHCIFSMaxFrameBytes)
			{
				USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::FindStartFrameAndStartTime - no room in frame(%d) based on time used", this, frameIndex2);
				epWillFit = false;
				break;				// we won't fit on this harmonic
			}
			
			// now see if I will also fit based on start time
			startTimeThisHarmonic = tempStartTimes[frameIndex2];
			if ((startTimeThisHarmonic + _FSBytesUsed) > kEHCIFSMaxFrameBytes)
			{
				USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::FindStartFrameAndStartTime - no room in frame(%d) based on start time", this, frameIndex2);
				epWillFit = false;
				break;
			}
			
			totalTimeUsedThisHarmonic += tempTimeUsed;
			
			if (startTimeThisHarmonic > startTimeThisFrame)
				startTimeThisFrame = startTimeThisHarmonic;
		}
		if (epWillFit)
		{
			// since we indexed past the array, we know that we fit in every harmonic
			USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::FindStartFrameAndStartTime - looks like we fit at frame (%d)", this, (int)frameIndex);
			if (!foundStartTimeCandidate)
			{
				foundStartTimeCandidate = true;
				bestStartTimeFound = startTimeThisFrame+1;
			}
			if (!foundTimeUsedCandidate)
			{
				foundTimeUsedCandidate = true;
				bestTimeUsedFound = totalTimeUsedThisHarmonic+1;
			}
			// note. if the below is less than instead of less than or equal, then we will start at the beginning of the possible frames
			// with <= we gravitate toward the end..
			if (startTimeThisFrame < bestStartTimeFound)
			{
				bestStartTimeFound = startTimeThisFrame;
				bestStartTimeFrame = frameIndex;
				timeUsedForBestStartTimeFrame = totalTimeUsedThisHarmonic;
			}
			if (totalTimeUsedThisHarmonic < bestTimeUsedFound)
			{
				bestTimeUsedFound = totalTimeUsedThisHarmonic;
				bestTimeUsedFrame = frameIndex;
			}
		}
	}
	
	if (!foundStartTimeCandidate)
	{
		USBLog(3, "AppleUSBEHCISplitPeriodicEndpoint[%p]::FindStartFrameAndStartTime - using Start Time entry found - _startFrame(%d) _startTime(%d)", this, (int)_startFrame, _startTime);
		_startTime = 0;
		_startFrame = kEHCIMaxPollingInterval;			// this is invalid
		return kIOReturnNoBandwidth;
	}
	else
	{
		// now see if we got anything
		if (bestStartTimeFound <= bestTimeUsedFound)
		{
			_startTime = bestStartTimeFound;
			_startFrame = bestStartTimeFrame;
			USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::FindStartFrameAndStartTime - using Start Time entry found - _startFrame(%d) _startTime(%d)", this, (int)_startFrame, _startTime);
		}
		else
		{
			_startTime = bestTimeUsedFound;
			_startFrame = bestTimeUsedFrame;
			USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::FindStartFrameAndStartTime - using Start Frame entry found - _startFrame(%d) _startTime(%d)", this, (int)_startFrame, _startTime);
		}
	}
				
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCISplitPeriodicEndpoint::CalculateAllFrameStartTimes(UInt16 *startTimes)
{
	AppleUSBEHCISplitPeriodicEndpoint	*curSPE;
	AppleUSBEHCISplitPeriodicEndpoint	*prevSPE;
	int									frameIndex;
	
	for (frameIndex=0; frameIndex < kEHCIMaxPollingInterval; frameIndex++)
	{
		if (_epType == kUSBInterrupt)
			prevSPE = _myTT->_interruptQueue[frameIndex];
		else
			prevSPE = _myTT->_isochQueue[frameIndex];
		
		if (!prevSPE)
		{
			USBLog(1, "AppleUSBEHCISplitPeriodicEndpoint[%p]::CalculateAllFrameStartTimes - no queue head for frameIndex(%d)", this, (int)frameIndex);
			return kIOReturnInternalError;
		}
		
		curSPE = prevSPE->_nextSPE;
		while (curSPE)
		{
			if (CheckPlacementBefore(curSPE) == kIOReturnSuccess)
				break;
			
			prevSPE = curSPE;
			curSPE = curSPE->_nextSPE;
		}
		startTimes[frameIndex] = CalculateStartTime(frameIndex, prevSPE, curSPE);
	}
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCISplitPeriodicEndpoint::CheckPlacementBefore(AppleUSBEHCISplitPeriodicEndpoint *afterEP)
{
	IOReturn		result = kIOReturnInvalid;
	
	// a large Isoc always returns invalid, as this routine should never be called with a large Isoc packet
	if (_FSBytesUsed < kEHCIFSLargeIsochPacket)
	{
		if (afterEP)
		{
			if (afterEP == this)
			{
				result = kIOReturnSuccess;
			}
			else
			{
				// interrupt endpoints go from the dummy EP at the beginning and builds a tree which looks like
				// the hardware tree. that is, higher periods come first, then smaller periods. within the same group of periods
				// the new EP will come at the end, to decrease the number of EPs which have to move
				// NOTE: Interrupt EPs end up in the list in INCREASING _startTime order
				if ((_epType == kUSBInterrupt) && (afterEP->_period < _period))
					result = kIOReturnSuccess;
				
				// Isoch EPs also go in decreasing period order, with the new EP coming at the end stream of other EPs with 
				// the same period. Note that since the 1.1 spec specified that Isoch EPs have a period of 1, most FS Isoch
				// devices will have a period of 1, so a new EP will always go at the end.
				// NOTE: This results in Isoch EPs ending up in the list in DECREASING _startTime order
				else if ((_epType == kUSBIsoc) && (afterEP->_period <= _period))
					result = kIOReturnSuccess;
			}

		}
		else
		{
			// always OK to insert on an empty list
			result = kIOReturnSuccess;
		}

	}

	USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::CheckPlacementBefore - afterEP[%p] - returning[%p]", this, afterEP, (void*)result);
	return result;
}



UInt16
AppleUSBEHCISplitPeriodicEndpoint::CalculateStartTime(UInt16 frameIndex, AppleUSBEHCISplitPeriodicEndpoint *prevSPE, AppleUSBEHCISplitPeriodicEndpoint *postSPE)
{
	UInt16								startTime = 0;
	AppleUSBEHCISplitPeriodicEndpoint	*tempSPE;
	
	// prevSPE should always be non-NULL, as there is always at least a dummy SPE in every list
	// postSPE will be NULL if this SPE goes at the end of the list, otherwise we will have to fit in the middle..
	if (!prevSPE)
	{
		USBLog(1, "AppleUSBEHCISplitPeriodicEndpoint[%p]::CalculateStartTime - prevSPE is NULL. no good!", this);
		return 0;
	}
	if (postSPE)
	{
		// we are in the middle of the list
		if (_epType == kUSBIsoc)
		{
			// the isoch list gets sorted in reverse order..
			startTime = postSPE->_startTime + postSPE->_FSBytesUsed;
			USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::CalculateStartTime - found ISOCH frameIndex(%d) prevSPE(%p)", this, frameIndex, prevSPE);
			prevSPE->print(5);
		}
		else
		{
			// interrupt is more complicated because it has to come after Isoch, depending on if there is any isoch
			if (prevSPE == _myTT->_interruptQueue[frameIndex])
			{
				// I will be inserted at the beginning of the interrupt queue (which is not NULL)
				tempSPE = _myTT->_isochQueue[frameIndex];
				if (tempSPE)
				{
					// this should always be true and should point to the dummy SPE for SOF
					if (tempSPE->_nextSPE)
					{
						// this means that there is at least one Isoch EP in this frame
						tempSPE = tempSPE->_nextSPE;
					}
					else if (_myTT->_largeIsoch[frameIndex])
					{
						// in this case, the large isoch is the only isoch we have so far
						tempSPE = _myTT->_largeIsoch[frameIndex];
					}
					
					// tempSPE will either point to the first real Isoch, or to the SOF Isoch, which has the correct _FSBytesUsed in it.
					startTime = tempSPE->_startTime + tempSPE->_FSBytesUsed;
				}
			}
			else
			{
				// we are between two real endpoints
				startTime = prevSPE->_startTime + prevSPE->_FSBytesUsed;
			}

		}

	}
	else
	{
		// we are at the end of the list
		if (_epType == kUSBIsoc)
		{
			tempSPE = _myTT->_largeIsoch[frameIndex];
			
			if (tempSPE)
				startTime = tempSPE->_startTime + tempSPE->_FSBytesUsed;
			else
			{	
				tempSPE = _myTT->_isochQueue[frameIndex];
				startTime = tempSPE->_startTime + tempSPE->_FSBytesUsed;
			}

		}
		else
		{
			// if there are some interrupt SPEs in the list already, then we just add on to the end of the previous
			if (prevSPE != _myTT->_interruptQueue[frameIndex])
				startTime = prevSPE->_startTime + prevSPE->_FSBytesUsed;
			else
			{
				// if there are no previous interrupt SPEs, then we have to add on to the end of the Isoch list instead
				tempSPE = _myTT->_isochQueue[frameIndex];
				if (tempSPE)
				{
					// this should always be true and should point to the dummy SPE for SOF
					if (tempSPE->_nextSPE)
					{
						// this means that there is at least one Isoch EP in this frame
						tempSPE = tempSPE->_nextSPE;
					}
					startTime = tempSPE->_startTime + tempSPE->_FSBytesUsed;
				}
			}
		}
	}

	USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::CalculateStartTime - index (%d) returning startTime(%d)", this, (int)frameIndex, (int)startTime);
	return startTime;
}



UInt16
AppleUSBEHCISplitPeriodicEndpoint::CalculateNewStartTimeFromChange(AppleUSBEHCISplitPeriodicEndpoint *changeSPE)
{
	UInt16								startTime = 0;
	UInt16								worstStartTimeFound = kEHCIFSSOFBytesUsed + kEHCIFSHubAdjustBytes;
	AppleUSBEHCISplitPeriodicEndpoint	*prevSPE;
	int									index;
	
	USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::CalculateNewStartTimeFromChange - changeSPE[%p]", this, changeSPE);
	print(5);
	if (changeSPE)
		changeSPE->print(5);
	
	for (index = _startFrame; index < kEHCIMaxPollingInterval; index += _period)
	{
		// find the SPE which occurs previous to myself in the appropriate list for this frame
		if (_epType == kUSBIsoc)
			prevSPE = _myTT->_isochQueue[index];
		else 
			prevSPE = _myTT->_interruptQueue[index];
		
		while (prevSPE->_nextSPE != this)
			prevSPE = prevSPE->_nextSPE;
		
		USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::CalculateNewStartTimeFromChange - calling CalculateStartTime with index(%d) prevSPE(%p) _nextSPE(%p)", this, index, prevSPE, _nextSPE);
		startTime = CalculateStartTime(index, prevSPE, _nextSPE);
		if (startTime > worstStartTimeFound)
		{
			USBLog(5, "AppleUSBEHCISplitPeriodicEndpoint[%p]::CalculateNewStartTimeFromChange - changeSPE(%p) new startTime(%d)", this, changeSPE, (int)startTime);
			worstStartTimeFound = startTime;
		}
	}
	return worstStartTimeFound;
}
