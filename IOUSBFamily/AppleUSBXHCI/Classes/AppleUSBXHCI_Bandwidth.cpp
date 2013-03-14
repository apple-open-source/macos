//
//  AppleUSBXHCI_Bandwidth.cpp
//  AppleUSBXHCI
//
//  Copyright 2011-2012 Apple Inc. All rights reserved.
//

#include "AppleUSBXHCI_Bandwidth.h"




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


#pragma mark ----------- TT Bandwidth Table  ---------------------
OSDefineMetaClassAndStructors(TTBandwidthTable, OSObject);

TTBandwidthTable *
TTBandwidthTable::WithHubAndPort(UInt8 hubSlot, UInt8 hubPort, bool mtt)
{
	TTBandwidthTable *me = OSTypeAlloc(TTBandwidthTable);
	if (me)
	{	
		int			i;
		
		me->init();
		
		me->hubSlotID = hubSlot;
		me->mtt = mtt;
		if (mtt)
			me->hubPortNum = hubPort;
		for (i=0; i < kMaxIntervalTableSize; i++)
		{
			me->interval[i].worstCaseMPS = 0;
			me->interval[i].totalPackets = 0;
			me->interval[i].packetOverhead = 0;
		}

	}
	return me;
}



void
TTBandwidthTable::AddToTable(UInt8 epSpeed, UInt8 epInterval, UInt16 mps)
{
	UInt16				mpsInBlocks= 0;
	bool				forceLS = false;
	UInt8				normalizedInterval = epInterval - 3;
	
	USBLog(5, "TTBandwidthTable[%p]::AddToTable: epSpeed(%d) epInterval(%d) mps(%d)", this, epSpeed, epInterval, mps);
	
	if ((epInterval < 3) || (epInterval >= kMaxFSIsochInterval))
	{
		USBLog(1, "TTBandwidthTable[%p]::AddToTable - invalid FS/LS interval of %d", this, epInterval);
		return;
	}
	
	if (epSpeed == kUSBDeviceSpeedLow)
		forceLS = true;
	
	// convert to XHCI blocks now
	if (forceLS)
	{
		mps = mps * 8;
		USBLog(5, "TTBandwidthTable[%p]::AddToTable - LS device  new mps %d", this, mps);
	}
	
	mpsInBlocks = (mps + kFSBytesPerBlock-1) / kFSBytesPerBlock;				// this is a divide by 1, but be consistent - bit stuffing is already accounted for

	if (normalizedInterval == 0)
	{
		interval[0].worstCaseMPS += (mpsInBlocks + (forceLS ? kLSPacketOverheadInBlocks : kFSPacketOverheadInBlocks));
	}
	else
	{
		// intervals 1 and above, which are shared amongst more than 1 packet
		interval[normalizedInterval].totalPackets++;
		
		// we know that the interval is > 2
		if (mpsInBlocks > interval[normalizedInterval].worstCaseMPS)
			interval[normalizedInterval].worstCaseMPS = mpsInBlocks;
		
		if (forceLS)
			interval[normalizedInterval].packetOverhead = kLSPacketOverheadInBlocks;
		else if (interval[normalizedInterval].packetOverhead == 0)
			interval[normalizedInterval].packetOverhead = kFSPacketOverheadInBlocks;
	}
	USBLog(5, "TTBandwidthTable[%p]::AddToTable: interval[%d].worstCaseMPS(%d)", this, normalizedInterval, interval[normalizedInterval].worstCaseMPS);
}



