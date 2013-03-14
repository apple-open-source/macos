//
//  AppleUSBXHCI_Bandwidth.h
//  AppleUSBXHCI
//
//  Copyright 2011 Apple Inc. All rights reserved.
//

#ifndef AppleUSBXHCI_AppleUSBXHCI_Bandwidth_h
#define AppleUSBXHCI_AppleUSBXHCI_Bandwidth_h

#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>

#include "AppleUSBXHCIUIM.h"

enum 
{
	kMaxFSIsochInterval				= 18,				
	kMaxFSLSInterruptInterval		= 10,
	kMaxHSSSInterval				= 15,
	kMaxIntervalTableSize			= 15,
	
	// these are Intels numbers of overhead measured in blocks
	kLSPacketOverheadInBlocks		= 128,
	kFSPacketOverheadInBlocks		= 20,
	kHSPacketOverheadInBlocks		= 26,
	kSSInitialOverheadInBlocks		= 32,
	kSSBurstOverheadInBlocks		= 8,
	
	// these value already take into account encoding (for SS and DMI) and bit stuffing (for the others)
	kSSBytesPerBlock				= 16,
	kHSBytesPerBlock				= 4,
	kFSBytesPerBlock				= 1,
	kUplinkDMIBytesPerBlock			= 32,
	
	// these values already take into account the cost of bit stuffing so we can actually use normal MPS
	// instead of adding in the bitstuffing again (Table 3 Section 2.4)
	kLSFSBandwidthLimitInBlocks		= 1156,									// 1285 blocks (including bitstuffing) * 90% (this is per ms)
	kHSBandwidthLimitInBlocks		= 1285,									// 1607 blocks (including bitstuffing) * 80% (this is per uSec)
	kSSBandwidthLimitInBlocks		= 3515									// 3906 blocks * 90% (this is per uFrame)
};



class TTBandwidthTable : public OSObject
{
	OSDeclareDefaultStructors(TTBandwidthTable)
public:
	static		TTBandwidthTable *WithHubAndPort(UInt8 hubSlot, UInt8 hubPort, bool mtt);
	
	void		AddToTable	(UInt8 epSpeed, UInt8 epInterval, UInt16 mps);
	SInt16		BandwidthAvailable(void);
	
	UInt8			hubSlotID;
	UInt8			hubPortNum;
	bool			mtt;
	struct
	{
		UInt8		totalPackets;						// LS packets are converted to FS packets and lumped together
		UInt8		packetOverhead;						// LS packets will force a higher packet overhead once it gets to this interval
		UInt16		worstCaseMPS;						// a LS packet will have to be multipled by 8 when calculating MPS
	} interval[kMaxIntervalTableSize];
};



class RootHubPortTable : public OSObject
{
	OSDeclareDefaultStructors(RootHubPortTable)
public:
	static			RootHubPortTable *WithRHPortAndSpeed(UInt8 rhPort, UInt8 portSpeed);
	void			AddToTable (UInt8 epInterval, UInt16 mps, UInt8 maxBurst, UInt8 mult, UInt8 epSpeed, UInt8 hubSlot, UInt8 hubPort, bool mtt);
	void			PrintTableInfo(void);
	bool			IsBandwidthAcceptable(void);
	SInt16			BandwidthAvailable(void);
	
	virtual void	free(void);
	
	UInt8		rhPort;								// root hub port number (1 based)
	UInt8		rhPortSpeed;						// the speed at which this root hub port is operating
	OSArray		*ttArray;							// keep track of any TT tables which are downstream of this RH
	struct
	{
		UInt8		totalPackets;					// number of packets for this interval
		UInt8		packetOverhead;					// a LS packet on a FS root hub port will force a higher packet overhead once it gets to this interval
		UInt16		worstCaseMPS;					// the worst case MPS for this interval
	} interval[kMaxIntervalTableSize];
};


// helper methods
RootHubPortTable *GetRootHubPortTable(OSArray *rhPortArray, UInt8 rhPort);
TTBandwidthTable *GetTTBandwidthTable(OSArray *ttArray, UInt32 hubSlot, UInt32 hubPort, bool multiTT);




#endif
