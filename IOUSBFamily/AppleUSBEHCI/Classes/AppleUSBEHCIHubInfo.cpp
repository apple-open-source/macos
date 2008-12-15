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

#include <IOKit/IOTypes.h>

#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBEHCI.h"
#include "AppleUSBEHCIHubInfo.h"

OSDefineMetaClassAndStructors(AppleUSBEHCIHubInfo, OSObject)

AppleUSBEHCIHubInfo*
AppleUSBEHCIHubInfo::GetHubInfo(AppleUSBEHCIHubInfo **hubListPtr, USBDeviceAddress hubAddr, int hubPort)
{
    AppleUSBEHCIHubInfo 	*hiList = *hubListPtr;
    AppleUSBEHCIHubInfo 	*hiPtr;

	// first see if it already exists
	hiPtr = FindHubInfo(hiList, hubAddr, hubPort);
	
	// if it doesn't already exist, see if the "master" for port 0 exists
	if (!hiPtr)
	{
		USBLog(7, "AppleUSBEHCIHubInfo::GetHubInfo -  addr: %d port %d not found, looking for master", hubAddr, hubPort);

		hiPtr = FindHubInfo(hiList, hubAddr, 0);
		// if the master exists, check the flags - perhaps the master is the right one
		// if the master does not exist, then we will not create it - that needs to be done when the hub is added
		if (hiPtr && (hiPtr->flags & kUSBEHCIFlagsMuliTT))
		{
			// this is the case where the per port hiPtr did not exist, but the master did,
			// and the flags say that we should have one per port
			USBLog(7, "AppleUSBEHCIHubInfo::GetHubInfo -  creating info for addr: %d port %d", hubAddr, hubPort);
			hiPtr = NewHubInfo(hubAddr, hubPort);
			if (hiPtr)
			{
				hiPtr->next = hiList;
				hiList = hiPtr;
			}
		}
	}
	
	*hubListPtr = hiList;
	return hiPtr;
}



AppleUSBEHCIHubInfo*
AppleUSBEHCIHubInfo::NewHubInfoZero	(AppleUSBEHCIHubInfo **hubListPtr, USBDeviceAddress hubAddr, UInt32 flags)
{
    AppleUSBEHCIHubInfo 	*hiList = *hubListPtr;
    AppleUSBEHCIHubInfo 	*hiPtr;
	
	USBLog(3, "AppleUSBEHCIHubInfo::NewHubInfoZero -  addr: %d", hubAddr);
	hiPtr = NewHubInfo(hubAddr, 0);
	if (!hiPtr)
		return NULL;
	
	hiPtr->flags = flags;
	hiPtr->next = hiList;
	hiList = hiPtr;
	*hubListPtr = hiList;
	
	return hiPtr;
}	



AppleUSBEHCIHubInfo*
AppleUSBEHCIHubInfo::FindHubInfo(AppleUSBEHCIHubInfo *hiPtr, USBDeviceAddress hubAddr, int hubPort)
{    
    while (hiPtr)
    {
		if ((hiPtr->hubAddr == hubAddr) && (hiPtr->hubPort == hubPort))
			break;
		hiPtr = hiPtr->next;
    }
    
    USBLog(5, "AppleUSBEHCIHubInfo::FindHubInfo(%d, %d), returning %p", hubAddr, hubPort, hiPtr);
    
    return hiPtr;
}



AppleUSBEHCIHubInfo *
AppleUSBEHCIHubInfo::NewHubInfo(USBDeviceAddress hubAddr, int hubPort)
{
    AppleUSBEHCIHubInfo 	*hiPtr = new AppleUSBEHCIHubInfo;
	int						i;
    
    if (!hiPtr)
		return NULL;
    
    hiPtr->hubAddr = hubAddr;
    hiPtr->hubPort = hubPort;
    hiPtr->next = NULL;
    hiPtr->flags = 0;
    
	//
	// theory of the bandwidth initialization
	// according to the USB Spec, we can transmit up to 188 bytes of Isochronous data per HS microframe
	// However, according to at least one Transaction Translator vendor we have spoken to, the real
	// limit, due to bit stuffing, etc, is more like 180 bytes per microframe of payload, IF there is only 
	// one Start Split in the microframe. So we will initialize each microframe as being able to handle 190
	// bytes, and when we allocate it, we will subtract the payload plus 10 bytes for the overhead
	//
	
	for (i=0; i < 8; i++)
	{
		hiPtr->isochOUTUsed[i] = 0;
		hiPtr->isochINUsed[i] = 0;
		hiPtr->interruptUsed[i] = 0;
	}
	
    return hiPtr;
}



