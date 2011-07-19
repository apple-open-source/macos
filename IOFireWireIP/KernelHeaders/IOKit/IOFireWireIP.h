/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_IOFIREWIREIP_H
#define _IOKIT_IOFIREWIREIP_H

extern "C"{
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/dlil.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/kern_event.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <firewire.h>
#include <if_firewire.h>
#include <sys/kpi_mbuf.h>	/* For MBUF_LOOP */
#include <netinet/in_var.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>

#include <sys/socketvar.h>
#include <net/dlil.h>

#include <libkern/version.h>
}

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCommandPool.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOService.h>

#include <IOKit/firewire/IOFWRegs.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOLocalConfigDirectory.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWDCLProgram.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireDevice.h>

#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOFWController.h>
#include <IOFWInterface.h>

#include "IOFireWireIPCommand.h"
#include "IOFWIPDefinitions.h"
#include "IOFireWireIPDiagnostics.h"


class IOFireWireNub;

typedef UInt32	(*IOTransmitPacket)(mbuf_t m, void *param);
typedef bool	(*IOUpdateARPCache)(void *refcon, IP1394_ARP *fwa);
typedef bool	(*IOUpdateMulticastCache)(void *refcon, IOFWAddress *addrs, UInt32 count);

typedef struct IOFireWireIPPrivateHandlers 
{
	OSObject				*newService;
    IOTransmitPacket		transmitPacket;
	IOUpdateARPCache		updateARPCache;
	IOUpdateMulticastCache	updateMulticastCache;
};

#include "IOFWIPBusInterface.h"

const UInt32 kUnicastHi					= 0x0001;
const UInt32 kUnicastLo					= 0x00000000;

const UInt32 kIOFireWireIPNoResources	= 0xe0009001;

#define TRANSMIT_QUEUE_SIZE     256		// Overridden by IORegistry value

#define NETWORK_STAT_ADD(  x )	(fpNetStats->x++)
#define ETHERNET_STAT_ADD( x )	(fpEtherStats->x++)

#define GUID_TYPE				0x1


#define kIOFireWireIPClassKey "IOFireWireIP"

/*! @defined kIOFWHWAddr
    @abstract kIOFWHWAddr is a property of IOFireWireIP
        objects. It has an OSData value.
    @discussion The kIOFWHWAddr property describes the hardware
        16 byte address containing eui64, maxrec, spd & fifo address */
#define kIOFWHWAddr            "IOFWHWAddr"

/*!
@class IOFireWireIP
@abstract nub for IP1394 devices
*/
class IOFireWireIP : public IOFWController
{
    OSDeclareDefaultStructors(IOFireWireIP)

friend class IOFireWireIPDiagnostics;
friend class IOFWIPBusInterface;

// Instance methods:
private:			
    IOFWInterface		    *networkInterface;
	IOBasicOutputQueue		*transmitQueue;
    IOPacketQueue			*debugQueue;
    IONetworkStats			*fpNetStats;
	IOFWStats				*fpEtherStats;
    IOFWAddress				myAddress;
    bool                    isPromiscuous;
    bool					multicastEnabled;
    bool                    isFullDuplex;
	bool                    netifEnabled;
	bool					busifEnabled;
	bool					fBuiltin;			// builtin = 1; PCI card = 0
	UInt32					linkStatusPrev;
	UInt16					phyStatusPrev;
    OSDictionary			*mediumDict;
	bool					fClientStarting;

protected:
    IOFireWireNub			*fDevice;
    IOFireWireController	*fControl;
    IOLocalConfigDirectory	*fLocalIP1394ConfigDirectory;
    IOLocalConfigDirectory	*fLocalIP1394v6ConfigDirectory;
	OSData					*fwOwnAddr;     // Own hardware address of type IP1394_HDW_ADDR
    IORecursiveLock			*ipLock;
    IOWorkLoop				*workLoop;
	IONotifier				*fIPUnitNotifier;
	IONotifier				*fIPv6UnitNotifier;
	LCB						*fLcb;
    UInt8 					macAddr[kIOFWAddressSize];
    bool					fStarted;	
	bool					fPacketsQueued;