SInt16
TTBandwidthTable::BandwidthAvailable()
{
	UInt16			bandwidthUsedinBlocks = interval[0].worstCaseMPS;	// accounts for any 1ms endpoints
	UInt32			numPacketRemainder = 0;								// from the previous interval
	UInt32			maxPacketSizeRemainder = 0;							// from the previous interval
	UInt32			numPacketsThisInterval = 0;
	UInt32			packetOverhead = 0;
	UInt32			maxBandwidthInBlocks;
	
	USBLog(6, "TTBandwidthTable[%p]::BandwidthAvailable - start with bandwidth used: %d", this, bandwidthUsedinBlocks);
	
	for (int i=1; i < kMaxIntervalTableSize; i++)
	{
		USBLog(6, "TTBandwidthTable[%p]::BandwidthAvailable - interval:%d pkts:%d mps:%d overhead:%d numPktRmndr:%d mpsRmndr:%d ", this, i, (int)interval[i].totalPackets, (int)interval[i].worstCaseMPS, (int)interval[i].packetOverhead, (int)numPacketRemainder, (int)maxPacketSizeRemainder);
		
		// first double the packets from the previous interval and add to this interval
		numPacketRemainder = 2 * numPacketRemainder + interval[i].totalPackets;
		
		if (interval[i].worstCaseMPS > maxPacketSizeRemainder)
			maxPacketSizeRemainder = interval[i].worstCaseMPS;
		
		// calculate how many packets will exactly fit in this interval
		numPacketsThisInterval = numPacketRemainder >> i;
		
		if (interval[i].packetOverhead > packetOverhead)
		{
			packetOverhead = interval[i].packetOverhead;
		}
		
		USBLog(6, "TTBandwidthTable[%p]::BandwidthAvailable - bandwidthUsed[%d] adding %d packets at %d blocks", this, (int)bandwidthUsedinBlocks, (int)numPacketsThisInterval, (int)(packetOverhead + maxPacketSizeRemainder));
		bandwidthUsedinBlocks += numPacketsThisInterval * (maxPacketSizeRemainder + packetOverhead);
		
		numPacketRemainder = numPacketRemainder % (1 << i);
		if (numPacketRemainder == 0)
		{
			maxPacketSizeRemainder = 0;
			packetOverhead = 0;
		}
		else if (numPacketsThisInterval > 0)
		{
			maxPacketSizeRemainder = interval[i].worstCaseMPS;
		}
	}
	
	if (numPacketRemainder)
	{
		USBLog(6, "TTBandwidthTable[%p]::BandwidthAvailable - finished with pkts:%d mps:%d overhead:%d", this, (int)numPacketRemainder, (int)maxPacketSizeRemainder, (int)packetOverhead);
		bandwidthUsedinBlocks += (packetOverhead + maxPacketSizeRemainder);
		
	}
	
	USBLog(4, "TTBandwidthTable[%p]::BandwidthAvailable - returning bandwidth available of %d (out of %d)", this, kLSFSBandwidthLimitInBlocks-bandwidthUsedinBlocks, kLSFSBandwidthLimitInBlocks);
	
	return kLSFSBandwidthLimitInBlocks-bandwidthUsedinBlocks;
}



TTBandwidthTable *
GetTTBandwidthTable(OSArray *ttArray, UInt32 hubSlot, UInt32 hubPort, bool multiTT)
{
	TTBandwidthTable *	ret = NULL;
	int					numTables = ttArray->getCount();
	int					i;
	
	USBLog(5, "GetTTBandwidthTable - slot(%d) port (%d) multiTT(%d)", (int)hubSlot, (int)hubPort, (int)multiTT);
	for (i=0; i < numTables; i++)
	{
		ret = (TTBandwidthTable*)ttArray->getObject(i);
		if (multiTT)
		{
			if ( (ret->hubSlotID == hubSlot) && (ret->hubPortNum == hubPort))
			{
				break;
			}
		}
		else
		{
			if (ret->hubSlotID == hubSlot)
			{
				break;
			}
			else
			{
				USBLog(6, "GetTTBandwidthTable - table %p slot %d != hubSlot %d", ret, (int)ret->hubSlotID, (int)hubSlot);
				ret = NULL;
			}
		}
	}
	if (!ret)
	{
		ret = TTBandwidthTable::WithHubAndPort(hubSlot, hubPort, multiTT);
		if (ret)
		{
			USBLog(5, "GetTTBandwidthTable - new table %p", ret);
			ttArray->setObject(ret);
			ret->release();											// is retained by the OSArray
		}
	}
	
	return ret;
}