IOReturn
AppleUSBEHCIHubInfo::DeleteHubInfoZero(AppleUSBEHCIHubInfo **hubListPtr, USBDeviceAddress hubAddress)
{
    AppleUSBEHCIHubInfo 	*hiList = *hubListPtr;
    AppleUSBEHCIHubInfo 	*hiPtr, *tempPtr;

	// first remove any at the beginning
	while (hiList && (hiList->hubAddr == hubAddress))
	{
		hiPtr = hiList;
		hiList = hiList->next;
		USBLog(5, "AppleUSBEHCIHubInfo::DeleteHubInfoZero- releasing %p from head of list", hiPtr);
		hiPtr->release();
	}

	hiPtr = hiList;
	while (hiPtr && (hiPtr->next))
	{
		if (hiPtr->next->hubAddr == hubAddress)
		{
			tempPtr = hiPtr->next;
			hiPtr->next = tempPtr->next;
			USBLog(5, "AppleUSBEHCIHubInfo::DeleteHubInfoZero- releasing %p from middle of list", tempPtr);
			tempPtr->release();
		}
		else
			hiPtr = hiPtr->next;
	}
	*hubListPtr = hiList;								// fix the master pointer
	return kIOReturnSuccess;
}



UInt32		
AppleUSBEHCIHubInfo::AvailableInterruptBandwidth()
{
	int			i;
	UInt32		avail = 0;
	
	// Theory - because of concerns overlapping Split Transactions, we will only allocate Interrupt endpoints on microframes 0 through 3
	// and then only if there is no Isoch bandwidth allocated on those microframes
	
	// if there is Isoch sceduled on microframe 0, then we have none available. Otehrwise, we start with that
	if (!isochINUsed[0] && !isochOUTUsed[0])
	{
		avail = kUSBEHCIMaxMicroframePeriodic - interruptUsed[0];
		
		// now add what is available on microframes 1-3 as long as there is not already any Isoch scheduled on it
		for (i=0; i<4; i++)
		{
			// if we hit any isoch traffic, we need to stop adding
			if (isochINUsed[i] || isochOUTUsed[i])
				break;

			avail += (kUSBEHCIMaxMicroframePeriodic - interruptUsed[i]);
		}
	}

	USBLog(3, "AppleUSBEHCIHubInfo[%p]::AvailableInterruptBandwidth, returning %d", this, (uint32_t)avail);
	
	return avail;
}