	OSObject				*fDiagnostics;

	OSObject				*fPrivateInterface;
    IOTransmitPacket		fOutAction;
	IOUpdateARPCache		fUpdateARPCache;
	IOUpdateMulticastCache	fUpdateMulticastCache;

	const OSSymbol 			*fDiagnostics_Symbol;

    /*! 
		@struct ExpansionData
        @discussion This structure will be used to expand the capablilties of the class in the future.
	*/
    struct ExpansionData { };

    /*! 
		@var reserved
        Reserved for future use.  (Internal use only)  
	*/
    ExpansionData *reserved;

    
public:
	typedef struct  {
		UInt32	fActiveBcastCmds;
		UInt32	fInActiveBcastCmds;
		UInt32	fActiveCmds;
		UInt32	fInActiveCmds;
		UInt32	fNoCommands;
		UInt32	fNoBCastCommands;
		UInt32	fDoubleCompletes;
		UInt32	fCallErrs;
		UInt32	fServiceInOutput;
		UInt32	fServiceInCallback;
		UInt32	fRxFragmentPkts;
		UInt32	fTxFragmentPkts;
		UInt16	fMaxPktSize;
		UInt16	fMaxInputCount;
		UInt32	fTxBcast;
		UInt32	fRxBcast;	
		UInt32	fTxUni;
		UInt32	fRxUni;
		UInt32	fMaxQueueSize;
		UInt32	fLastStarted;
		UInt32	fMaxPacketSize;
		
		UInt32	fGaspTagError;
		UInt32	fGaspHeaderError;
		UInt32	fNonRFC2734Gasp;
		UInt32	fRemoteGaspError;			// not from local bus
		UInt32	fEncapsulationHeaderError;
		UInt32	fNoMbufs;
		UInt32	fInCorrectMCAPDesc;
		UInt32	fUnknownMCAPDesc;
		UInt32	fUnknownGroupAddress;
		UInt32	fSubmitErrs;
		UInt32	fNoResources;
		
		UInt32	activeMbufs;
		UInt32	inActiveMbufs;
		UInt32	fBusyAcks;
		UInt32	fFastRetryBusyAcks;
		UInt32	fDoFastRetry;
		UInt32	fNoRCBCommands;
		UInt32  fRxFragmentPktsDropped;
	}IPoFWDiagnostics;

	IPoFWDiagnostics	fIPoFWDiagnostics;
	
	// IOService overrides
    virtual bool		start(IOService *provider);
	virtual void		stop(IOService *provider);
	virtual void		free();
    virtual bool		finalize(IOOptionBits options);
    virtual IOReturn	message(UInt32 type, IOService *provider, void *argument);
	virtual bool		matchPropertyTable(OSDictionary * table);

	
	#pragma mark -
	#pragma mark еее IOFWController defs еее
    virtual IOReturn 		setMaxPacketSize(UInt32 maxSize);
	virtual IOReturn		getMaxPacketSize(UInt32 * maxSize) const;
	virtual bool			createWorkLoop();
	virtual IOWorkLoop		*getWorkLoop() const;
	virtual IOOutputAction	getOutputHandler() const;

	virtual IOReturn		enable(IONetworkInterface * netif);
	virtual IOReturn		disable(IONetworkInterface * netif);
	
	virtual IOReturn		setWakeOnMagicPacket( bool active );
	virtual IOReturn		getPacketFilters(const OSSymbol	*group, UInt32	*filters ) const;
											
	virtual IOReturn		getHardwareAddress(IOFWAddress *addr);
	
	virtual IOReturn		setMulticastMode(IOEnetMulticastMode mode);
	virtual IOReturn        setMulticastList(IOFWAddress *addrs, UInt32 count);
	virtual IOReturn        setPromiscuousMode(IOEnetPromiscuousMode mode);

	virtual IOOutputQueue	*createOutputQueue();
	
	virtual const OSString	*newVendorString() const;
	virtual const OSString	*newModelString() const;
	virtual const OSString	*newRevisionString() const;
		