#pragma mark ----------- Root Hub Port Table  ---------------------
OSDefineMetaClassAndStructors(RootHubPortTable, OSObject);

RootHubPortTable *
RootHubPortTable::WithRHPortAndSpeed(UInt8 rhPort, UInt8 rhPortSpeed)
{
	RootHubPortTable *me = OSTypeAlloc(RootHubPortTable);
	if (me)
	{
		int			i;
		me->init();
		
		USBLog(5, "RootHubPortTable[%p]::WithRHPortAndSpeed - rhPort(%d) rhSpeed(%d)", me, rhPort, rhPortSpeed);
		me->rhPort = rhPort;
		me->rhPortSpeed = rhPortSpeed;
		for (i=0; i < kMaxIntervalTableSize; i++)
		{
			me->interval[i].worstCaseMPS = 0;
			me->interval[i].totalPackets = 0;
			me->interval[i].packetOverhead = 0;
		}
	}
	
	return me;
}



void
RootHubPortTable::AddToTable(UInt8 epInterval, UInt16 mps, UInt8 maxBurst, UInt8 mult, UInt8 epSpeed, UInt8 hubSlot, UInt8 hubPort, bool mtt)
{
	UInt16			mpsInBlocks = mps;
	UInt16			packetoverhead;
	
	interval[epInterval].totalPackets += (maxBurst +1);
	
	if ((rhPortSpeed == kUSBDeviceSpeedLow) || ((rhPortSpeed == kUSBDeviceSpeedFull) && (epSpeed == kUSBDeviceSpeedLow)))
	{
		// this is either a LS device directly connected or a LS device connected to a FS hub which is directly connected
		mpsInBlocks *= 8;								// convert to FS equivalent
		mpsInBlocks += (kFSBytesPerBlock-1);			// round up
		mpsInBlocks /= kFSBytesPerBlock;
		packetoverhead = kLSPacketOverheadInBlocks;
	}
	else if (rhPortSpeed == kUSBDeviceSpeedFull)
	{
		// either a FS device directly connected or a FS hub with a FS device downstream
		mpsInBlocks += (kFSBytesPerBlock-1);			// round up
		mpsInBlocks /= kFSBytesPerBlock;
		packetoverhead = kFSPacketOverheadInBlocks;
	}
	else if (rhPortSpeed == kUSBDeviceSpeedHigh)
	{
		// HS hub - secondary (TT) bandwidth will be taken care of on the side - just need to deal with HS bandwidth here
		mps *= (mult+1);											// multiplier for HSHB endpoints
		mpsInBlocks += (kHSBytesPerBlock-1);						// round up
		mpsInBlocks /= kHSBytesPerBlock;							// bit stuffing is taken into account already
		packetoverhead = kHSPacketOverheadInBlocks;
	}
	else
	{
		// Super Speed
		mps *= (maxBurst * 1);										// get max burst size
		mps *= (mult + 1);											// and account for multiplier
		mpsInBlocks += (kSSBytesPerBlock-1);						// round up
		mpsInBlocks /= kSSBytesPerBlock;							// encoding is taken into account already
		packetoverhead = kSSBurstOverheadInBlocks;
		
	}	
	
	USBLog(6, "RootHubPortTable[%p]::AddToTable - adjusted numbers: interval:%d mpsInBlocks:%d overhead:%d",this, epInterval, mpsInBlocks, packetoverhead);
	if (epInterval == 0)
	{
		
		// since these packets occur every uFrame, then we will go ahead and calculate the fully loaded bandwidth with overhead
		// and then convert to XHCI blocks. the total number of blocks depends on the rhSpeed
		// also note.. epInterval 0 will only be for HS and SS endpoints, since others have a minimum interval of 3 (1ms)
		// also, the root hub and the endpoint better have the same speed for interval 0!
		
		interval[0].worstCaseMPS += ((maxBurst+1) * (mpsInBlocks + packetoverhead));
	}
	else
	{
		// since these packets will be scheduled by the HC, we just keep track of number and mps
		// we will then convert to blocks and add overhead later
		if (mpsInBlocks > interval[epInterval].worstCaseMPS)
			interval[epInterval].worstCaseMPS = mpsInBlocks;
		
		// a LS endpoint on a FS root hub port will have a higher packetOverhead than others, and this accounts for that
		if (packetoverhead > interval[epInterval].packetOverhead)
			interval[epInterval].packetOverhead = packetoverhead;
	}
	
	if (hubSlot)
	{
		if (!ttArray)
		{
			ttArray = OSArray::withCapacity(10);
		}
		if (ttArray)
		{
			TTBandwidthTable*  tt = GetTTBandwidthTable(ttArray, hubSlot, hubPort, mtt);
			if (tt)
			{
				tt->AddToTable(epSpeed, epInterval, mps);
			}
		}
	}
}