UInt32
AppleUSBEHCIHubInfo::AvailableIsochBandwidth(UInt32 direction)
{
	UInt32		availBandwidth = 0, maxSegment = 0;
	int			i, thisFrameSize;
	bool		firstSegment = true, lastSegment=false;				// we can have a short frame as the first segment or the last
	
	switch (direction)
	{
		case	kUSBOut:
			for (i=0; i<8; i++)
			{
				// we can't use any microframes with interrupt scheduling
				if (interruptUsed[i])
					continue;

				USBLog(3, "AppleUSBEHCIHubInfo[%p]::AvailableIsochBandwidth OUT, microframe %d is using %d bytes OUT and %d bytes IN", this, i, isochOUTUsed[i], isochINUsed[i]);
				// first check to see if we are using any Isoch IN bandwidth. if so, we can only use OUT up until it, and we cannot go further
				if (isochINUsed[i])
				{
					thisFrameSize = (kUSBEHCIMaxMicroframePeriodic - isochINUsed[i]);
					lastSegment = true;
					if ((thisFrameSize + isochOUTUsed[i]) > kUSBEHCIMaxMicroframePeriodic)
						thisFrameSize = kUSBEHCIMaxMicroframePeriodic - isochOUTUsed[i];
				}
				// now see if we are using any OUT already. if so, this has to be either the first or last segment
				else if (isochOUTUsed[i])
				{
					thisFrameSize = kUSBEHCIMaxMicroframePeriodic - isochOUTUsed[i];
					if (!firstSegment)
						lastSegment = true;
				}
				else
					thisFrameSize = kUSBEHCIMaxMicroframePeriodic;
				
				if (thisFrameSize)
					firstSegment = false;
				
				maxSegment += thisFrameSize;
				
				if (lastSegment)
				{
					// this is a partially filled frame - we need to end
					if (maxSegment > availBandwidth)
						availBandwidth = maxSegment;
					maxSegment = 0;
					firstSegment = true;
					lastSegment = false;
				}
			}
			break;
			
		case kUSBIn:
			// microframe 0 is actually the LAST possible microframe for an IN CS, and can only be about 1/2 frame MAX
			// we need to subtract any Interrupt which is used in frame 0, but not OUT, since that really won't happen
			// until frame 1
			if (!isochINUsed[0])
				maxSegment = kUSBEHCIMaxMicroframePeriodic / 2;			// max possible for frame 0
			
			if ((maxSegment + interruptUsed[0]) > kUSBEHCIMaxMicroframePeriodic)
				maxSegment = kUSBEHCIMaxMicroframePeriodic - interruptUsed[0];
				
			USBLog(3, "AppleUSBEHCIHubInfo[%p]::AvailableIsochBandwidth IN, microframe 0 has %d bytes available", this, (uint32_t)maxSegment);
			if (maxSegment)
				firstSegment = false;
				
			for (i = 7; i > 0; i--)
			{
				USBLog(3, "AppleUSBEHCIHubInfo[%p]::AvailableIsochBandwidth IN, microframe %d is using %d bytes OUT and %d bytes IN", this, i, isochOUTUsed[i], isochINUsed[i]);
				if (isochOUTUsed[i])
				{
					// if there is OUT traffic schedule, we cannot use ANY IN on this segment
					thisFrameSize = 0;
					lastSegment = true;
				}
				else if (isochINUsed[i])
				{
					thisFrameSize = kUSBEHCIMaxMicroframePeriodic - isochINUsed[i];
					if (!firstSegment)
						lastSegment = true;
				}
				else
					thisFrameSize = kUSBEHCIMaxMicroframePeriodic;
				maxSegment += thisFrameSize;
				if (lastSegment)
				{
					// this is a partially filled frame - we need to end
					if (maxSegment > availBandwidth)
						availBandwidth = maxSegment;
					maxSegment = 0;
					firstSegment = true;
					lastSegment = false;
				}
			}
			break;
			
		default:
			USBLog(1, "AppleUSBEHCIHubInfo[%p]::AvailableIsochBandwidth, invalid direction %d", this, (uint32_t)direction);
	}
	
	// check to see if the max segment was determined before we exited the loop
	if (maxSegment > availBandwidth)
		availBandwidth = maxSegment;
	
	// return the amount of bandwidth available for Isoch endpoints
	USBLog(3, "AppleUSBEHCIHubInfo[%p]::AvailableIsochBandwidth, returning %d", this, (uint32_t)availBandwidth);
	return availBandwidth;
}



