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
#include <netinet/in.h>	/* For M_LOOP */
#include <netinet/in_var.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>

#include <sys/socketvar.h>
#include <net/dlil.h>
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

#include "IOFWAsyncStreamRxCommand.h"
#include "IOFireWireIPCommand.h"
#include "ip_firewire.h"
#include "IOFireWireIPDiagnostics.h"
#include "firewire.h"

class IOFireWireNub;
// class IOFWIPAsyncWriteCommand;

typedef UInt32	(*IOTransmitPacket)(mbuf_t m, void * param);
typedef bool	(*IOUpdateARPCache)(void *refcon, IP1394_ARP *fwa);

typedef struct IOFireWireIPPrivateHandlers 
{
	OSObject				*newService;
    IOTransmitPacket		transmitPacket;
	IOUpdateARPCache		updateARPCache;
};

#include "IOFWIPBusInterface.h"

// Should be zero because the broadcast handle has value as "0"
#define kInvalidIPDeviceRefID 0

const UInt32 kUnicastHi = 0x0001;
const UInt32 kUnicastLo = 0x00000000;

const UInt32 kTransmitQueueStalled = 0x3;
const UInt32 kTransmitQueueRestart = 0x1;	

const UInt32 kIOFireWireIPNoResources = 0xe0009001;

// Size of the Pseudo address space
#define MAX_FIFO_SIZE 4096

// Watch dog timeout set to 1 sec = 1000 milli second
#define WATCHDOG_TIMER_MS 1000

#define TRANSMIT_QUEUE_SIZE     256		// Overridden by IORegistry value

#define NETWORK_STAT_ADD(  x )	(fpNetStats->x++)
#define ETHERNET_STAT_ADD( x )	(fpEtherStats->x++)

#define GUID_TYPE 0x1

/*! @defined kIOFWHWAddr
    @abstract kIOFWHWAddr is a property of IOFireWireIP
        objects. It has an OSData value.
    @discussion The kIOFWHWAddr property describes the hardware
        16 byte address containing eui64, maxrec, spd & fifo address */
#define kIOFWHWAddr            "IOFWHWAddr"


//
// defines for channel allocation and reallocation
//
#define kChannelPrime	((UInt32)0x80000000)

enum {
	kNotifyOnSuccess = 0x80000000,
	kDoNotNotifyOnFailure = 0x40000000,
	kDoNotAllocate = 0x20000000,
	kDoNotDeallocate = 0x10000000
};

const int rcbexpirationtime = 60; // 60 seconds active time for reassembly control blocks, decremented by watchdog


// Set this flag to true if you need to copy the payload
const bool kCopyBuffers = false;

// Set this flag to false if you don't need to queue the block write packets 
const bool kQueueCommands = false; 

// Low water mark for commands in the pre-allocated pool
const UInt32 kLowWaterMark = 48;

const int	kDeviceHoldSeconds = (30*60); // lets wait for 30 mins safe !


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
    u_char 					macAddr[FIREWIRE_ADDR_LEN];
    bool					fStarted;	
	IOService				*fPolicyMaker;
	bool					fPacketsQueued;
	UInt32					fActiveCmds;
	UInt32					fInActiveCmds;
	UInt32					fNoCommands;
	UInt32					fNoBCastCommands;
	UInt32					fMissedQRestarts;
	UInt32					fDoubleCompletes;
	UInt32 					fCallErrs;
	UInt32 					fStalls;
	UInt32 					fRxFragmentPkts;
	UInt32 					fTxFragmentPkts;
	UInt16 					fMaxPktSize;
	UInt16 					fMaxInputCount;
	UInt32					fTxBcast;
	UInt32					fRxBcast;	
	UInt32					fTxUni;
	UInt32					fRxUni;;
        
	IOFWSpeed				fPrevBroadcastSpeed;
	bool					fDumpLog;
	OSObject				*fDiagnostics;

	OSObject				*fPrivateInterface;
    IOTransmitPacket		fOutAction;
	IOUpdateARPCache		fUpdateARPCache;

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
	UInt32 				fSubmitErrs;
	UInt32 				fNoResources;
	

	// IOService overrides
    virtual bool		start(IOService *provider);
	virtual void		stop(IOService *provider);
	virtual void		free();
    virtual bool		finalize(IOOptionBits options);
    virtual IOReturn	message(UInt32 type, IOService *provider, void *argument);
	
	
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
	
	virtual IOReturn		enable(IOKernelDebugger * debugger);
	virtual IOReturn		disable(IOKernelDebugger * debugger);
	
	virtual bool			configureInterface(IONetworkInterface *netif);

    virtual void			receivePackets(void * pkt, UInt32 pkt_len, UInt32 options);
    static UInt32			outputPacket(mbuf_t m, void * param);

	virtual	bool			arpCacheHandler(IP1394_ARP *fwa);
	virtual UInt32			transmitPacket(mbuf_t m, void * param);

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

	#pragma mark -
	#pragma mark еее IOFireWireIP defs еее

	void updateMTU(bool onLynx);
	
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

	inline void IOFireWireIP::closeIPGate()
	{
		IORecursiveLockLock(ipLock);
	}

	inline void IOFireWireIP::openIPGate()
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
        @result bool.
	*/
	static bool fwIPUnitAttach(void *target, void *refCon, IOService *newService);

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
	void getBytesFromGUID(void *guid, u_char *bufAddr, UInt8 type);
	
	/*!
		@function makeEthernetAddress
		@abstract constructs mac address from the GUID.
		@param fwuid - GUID of the node.
		@param bufAddr - pointer to the buffer.
		@param vendorID - vendorID.
        @result void.
	*/
	void makeEthernetAddress(CSRNodeUniqueID *fwuid, u_char *bufAddr, UInt32 vendorID);

#ifdef DEBUG	
	void showRcb(RCB *rcb);
	void showArb(ARB *arb);
	void showHandle(TNF_HANDLE *handle);
	void showDrb(DRB *drb);
	void showLcb(); 
#endif
};
#endif // _IOKIT_IOFIREWIREIP_H