void
RootHubPortTable::PrintTableInfo()
{
	
	USBLog(6, "RootHubPortTable[%p]::PrintTableInfo - checking rhPort(%d) speed (%d) downstream tts (%d)", this, rhPort, rhPortSpeed, ttArray ? ttArray->getCount() : 0);
	for (int j=0; j < kMaxIntervalTableSize; j++)
	{
		if (interval[j].totalPackets)
		{
			USBLog(6, "RootHubPortTable[%p]::PrintTableInfo - rhPort[%d] interval:%d packets:%d MPS:%d overhead:%d", this, rhPort, j, interval[j].totalPackets, interval[j].worstCaseMPS, interval[j].packetOverhead);
		}
	}
	
	if (ttArray)
	{
		int			numTTTables = ttArray->getCount();
		for (int i = 0; i < numTTTables; i++)
		{
			TTBandwidthTable *ttTable = (TTBandwidthTable*)ttArray->getObject(i);
			for (int j=0; j < kMaxIntervalTableSize; j++)
			{
				if (ttTable->interval[j].totalPackets || ttTable->interval[j].worstCaseMPS)
				{
					USBLog(6, "RootHubPortTable[%p]::PrintTableInfo - ttTable[%p] interval[%d] packets[%d] MPS[%d] overhead[%d]", this, ttTable, j, ttTable->interval[j].totalPackets, ttTable->interval[j].worstCaseMPS, ttTable->interval[i].packetOverhead);
			
				}
			}
		}
	}
}



