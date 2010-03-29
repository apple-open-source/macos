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


#ifndef _APPLEEHCIHUBINFO_H
#define _APPLEEHCIHUBINFO_H

#include "AppleUSBEHCI.h"
#include "AppleEHCIListElement.h"

// this structure is used to monitor the hubs which are attached. there will
// be an instance of this structure for every high speed hub with a FS/LS
// device attached to it. If the hub is in single TT mode, then there will
// just be one instance on port 0. If the hub is in multi-TT mode, then there
// will be that instance AND an instance for each active port
class AppleEHCIIsochEndpoint;
class AppleUSBEHCISplitPeriodicEndpoint;

class AppleUSBEHCITTInfo : public OSObject
{
    OSDeclareDefaultStructors(AppleUSBEHCITTInfo)
	
public:	
	static AppleUSBEHCITTInfo	*NewTTInfo(int portAddress);

	// from OSObject
	virtual void release() const;

	// AppleUSBEHCITTInfo methods 
	IOReturn	AllocatePeriodicBandwidth(AppleUSBEHCISplitPeriodicEndpoint *pSPE);
	IOReturn	DeallocatePeriodicBandwidth(AppleUSBEHCISplitPeriodicEndpoint *pSPE);
	
	// this methods help track time reserved for IN bytes after periodic CS tokens
	IOReturn	ReserveHSSplitINBytes(int frame, int uFrame, UInt16 bytesToReserve);
	IOReturn	ReleaseHSSplitINBytes(int frame, int uFrame, UInt16 bytesToRelease);
	
	// these methods help track the bytes used on the FS bus
	IOReturn	ReserveFSBusBytes(int frame, UInt16 bytesToReserve);
	IOReturn	ReleaseFSBusBytes(int frame, UInt16 bytesToRelease);
	
	IOReturn	CalculateSPEsToAdjustAfterChange(AppleUSBEHCISplitPeriodicEndpoint *pSPEChanged, bool added);

	// debugging aids
	void		print(int level, const char *fromStr);
	IOReturn	ShowPeriodicBandwidthUsed(int level, const char *fromStr);
	IOReturn	ShowHSSplitTimeUsed(int level, const char *fromStr);
	
	
    AppleUSBEHCITTInfo					*next;
	AppleUSBEHCISplitPeriodicEndpoint	*_largeIsoch[kEHCIMaxPollingInterval];				// special case large (> half) Isoch xaction
	AppleUSBEHCISplitPeriodicEndpoint	*_interruptQueue[kEHCIMaxPollingInterval];			// the head of the interrupt list for each frame in the TT
	AppleUSBEHCISplitPeriodicEndpoint	*_isochQueue[kEHCIMaxPollingInterval];				// the head of the isoch list for each frame  in the TT
	OSOrderedSet						*_pSPEsToAdjust;									// an ordered set of SPEs to adjust
    UInt8								hubPort;
	UInt16								_thinkTime;
	UInt16								_FStimeUsed[kEHCIMaxPollingInterval];				// the amound of time used (in FS bytes) for each frame
	UInt16								_HSSplitINBytesUsed[kEHCIMaxPollingInterval][kEHCIuFramesPerFrame];
		
};

enum 
{
	kAppleEHCITTInfoInitialOrderedSetSize =		16
};


class AppleUSBEHCIHubInfo : public OSObject
{
    OSDeclareDefaultStructors(AppleUSBEHCIHubInfo)
	
public:
	static AppleUSBEHCIHubInfo *FindHubInfo(AppleUSBEHCIHubInfo *hubList, USBDeviceAddress hubAddress);	
	static AppleUSBEHCIHubInfo *AddHubInfo(AppleUSBEHCIHubInfo **hubList, USBDeviceAddress hubAddress, UInt32 flags);
	static IOReturn				DeleteHubInfo(AppleUSBEHCIHubInfo **hubList, USBDeviceAddress hubAddress);

	AppleUSBEHCITTInfo			*GetTTInfo(int portAddress);

private:
    AppleUSBEHCIHubInfo		*next;
	AppleUSBEHCITTInfo		*ttList;
    bool					multiTT;
    UInt8					hubAddr;
	
};


class AppleUSBEHCISplitPeriodicEndpoint : public OSObject
{
    OSDeclareDefaultStructors(AppleUSBEHCISplitPeriodicEndpoint)
	
public:
	static AppleUSBEHCISplitPeriodicEndpoint	*NewSplitPeriodicEndpoint(AppleUSBEHCITTInfo *myTT,
																		  UInt16 epType, OSObject *pEP,
																		  UInt16 FSBytesUsed, UInt8 period);
	
	// from OSObject
	virtual void release() const;

	// debugging
	void		print(int level);
	
	IOReturn	FindStartFrameAndStartTime(void);
	IOReturn	CalculateAllFrameStartTimes(UInt16 *startTimes);
	IOReturn	SetStartFrameAndStartTime(UInt8 startFrame, UInt16 startTime);
	IOReturn	CheckPlacementBefore(AppleUSBEHCISplitPeriodicEndpoint *afterEP);
	UInt16		CalculateStartTime(UInt16 frameIndex, AppleUSBEHCISplitPeriodicEndpoint *prevSPE, AppleUSBEHCISplitPeriodicEndpoint *postSPE);
	UInt16		CalculateNewStartTimeFromChange(AppleUSBEHCISplitPeriodicEndpoint *changeSPE);
	
	AppleUSBEHCISplitPeriodicEndpoint		*_nextSPE;
	AppleEHCIQueueHead						*_intEP;				// only valid if _epType == kUSBInterrupt
	AppleEHCIIsochEndpoint					*_isochEP;				// only valid is _epType == kUSBIsoch
	AppleUSBEHCITTInfo						*_myTT;					// pointer to the transaction translator for this EP

	UInt16									_epType;
	UInt16									_FSBytesUsed;			// number of bytes used on the FS bus (including bit stuffing)
	UInt16									_startTime;				// the time (in FS bytes) that this EP will get serviced on the FS bus
	UInt8									_direction;				// copied from the interrupt or isoch endpoint
	UInt8									_period;				// this is the number of ms between services of this EP
	UInt8									_startFrame;			// the frame number in the 32 ms schedule where this EP is resting (may have multiple frames)
	UInt8									_numSS;					// the number of SS needed for this EP
	UInt8									_numCS;					// the number of CS needed for this EP
	UInt8									_SSflags;				// SS flags for the hardware programming
	UInt									_CSflags;				// CS flags for the hardware programming
	bool									_wraparound;			// do we need to wrap around to the next frame
	
	
};

enum
{
    kUSBEHCIFlagsMuliTT		= 0x0001
};

#endif