IOReturn
AppleUSBEHCIHubInfo::AllocateInterruptBandwidth(AppleEHCIQueueHead *pQH, UInt32 maxPacketSize)
{
	UInt32		mySMask = 0;
	UInt8		myStartFrame;			// microframe for the SS
	int			i;
	UInt32		thisFrameSize;
	
	USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateInterruptBandwidth: allocating %d", this, (uint32_t)maxPacketSize);

	for (i=0; i<4; i++)
	{
		// as soon as we run into some Isoch traffic, we are done
		if (isochINUsed[i] || isochOUTUsed[i])
			break;
		// if there is not enough left to even get started on this frame, then just go to the next
		if ((interruptUsed[i] + kUSBEHCIFSPeriodicOverhead) > kUSBEHCIMaxMicroframePeriodic)
			continue;
		
		thisFrameSize = kUSBEHCIMaxMicroframePeriodic - interruptUsed[i];
		if (thisFrameSize > maxPacketSize)
			thisFrameSize = maxPacketSize;
		
		maxPacketSize -= thisFrameSize;
		myStartFrame = i;
		if (maxPacketSize)
		{
			// if we still need more, we can look ahead, but at most one frame - unless we are at the limit already
			if (i == 3)
				break;
			if (isochINUsed[i+1] || isochOUTUsed[i+1])
				break;
			if ((interruptUsed[i+1] + maxPacketSize + kUSBEHCIFSPeriodicOverhead) > kUSBEHCIMaxMicroframePeriodic)
				break;
		}
		// there is room for the rest in the next frame, so i can fill in some more values
		USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateInterruptBandwidth: assigning %d bytes to microframe %d", this, (uint32_t)thisFrameSize, i);
		interruptUsed[i] += (thisFrameSize + kUSBEHCIFSPeriodicOverhead);
		pQH->_bandwidthUsed[i] = thisFrameSize + kUSBEHCIFSPeriodicOverhead;
		mySMask = (0x01 << (i+kEHCIEDSplitFlags_SMaskPhase));		// 3272813 - start on microframe 0
		mySMask |= (0x1C << (i+kEHCIEDSplitFlags_CMaskPhase));		// Complete  on frames 2, 3, and 4
		if (!maxPacketSize)
			break;
	}
	
	// if we still have some residue, there is no more room
	if (maxPacketSize)
		return kIOReturnNoBandwidth;
	
    pQH->GetSharedLogical()->splitFlags |= HostToUSBLong(mySMask);

	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCIHubInfo::DeallocateInterruptBandwidth(AppleEHCIQueueHead *pQH)
{
	int		i;
	
	// return some bandwidth from the given interrupt endpoint
	if (pQH)
	{
		for (i=0; i<4; i++)
		{
			if (pQH->_bandwidthUsed[i])
			{
				USBLog(3, "AppleUSBEHCIHubInfo[%p]::DeallocateInterruptBandwidth: deallocating %d bytes from microframe %d", this, pQH->_bandwidthUsed[i], i);
				interruptUsed[i] -= pQH->_bandwidthUsed[i];
				pQH->_bandwidthUsed[i] = 0;
			}
		}
	}
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCIHubInfo::AllocateIsochBandwidth(AppleEHCIIsochEndpoint	*pEP, UInt32 maxPacketSize)
{
	UInt32		remainder = maxPacketSize;
	UInt32		thisFrameSize;
	UInt8		startSplitFlags = 0, completeSplitFlags = 0, completeSplitsNeeded = 0, SSframe = 255, CSframe=0;
	UInt8		framesUsed[8];
	UInt8		amountsUsed[8];
	int			i,j, frameIndex;
	
	if (maxPacketSize == 0)
		return kIOReturnSuccess;
	
	if (AvailableIsochBandwidth(pEP->direction) < maxPacketSize)
	{
		USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth - maxPacketSize greater than known available - returning no bandwidth", this);
		return kIOReturnNoBandwidth;
	}
	
	for (i=0; i<8; i++)
	{
		framesUsed[i] = 255;
		amountsUsed[i] = 255;
	}
	// allocate some bandwidth on the given Isoch endpoint
	switch (pEP->direction)
	{
		case kUSBIn:
			USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: allocating %d bytes IN", this, (uint32_t)maxPacketSize);
			// IN is weird, because we need to actually reserve more bandwidth than we need, since we might
			// get 1-180 bytes in the first frame and 1-180 in the last, and we don't really know which it will be
			// so start by increasing the remainder
			
			// edge case - we need more than 6 frames - 5 full frames and 2 partial frames
			// - in that case we will need both a start split and a complete split on frame 0
			frameIndex = 0;
			if (remainder > ((5*kUSBEHCIMaxMicroframePeriodic)+2))
			{
				// in order to get a frame this big to work, we need to use every microframe, and they
				// cannot be used for anything else - See Figure 4-21
				for(i=0; i< 8; i++)
					if (isochOUTUsed[i] || interruptUsed[i] || isochINUsed[i])
					{
						USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth:  We want %d bytes, but some microframe is already using some, so not bandwidth (isochOUT: %d, interruptUsed: %d, isochIN: %d", this, (int)maxPacketSize, (int)isochOUTUsed[i], (int)interruptUsed[i], (int)isochINUsed[i]);
						return kIOReturnNoBandwidth;
					}
				i=0;
				thisFrameSize = kUSBEHCIMaxMicroframePeriodic;
				while (remainder)
				{
					if (remainder < kUSBEHCIMaxMicroframePeriodic)
						thisFrameSize = remainder;
					remainder -= thisFrameSize;
					framesUsed[frameIndex]=i;
					amountsUsed[frameIndex++] = thisFrameSize;
				}
				completeSplitFlags = 0xFD;				// CS on every frame except 1
				startSplitFlags = 1;					// SS on frame 0
				pEP->useBackPtr = true;
			}
			else
			{
				// we cannot issue a SS on microframe 7, although we can issue a CS on that microframe
				// at one point i thought this was the right code, but now i think it is always 7,
				// since we always subtract 1 for the SS
				//	for (i = (remainder > isochINAvailable[7]) ? 7 : 6; i > 1; i--)
				for (i = 7; i > 1; i--)
				{
					if ((isochINUsed[i] < kUSBEHCIMaxMicroframePeriodic) && (isochOUTUsed[i] == 0))
					{
						if ((isochINUsed[i] + remainder) < kUSBEHCIMaxMicroframePeriodic)
							thisFrameSize = remainder;
						else
							thisFrameSize = kUSBEHCIMaxMicroframePeriodic - isochINUsed[i];
						if (SSframe > i)
							SSframe = i;
						completeSplitsNeeded++;
						USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth - frame %d uses %d bytes, total completesplits is now %d (SSframe is %d)", this, (uint32_t)i, (uint32_t)thisFrameSize, completeSplitsNeeded, SSframe);
						remainder -= thisFrameSize;
						framesUsed[frameIndex] = i;
						USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth - using %d bytes in frame %d even though we might only need %d", this, kUSBEHCIMaxMicroframePeriodic, frameIndex, (int)thisFrameSize);
						amountsUsed[frameIndex++] = kUSBEHCIMaxMicroframePeriodic;				// since IN xactions cant overlap, we need to claim the entire uFrame
						if (!remainder)
						{
							// we have every frame scheduled, now calculate SS and CC bitmasks
							// SS need to happen on previous frame because of the 1 frame delay
							startSplitFlags = 1 << (SSframe-1);
							completeSplitFlags = 0;
							completeSplitsNeeded++;					// always need one extra CS
							USBLog(3, "AppleUSBEHCIHubInfo::AllocateIsochBandwidth - extra completesplits is now %d (SSFrame = %d)", completeSplitsNeeded, SSframe);
							CSframe = (SSframe + 1) % 8;			// CS frame is 0-6
							while (completeSplitsNeeded--)
							{
								if (CSframe == 0)
									pEP->useBackPtr = true;
								completeSplitFlags |= (1 << CSframe);
								CSframe = (CSframe + 1) % 8;
							}
							break;
						}
					}
					else
					{
						if (remainder == maxPacketSize)
							continue;
						USBLog(1, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: ran out of IN frames - need to redo", this);
						// code needed here to reset the framesUsed and amounstUsed structure
						// need to check to make sure we are still OK
					}
				}
				if (remainder)
				{
					USBLog(1, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: not enough bandwidth for IN transaction", this);
					return kIOReturnNoBandwidth;
				}
			}
			for (frameIndex=0; (frameIndex < 8) && (framesUsed[frameIndex] < 255); frameIndex++)
			{
				USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth IN: using %d bytes in frame %d", this, amountsUsed[frameIndex], framesUsed[frameIndex]);
				pEP->bandwidthUsed[framesUsed[frameIndex]] = amountsUsed[frameIndex];
				isochINUsed[framesUsed[frameIndex]] += amountsUsed[frameIndex];
			}
			pEP->startSplitFlags = startSplitFlags;
			pEP->completeSplitFlags = completeSplitFlags;
			pEP->isochINSSFrame = SSframe;
			break;

		case kUSBOut:
			USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: allocating %d bytes OUT", this, (uint32_t)maxPacketSize);
			for (i=0,frameIndex=0; i< 7; i++)
			{
				if ((interruptUsed[i] == 0) && (isochINUsed[i] == 0) && (isochOUTUsed[i] < kUSBEHCIMaxMicroframePeriodic))
				{
					USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: OUT frame %d already using %d", this, i, isochOUTUsed[i]);
					if ((isochOUTUsed[i] + remainder) < kUSBEHCIMaxMicroframePeriodic)
						thisFrameSize = remainder;
					else
						thisFrameSize = kUSBEHCIMaxMicroframePeriodic - isochOUTUsed[i];
					USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: OUT frame %d will have %d bytes (remainder=%d)", this, i, (uint32_t)thisFrameSize, (uint32_t)remainder);
					startSplitFlags |= (1 << i);
					remainder -= thisFrameSize;
					framesUsed[frameIndex] = i;
					amountsUsed[frameIndex++] = thisFrameSize;
				}
				else
				{
					if (remainder == maxPacketSize)
						continue;
					USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: ran out of OUT frames - need to redo", this);
				}
				if (!remainder)
					break;
			}
			if (remainder)
			{
				USBLog(1, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: not enough bandwidth for OUT transaction", this);
				return kIOReturnNoBandwidth;
			}
			for (frameIndex=0; (frameIndex < 8) && (framesUsed[frameIndex] < 255); frameIndex++)
			{
				
				USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: using %d bytes in frame %d", this, amountsUsed[frameIndex], framesUsed[frameIndex]);
				pEP->bandwidthUsed[framesUsed[frameIndex]] = amountsUsed[frameIndex];
				isochOUTUsed[framesUsed[frameIndex]] += amountsUsed[frameIndex];
			}
			pEP->startSplitFlags = startSplitFlags;
			pEP->completeSplitFlags = 0;
			break;

		default:
			USBLog(1, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: unknown pEP direction (%d)", this, pEP->direction);
		
	}
	if (!remainder)
	{
		pEP->maxPacketSize = maxPacketSize;
		USBLog(3, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: SS %x CS %x", this, pEP->startSplitFlags, pEP->completeSplitFlags);
		return kIOReturnSuccess;
	}
	USBLog(1, "AppleUSBEHCIHubInfo[%p]::AllocateIsochBandwidth: returning kIOReturnNoBandwidth", this);
	return kIOReturnNoBandwidth;
}



IOReturn
AppleUSBEHCIHubInfo::DeallocateIsochBandwidth(AppleEHCIIsochEndpoint* pEP)
{
	int i;
	// return some bandwidth from the given Isoch endpoint
	USBLog(3, "AppleUSBEHCIHubInfo[%p]::DeallocateIsochBandwidth: endpoint %p", this, pEP);
	switch (pEP->direction)
	{
		case kUSBIn:
			for (i=0;i<8;i++)
			{
				if (pEP->bandwidthUsed[i])
				{
					USBLog(3, "AppleUSBEHCIHubInfo[%p]::DeallocateIsochBandwidth: returning %d IN bytes, frame %d", this, pEP->bandwidthUsed[i], i);
					isochINUsed[i] -= pEP->bandwidthUsed[i];
					pEP->bandwidthUsed[i] = 0;
				}
			}
			break;
			
		case kUSBOut:
			for (i=0;i<8;i++)
			{
				if (pEP->bandwidthUsed[i])
				{
					isochOUTUsed[i] -= pEP->bandwidthUsed[i];
					USBLog(3, "AppleUSBEHCIHubInfo[%p]::DeallocateIsochBandwidth: returned %d OUT bytes, frame %d, isochOUTUsed now %d", this, pEP->bandwidthUsed[i], i, isochOUTUsed[i]);
					pEP->bandwidthUsed[i] = 0;
				}
			}
			break;
			
		default:
			USBLog(3, "AppleUSBEHCIHubInfo[%p]::DeallocateIsochBandwidth: unknown pEP direction (%d)", this, pEP->direction);
			break;
			
	}
	pEP->maxPacketSize = 0;
	return kIOReturnSuccess;
}



IOReturn
AppleUSBEHCIHubInfo::ReallocateIsochBandwidth(AppleEHCIIsochEndpoint* pEP, UInt32 maxPacketSize)
{
	IOReturn res = kIOReturnSuccess;
	
	// change the amount of allocated bandwidth on the given Isoch endpoint
	USBLog(3, "AppleUSBEHCIHubInfo[%p]::ReallocateIsochBandwidth: reallocating ep %p (%d) to size %d", this, pEP, (uint32_t)pEP->maxPacketSize, (uint32_t)maxPacketSize);
	if (pEP->maxPacketSize != maxPacketSize)
	{
		if (pEP->maxPacketSize)
			DeallocateIsochBandwidth(pEP);
		
		res = AllocateIsochBandwidth(pEP, maxPacketSize);
	}
	else
	{
		USBLog(3, "AppleUSBEHCIHubInfo[%p]::ReallocateIsochBandwidth: MPS not changed, ignoring", this);
	}

	if (res)
	{
		USBLog(3, "AppleUSBEHCIHubInfo[%p]::ReallocateIsochBandwidth: reallocation failed, result=0x%x", this, res);
	}

	return res;
}