// a negative return indicates that this rh is oversubscribed
SInt16			
RootHubPortTable::BandwidthAvailable(void)
{
	UInt16			bandwidthUsedinBlocks = interval[0].worstCaseMPS;
	UInt32			numPacketRemainder = 0;								// from the previous interval
	UInt32			maxPacketSizeRemainder = 0;							// from the previous interval
	UInt32			numPacketsThisInterval = 0;
	UInt32			packetOverhead = 0;
	UInt32			maxBandwidthInBlocks = 0;
	SInt16			bandwidthAvailable = 0;
	
	switch (rhPortSpeed)
	{
		case kUSBDeviceSpeedLow:
			maxBandwidthInBlocks = kLSFSBandwidthLimitInBlocks;
			break;
			
		case kUSBDeviceSpeedFull:
			maxBandwidthInBlocks = kLSFSBandwidthLimitInBlocks;
			break;
			
		case kUSBDeviceSpeedHigh:
			maxBandwidthInBlocks = kHSBandwidthLimitInBlocks;
			break;
			
		case kUSBDeviceSpeedSuper:
			maxBandwidthInBlocks = kSSBandwidthLimitInBlocks;
			break;
			
		default:
			break;
	}
	
	USBLog(6, "RootHubPortTable[%p]::BandwidthAvailable - checking rhPort(%d) speed (%d) downstream tts (%d) BW0[%d]", this, rhPort, rhPortSpeed, ttArray ? ttArray->getCount() : 0, interval[0].worstCaseMPS);
	
	if (ttArray)
	{
		// first check each TT to make sure that it has enough secondary bandwidth.
		// if not, then we must have recently added an EP on the TT which pushes it over the limit
		int		numTTS = ttArray->getCount();
		for (int i=0; i < numTTS; i++)
		{
			TTBandwidthTable	*tt = (TTBandwidthTable*)ttArray->getObject(i);
			bandwidthAvailable = tt->BandwidthAvailable();
			if (bandwidthAvailable < 0)
			{
				USBLog(1, "RootHubPortTable[%p]::BandwidthAvailable: TT did not have enough secondary bandwidth: %d", this, bandwidthAvailable);
				break;
			}
		}
	}
	
	if (bandwidthAvailable >= 0)
	{
		USBLog(6, "RootHubPortTable[%p]::BandwidthAvailable - start interval:0 bandwidth used:%d", this, bandwidthUsedinBlocks);

		for (int i=1; i < kMaxIntervalTableSize; i++)
		{
			USBLog(6, "RootHubPortTable[%p]::BandwidthAvailable - interval:%d pkts:%d mps:%d overhead:%d numPktRmndr:%d mpsRmndr:%d ", this, i, (int)interval[i].totalPackets, (int)interval[i].worstCaseMPS, (int)interval[i].packetOverhead, (int)numPacketRemainder, (int)maxPacketSizeRemainder);
			
			// first double the packets from the previous interval and add to this interval
			numPacketRemainder = 2 * numPacketRemainder + interval[i].totalPackets;
			
			if (interval[i].worstCaseMPS > maxPacketSizeRemainder)
				maxPacketSizeRemainder = interval[i].worstCaseMPS;
			
			// this can change on a FS root hub port due to the presence of a LS device
			if (interval[i].packetOverhead > packetOverhead)
				packetOverhead = interval[i].packetOverhead;
			
			// calculate how many packets will exactly fit in this interval
			numPacketsThisInterval = numPacketRemainder >> i;
			
			if (numPacketsThisInterval)
			{
				USBLog(6, "RootHubPortTable[%p]::BandwidthAvailable - bandwidthUsed[%d] adding %d packets at %d blocks", this, (int)bandwidthUsedinBlocks, (int)numPacketsThisInterval, (int)(packetOverhead + maxPacketSizeRemainder));
			}
			bandwidthUsedinBlocks += numPacketsThisInterval * (packetOverhead + maxPacketSizeRemainder);
			
			numPacketRemainder = numPacketRemainder % (1 << i);
			if (numPacketRemainder == 0)
			{
				maxPacketSizeRemainder = 0;
				packetOverhead = 0;					// get to reset once we have scheduled the packets
			}
			else if (numPacketsThisInterval > 0)
			{
				maxPacketSizeRemainder = interval[i].worstCaseMPS;
			}
		}
		
		if (numPacketRemainder)
		{
			USBLog(6, "RootHubPortTable[%p]::BandwidthAvailable - finished with pkts:%d mps:%d overhead:%d", this, (int)numPacketRemainder, (int)maxPacketSizeRemainder, (int)packetOverhead);
			bandwidthUsedinBlocks += (packetOverhead + maxPacketSizeRemainder);
		}
		bandwidthAvailable = maxBandwidthInBlocks - bandwidthUsedinBlocks;
	}
	USBLog(4, "RootHubPortTable[%p]::BandwidthAvailable - returning bandwidth available of %d (out of %d)", this, bandwidthAvailable, (int)maxBandwidthInBlocks);
	return bandwidthAvailable;
}


void
RootHubPortTable::free()
{
	if (ttArray)
		ttArray->release();
    
	OSObject::free();
}



RootHubPortTable *
GetRootHubPortTable(OSArray *rhPortArray, UInt8 rhPort)
{
	RootHubPortTable *	ret = NULL;
	int					numTables = rhPortArray->getCount();
	int					i;
	
	USBLog(6, "GetRootHubPortTable - numTables %d rhPort %d", numTables, rhPort);
	for (i=0; i < numTables; i++)
	{
		ret = (RootHubPortTable*)rhPortArray->getObject(i);
		if (ret && ret->rhPort == rhPort)
		{
			break;
		}
	}
	if (ret && ret->rhPort != rhPort)
	{
		USBLog(1, "GetRootHubPortTable - could not find table for port %d", rhPort);
		ret = NULL;
	}
	return ret;
}