	virtual bool			configureInterface(IONetworkInterface *netif);

    virtual void			receivePackets(mbuf_t pkt, UInt32 pkt_len, UInt32 options);
	virtual UInt32			outputPacket(mbuf_t m, void * param);

	virtual	bool			arpCacheHandler(IP1394_ARP *fwa);
	virtual UInt32			transmitPacket(mbuf_t m, void * param);

	virtual bool			multicastCacheHandler(IOFWAddress *addrs, UInt32 count);
	
	void networkStatAdd(UInt32 *x) const
	{(*x)++;};
	
	IONetworkStats* getNetStats() const
	{return fpNetStats;};

	/*!
		@function createMediumState
		@abstract 
		@param none.
		@result create a supported medium information and 
				attach to the IONetworkingFamily.
	*/
	bool createMediumState();

	/*!
		@function getFeatures
		@abstract 
		@param none.
		@result Tell family we can handle multipage mbufs. kIONetworkFeatureMultiPages
	*/
	UInt32 getFeatures() const;

	#pragma mark -
	#pragma mark еее IOFireWireIP defs еее

	UInt32	getMaxARDMAPacketSize();
	UInt8	getMaxARDMARec(UInt32 size);

	void updateMTU(UInt32 mtu);
	
    /*!
        @function getDevice
		@abstract Returns the FireWire device nub that is this object's provider .
     */
    IOFireWireNub* getDevice() const
	{return fDevice;};
	
	IOFireWireController *getController() const
	{return fControl;}; 

    /*!
		@function getLcb
		@abstract Returns the link control block for the IOLocalNode
    */
    LCB* getLcb() const
    {return fLcb;};
	
    /*!
		@function getIPLock
		@abstract Returns lock from the link control block
    */	
	IORecursiveLock *getIPLock() const
	{return ipLock;};

	inline void closeIPoFWGate()
	{
		IORecursiveLockLock(ipLock);
	}

	inline void openIPoFWGate()
	{
		IORecursiveLockUnlock(ipLock);
	}

	/*!
		@function createIPConfigRomEntry
		@abstract creates the config rom entry for IP over Firewire.
		@param 	none
		@result IOReturn - kIOReturnSuccess or error if failure.
	*/
	IOReturn createIPConfigRomEntry();

	/*!
		@function fwIPUnitAttach
		@abstract Callback for a Unit attached of type IP1394
		@param target - callback data.
        @param refcon - callback data.
        @param newService - handle to the new IP1394 unit created.
		@param notifier - handle to the notification request.
        @result bool.
	*/
	static bool fwIPUnitAttach(void *target, void *refCon, IOService *newService, IONotifier * notifier);

	void registerFWIPPrivateHandlers(IOFireWireIPPrivateHandlers *service);

	/*!
		@function deRegisterFWIPPrivateHandlers
		@abstract Callback for a detaching FWIPPrivateHandlers
        @result void.
	*/	
	void deRegisterFWIPPrivateHandlers();

	/*!
		@function getBytesFromGUID
		@abstract constructs byte array from the GUID.
		@param fwuid - GUID of the node.
		@param bufAddr - pointer to the buffer.
		@result void.
	*/	
	void getBytesFromGUID(void *guid, UInt8 *bufAddr, UInt8 type);
	
	/*!
		@function makeEthernetAddress
		@abstract constructs mac address from the GUID.
		@param fwuid - GUID of the node.
		@param bufAddr - pointer to the buffer.
		@param vendorID - vendorID.
        @result void.
	*/
	void makeEthernetAddress(CSRNodeUniqueID *fwuid, UInt8 *bufAddr, UInt32 vendorID);
	
	bool clientStarting();
};

class recursiveScopeLock
{
private:
	IORecursiveLock *fLock;
public:
	recursiveScopeLock(IORecursiveLock *lock){fLock = lock; IORecursiveLockLock(fLock);};
	~recursiveScopeLock(){IORecursiveLockUnlock(fLock);};
};

#endif // _IOKIT_IOFIREWIREIP_H