IOReturn
AppleUSBXHCI::BuildRHPortBandwidthArray(OSArray *rhPortArray)
{
	// first make a pass through all of the devices creating the initial rhTable list with devices 
	// which are connected directly to the root hub (this allows me to get the speed of that port
	
	for(int slot = 0; slot < _numDeviceSlots; slot++)
	{
		if(_slots[slot].buffer != NULL)
		{
			Context	*			slotContext = GetSlotContext(slot);

			UInt8				slotSpeed = GetSlCtxSpeed(slotContext);
			UInt8				rhPort = GetSlCtxRootHubPort(slotContext);
			UInt32				slotRouteString = GetSlCtxRouteString(slotContext);
			
			if (slotRouteString == 0)
			{
				RootHubPortTable *	rhTable = RootHubPortTable::WithRHPortAndSpeed(rhPort, slotSpeed);
				if (rhTable)
				{
					rhPortArray->setObject(rhTable);
					rhTable->release();								// it is now retained by the array
				}
			}
		}
	}
	// now gather all of the data for the existing devices
	// need to collect data on all devices because they all share the same DMI
	for(int slot = 0; slot < _numDeviceSlots; slot++)
	{
		if(_slots[slot].buffer != NULL)
		{
			Context	*			slotContext = GetSlotContext(slot);
			
			UInt8				slotSpeed = GetSlCtxSpeed(slotContext);
			UInt8				slotTTHubSlot = GetSlCtxTTSlot(slotContext);
			UInt8				slotTTHubPort = GetSlCtxTTPort(slotContext);
			bool				slotOnMTTHub = GetSlCtxMTT(slotContext);
			UInt8				rhPort = GetSlCtxRootHubPort(slotContext);
			RootHubPortTable *	rhTable = GetRootHubPortTable(rhPortArray, rhPort);
			
			if (!rhTable)
			{
				USBLog(1, "AppleUSBXHCI[%p]::BuildRHPortBandwidthArray - could not find a rh table for rhPort %d", this, rhPort);
				continue;
			}
			
			UInt8			numEpContexts = GetSlCtxEntries(slotContext);
			
			if (numEpContexts)
			{
				USBLog(6, "AppleUSBXHCI[%p]::BuildRHPortBandwidthArray - slot %d has %d endpoints", this, slot, numEpContexts);
				
				// start at index 2, since 0 is the slot context and 1 is the conrol ep, which is not periodic
				for (int endp = 2; endp <= numEpContexts; endp++)
				{
					Context		*epContext = GetEndpointContext(slot, endp);

					if (GetEpCtxEpState(epContext) != kXHCIEpCtx_State_Disabled)
					{
						UInt8		thisEpType = GetEpCtxEpType(epContext);
						UInt8		thisEpInterval = GetEPCtxInterval(epContext);
						UInt16		thisEpMPS = GetEpCtxMPS(epContext);
						UInt8		thisEPMaxBurst = GetEPCtxMaxBurst(epContext);
						UInt8		thisEPMult = GetEPCtxMult(epContext);
						
						if ((thisEpType == kXHCIEpCtx_EPType_BulkIN) || (thisEpType == kXHCIEpCtx_EPType_BulkOut) || (thisEpType == kXHCIEpCtx_EPType_Control))
						{
							USBLog(7, "AppleUSBXHCI[%p]::BuildRHPortBandwidthArray - don't need to look at Control/Bulk EPs", this);
							continue;
						}
						
						rhTable->AddToTable(thisEpInterval, thisEpMPS, thisEPMaxBurst, thisEPMult, slotSpeed, slotTTHubSlot, slotTTHubPort, slotOnMTTHub);
					}
				}
			}
			
		}
	} // end of gathering data
	return kIOReturnSuccess;
}



#pragma mark ----------- Main Method  ---------------------

IOReturn 
AppleUSBXHCI::CheckPeriodicBandwidth(int slotID, int endpointIdx, UInt16 maxPacketSize, short interval, int epType, UInt8 maxStream, UInt8 maxBurst, UInt8 mult)
{
	IOReturn				err = kIOReturnSuccess;
	Context *				newEpSlotContext;
	UInt32					newEpRootHubPort;
	UInt8					newEpSpeed;
	UInt8					newEpTTHubSlot;
	UInt8					newEpTTHubPort;
	bool					newEpOnMTTHub;
	UInt16					bandwidthUsed[_v3ExpansionData->_rootHubNumPortsHS + _v3ExpansionData->_rootHubNumPortsSS];
	OSArray *				rhPortArray = OSArray::withCapacity(10);
	RootHubPortTable *		rhTableForNewEP = NULL;
	SInt16					bandwidthAvailable;
	
	
	USBLog(5, "AppleUSBXHCI[%p]::CheckPeriodicBandwidth - Sl:%d, ep:%d, mps:%d, poll:%d, typ:%d, maxStream:%d, maxBurst:%d, mult: %d", this, slotID, endpointIdx, maxPacketSize, interval, epType, (int)maxStream, (int)maxBurst, (int)mult);
	
	newEpSlotContext = GetSlotContext(slotID);
	newEpRootHubPort = GetSlCtxRootHubPort(newEpSlotContext);
	newEpTTHubSlot = GetSlCtxTTSlot(newEpSlotContext);
	newEpTTHubPort = GetSlCtxTTPort(newEpSlotContext);
	newEpOnMTTHub = GetSlCtxMTT(newEpSlotContext);
	newEpSpeed = GetSlCtxSpeed(newEpSlotContext);
	
	USBLog(5, "AppleUSBXHCI[%p]::CheckPeriodicBandwidth - new EP speed (%d) RH port(%d) TTHub(%d) TTPort(%d) MTT(%d)", this, newEpSpeed, (int)newEpRootHubPort, newEpTTHubSlot, newEpTTHubPort, newEpOnMTTHub);
	
	err = BuildRHPortBandwidthArray(rhPortArray);
	
	// Now that we have all of the device endpoint information for the existing devices, add in the new device
	rhTableForNewEP = GetRootHubPortTable(rhPortArray, newEpRootHubPort);
	USBLog(6, "AppleUSBXHCI[%p]::CheckPeriodicBandwidth - new endpoint rhTable %p", this, rhTableForNewEP);
	if (rhTableForNewEP)
	{
		rhTableForNewEP->AddToTable(interval, maxPacketSize, maxBurst, mult, newEpSpeed, newEpTTHubSlot, newEpTTHubPort, newEpOnMTTHub);
		bandwidthAvailable = rhTableForNewEP->BandwidthAvailable();
		if (bandwidthAvailable < 0)
			err = kIOReturnNoBandwidth;
		// TODO - also calculate the DMI bandwidth
	}
	
	// The table now as what the bandwidth usage would be with the new EP added in.  see if it is too much..
	int			numRHPortTables = rhPortArray->getCount();
	for (int i = 0; i < numRHPortTables; i++)
	{
		RootHubPortTable *rhTable = (RootHubPortTable*)rhPortArray->getObject(i);
		rhTable->PrintTableInfo();
	}
	
	if (rhPortArray)
		rhPortArray->release();
	
	return err;
}



UInt32 		
AppleUSBXHCI::GetBandwidthAvailable( void )
{	
    return 0;
}



IOReturn
AppleUSBXHCI::GetBandwidthAvailableForDevice(IOUSBDevice *forDevice, UInt32 *pBandwidthAvailable)
{
	IOReturn				ret = kIOReturnSuccess;
	OSArray *				rhPortArray = OSArray::withCapacity(10);
	RootHubPortTable *		rhTableForPort = NULL;
	SInt16					bandwidthAvailableInBlocks = 0;							// default to 0
	UInt32					bandwidthAvailable = 0;
	UInt8					rhPortSpeed = 0;
	int						slotID = GetSlotID(forDevice->GetAddress());
	Context *				slotContext;
	UInt32					rootHubPort = 0;
	UInt8					deviceSpeed = 0;
	UInt8					controllingPortSpeed;

	if (rhPortArray && slotID)
	{
		slotContext = GetSlotContext(slotID);
		rootHubPort = GetSlCtxRootHubPort(slotContext);
		deviceSpeed = GetSlCtxSpeed(slotContext);

		ret = BuildRHPortBandwidthArray(rhPortArray);
		rhTableForPort = GetRootHubPortTable(rhPortArray, rootHubPort);
		if (rhTableForPort)
		{
			rhPortSpeed = rhTableForPort->rhPortSpeed;
			controllingPortSpeed = rhPortSpeed;
			bandwidthAvailableInBlocks = rhTableForPort->BandwidthAvailable();

			if ((rhPortSpeed == kUSBDeviceSpeedHigh) && (deviceSpeed != kUSBDeviceSpeedHigh))
			{
				if (rhTableForPort->ttArray)
				{
					// FS or LS device on a HS hub
					
					UInt8					slotTTHubSlot = GetSlCtxTTSlot(slotContext);
					UInt8					slotTTHubPort = GetSlCtxTTPort(slotContext);
					bool					slotOnMTTHub = GetSlCtxMTT(slotContext);
					TTBandwidthTable *		ttTable = GetTTBandwidthTable(rhTableForPort->ttArray, slotTTHubSlot, slotTTHubPort, slotOnMTTHub);
					
					if (ttTable)
					{
						USBLog(2, "AppleUSBXHCI::GetBandwidthAvailableForDevice - using ttTable for hubSlot(%d) hubPort(%d) onMTT(%d)", (int)slotTTHubSlot, (int)slotTTHubPort, (int)slotOnMTTHub);
						bandwidthAvailableInBlocks = ttTable->BandwidthAvailable();
						controllingPortSpeed = deviceSpeed;										// this is for the switch statment below
					}
					else
					{
						USBLog(2, "AppleUSBXHCI::GetBandwidthAvailableForDevice - no ttTable for hubSlot(%d) hubPort(%d) onMTT(%d)", (int)slotTTHubSlot, (int)slotTTHubPort, (int)slotOnMTTHub);
					}
				}
				else
				{
					USBLog(2, "AppleUSBXHCI::GetBandwidthAvailableForDevice - no ttArray for rootHubPort (%d)", (int)rootHubPort);
				}
			}
			if ((rhPortSpeed == kUSBDeviceSpeedFull) && (deviceSpeed != kUSBDeviceSpeedFull))
			{
				// FS hub in the root port - LS device downstream
				// switch which speed controlls us, but the bandwidthinblocks is correct
				controllingPortSpeed = deviceSpeed;
			}
		}
	}

	
	if (bandwidthAvailableInBlocks > 0)
	{
		switch (controllingPortSpeed)
		{
			case kUSBDeviceSpeedLow:
				bandwidthAvailable = (bandwidthAvailableInBlocks * kFSBytesPerBlock) / 8;
				break;
				
			case kUSBDeviceSpeedFull:
				bandwidthAvailable = bandwidthAvailableInBlocks * kFSBytesPerBlock;
				break;
				
			case kUSBDeviceSpeedHigh:
				bandwidthAvailable = bandwidthAvailableInBlocks * kHSBytesPerBlock;
				break;
				
			case kUSBDeviceSpeedSuper:
				bandwidthAvailable = bandwidthAvailableInBlocks * kSSBytesPerBlock;
				break;
				
			default:
				bandwidthAvailable = 0;
		}
	}
	else
	{
		bandwidthAvailable = 0;
	}
	
	USBLog(2, "AppleUSBXHCI::GetBandwidthAvailableForRootHubPort - port(%d) portSpeed (%d) deviceSpeed(%d) - bandwidthAvailableInBlocks (%d) - returning (%d)", (int)rootHubPort, rhPortSpeed, deviceSpeed, bandwidthAvailableInBlocks, (int)bandwidthAvailable);
	
	if (rhPortArray)
		rhPortArray->release();

	if (ret == kIOReturnSuccess)
		*pBandwidthAvailable = bandwidthAvailable;

	return ret;
}



