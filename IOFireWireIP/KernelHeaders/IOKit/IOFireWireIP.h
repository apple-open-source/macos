/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <net/netisr.h>
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
#include "ip_firewire.h"
#include "IOIPPort.h"
//#include "IOFWIPEventSource.h"
#include "IOFireWireIPDiagnostics.h"

#include "firewire.h"


class IOFireWireNub;

// Should be zero because the broadcast handle has value as "0"
#define kInvalidIPDeviceRefID 0

// Number of available asynchronous command pool objects 
#define MAX_ASYNC_WRITE_CMDS  127

// Number of available asyncstream command pool objects 
#define MAX_ASYNCSTREAM_TX_CMDS  5

// Available buffer space for mbuf copy : no longer needed, since
// we allocate dynamically based on max rec size 
//
// #define FIREWIRE_CMD_BUFFER	  4096

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

/*!
@class IOFireWireIP
@abstract nub for IP1394 devices
*/
class IOFireWireIP : public IOFWController
{
    OSDeclareDefaultStructors(IOFireWireIP)

friend class IOFireWireIPDiagnostics;

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
	bool					fBuiltin;			// builtin = 1; PCI card = 0
	UInt32					linkStatusPrev;
	UInt16					phyStatusPrev;
    OSDictionary			*mediumDict;

protected:
    IOFireWireNub			*fDevice;
    IOFireWireController	*fControl;
    IOLocalConfigDirectory	*fLocalIP1394ConfigDirectory;
    IOLocalConfigDirectory	*fLocalIP1394v6ConfigDirectory;
	IOFWAsyncStreamRxCommand *fBroadcastReceiveClient;
	OSData					*fwOwnAddr;     // Own hardware address of type IP1394_HDW_ADDR
    IOLock					*ipLock;
    IOWorkLoop				*workLoop;
    IOFWPseudoAddressSpace	*fIP1394AddressSpace;
    FWAddress				fIP1394Address;
    IOCommandPool			*fAsyncCmdPool;
    IOCommandPool			*fAsyncStreamTxCmdPool;
	IOTimerEventSource		*timerSource;
//  IOFWIPEventSource		*ipRxEventSource;
	IONotifier				*fIPUnitNotifier;
	IONotifier				*fIPv6UnitNotifier;
	LCB						*fLcb;
    UInt32					*fMemoryPool;
    struct ifnet			*ifp;
    u_char 					macAddr[FIREWIRE_ADDR_LEN];
	UInt16					unitCount;
    bool					fStarted;	
	UInt32					fMaxRxIsocPacketSize;
	UInt32					fMaxTxAsyncDoubleBuffer;
	bool					bOnLynx;
	IOService				*fPolicyMaker;
	bool					fPacketsQueued;
	IOReturn				fTxStatus;
	UInt16					fUsedCmds;
	UInt16 					fSubmitErrs;
	UInt16 					fCallErrs;
	UInt16 					fStalls;
	UInt16 					fRxFragmentPkts;
	UInt16 					fTxFragmentPkts;
	UInt16 					fMaxPktSize;
	UInt16 					fMaxInputCount;
	UInt16 					fIsoRxOverrun;
	UInt32					fTxBcast;
	UInt32					fRxBcast;	
	UInt32					fTxUni;
	UInt32					fRxUni;;
        
	IOFWSpeed				fPrevBroadcastSpeed;
	bool					fDumpLog;
	OSObject				*fDiagnostics;
	const OSSymbol 			*fDiagnostics_Symbol;
	
	//
	// FIREWIRETODO : will be removed when IOFireWireFamily  has services
	// for reallocating Isoc resources for AsyncStream client
	//
	IOFWReadQuadCommand			*fReadCmd;
	IOFWCompareAndSwapCommand	*fLockCmd;
	FWUnsignedWide				fReAllocateChannel;
	//
	
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

private:			
	// Instance methods:
    bool	transmitPacket(struct mbuf * packet);
    UInt32  outputPacket(struct mbuf * m, void * param);
    void    sendPacket(void * pkt, UInt32 pkt_len);
    
public:

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

    void receivePackets(void * pkt, UInt32 pkt_len, UInt32 options);

	void networkStatAdd(UInt32 *x) const
	{(*x)++;};
	
	IONetworkStats* getNetStats() const
	{return fpNetStats;};

	/*!
		@function watchdog
		@abstract watchdog timer - cleans the Link control block's stale arb's, drb's and rcb's.
		@param timer - IOTimerEventsource.
		@result void.
	*/
	void watchdog(IOTimerEventSource *);
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

	/*!
		@function builtIn
		@abstract checks whether the FWIM is for builtin H/W.
		@param none.
		@result Returns void.
	*/
	void builtIn();

	/*!
		@function initAsyncStreamCmdPool
		@abstract constructs Asyncstream Send command objects and queues them in the pool
		@param none.
        @result Returns kIOReturnSuccess if it was successful, else kIOReturnNoMemory.
	*/
	UInt32 initAsyncStreamCmdPool();

	/*!
		@function initAsyncCmdPool
		@abstract constructs Asynchronous Send command objects and queues them in the pool
		@param none.
        @result Returns kIOReturnSuccess if it was successful, else kIOReturnNoMemory.
	*/
	UInt32 initAsyncCmdPool();
	
	/*!
		@function freeIPCmdPool
		@abstract frees the command objects from the pool
		@param none.
        @result void.
	*/
	void freeIPCmdPool();
	
    /*!
        @function getDevice
		@abstract Returns the FireWire device nub that is this object's provider .
     */
    IOFireWireNub* getDevice() const
	{return fDevice;};

    /*!
		@function getLcb
		@abstract Returns the link control block for the IOLocalNode
    */
    LCB* getLcb() const
    {return fLcb;};
	
    /*!
		@function getUnitCount
		@abstract Returns the number of devices connected to the current LCB
    */	
	UInt32 getUnitCount() const
	{return unitCount;};
	

    /*!
		@function getIPLock
		@abstract Returns lock from the link control block
    */	
	IOLock *getIPLock() const
	{return ipLock;};
		

	/*!
		@function createIPConfigRomEntry
		@abstract creates the config rom entry for IP over Firewire.
		@param 	none
		@result IOReturn - kIOReturnSuccess or error if failure.
	*/
	IOReturn createIPConfigRomEntry();

	/*!
		@function createIPFifoAddress
		@abstract creates the pseudo address space for IP over Firewire.
		@param 	UInt32 fifosize - size of the pseudo address space
		@result IOReturn - kIOReturnSuccess or error if failure.
	*/
	IOReturn createIPFifoAddress(UInt32 fifosize);

	/*!
		@function deviceAttach
		@abstract Callback for a Unit attached of type IP1394
		@param target - callback data.
        @param refcon - callback data.
        @param newService - handle to the new IP1394 unit created.
        @result bool.
	*/
	static bool deviceAttach(void *target, void *refCon, IOService *newService);

	/*!
		@function deviceDetach
		@abstract Callback for a Unit detach of type IP1394
		@param target - callback data.
        @result void.
	*/	
	void deviceDetach(void *target);
	
	/*!
		@function updateBroadcastValues
		@abstract Updates the max broadcast payload and speed  
		@param reset - useful to know whether to start from beginning.
		@result void.
	*/	
	void updateBroadcastValues(bool reset);
	
	/*!
		@function updateLinkStatus
		@abstract Updates the link status based on maxbroadcast speed & payload.  
		@param None.
		@result void.
	*/	
	void updateLinkStatus();


#ifndef FIREWIRETODO
	/*!
		@function nicAttach
		@abstract Attach the IOFireWireIP module to the networking stack during startup.
		@param None.
        @result bool - status true if successful or false if failed.
	*/
	bool nicAttach();
	
	/*!
		@function nicDetach
		@abstract Detach the IOFireWireIP module from the networking stack.
		@param None.
        @result bool - status true if successful or false if failed.
	*/
	bool nicDetach();
#endif

	/*!
		@function allocateCBlk
		@abstract allocates a generic control block
		@param lcb - the firewire link control block for this interface.
        @result returns a preallocated control block
	*/	
	void *allocateCBlk(LCB *lcb);
	
	/*!
		@function deallocateCBlk
		@abstract deallocates a generic control block
		@param lcb - the firewire link control block for this interface.
		@param arb - a control block i.e: arb, rcb or drb.
        @result returns a preallocated control block
	*/	
	void deallocateCBlk(LCB *lcb, void *cBlk);

	/*!
		@function getDeviceID
		@abstract returns a fireWire device object for the GUID
		@param lcb - the firewire link control block for this interface.
		@param eui64 - global unique id of a device on the bus.
		@param itsMac - destination is Mac or not.
		@result Returns IOFireWireNub if successfull else 0.
	*/
	UInt32 getDeviceID(LCB *lcb, UWIDE eui64, BOOLEAN *itsMac);
	
	/*!
		@function getDrbFromEui64
		@abstract Locates the corresponding DRB (device reference block) for GUID
		@param lcb - the firewire link control block for this interface.
        @param eui64 - global unique id of a device on the bus.
        @result Returns DRB if successfull else NULL.
	*/
	DRB *getDrbFromEui64(LCB *lcb, UWIDE eui64);
	
	/*!
		@function getArbFromEui64
		@abstract Locates the corresponding Unicast ARB (Address resolution block) for GUID
		@param lcb - the firewire link control block for this interface.
        @param eui64 - global unique id of a device on the bus.
        @result Returns ARB if successfull else NULL.
	*/
	ARB *getArbFromEui64(LCB *lcb, UWIDE eui64);

	/*! 
		@function getArbFromFwAddr
		@abstract Locates the corresponding Unicast ARB (Address resolution block) for GUID
		@param lcb - the firewire link control block for this interface.
		@param FwAddr - global unique id of a device on the bus.
		@result Returns ARB if successfull else NULL.
	*/
	ARB *getArbFromFwAddr(LCB *lcb, u_char *fwaddr);

	/*!
		@function getDrbFromFwAddr
		@abstract Locates the corresponding DRB (device reference block) for GUID
		@param lcb - the firewire link control block for this interface.
		@param fwaddr - global unique id of a device on the bus.
		@result Returns DRB if successfull else NULL.
	*/
	DRB *getDrbFromFwAddr(LCB *lcb, u_char *fwaddr);
	
	/*!
		@function getDrbFromDeviceID
		@abstract Locates the corresponding DRB (Address resolution block) for IOFireWireNub
		@param lcb - the firewire link control block for this interface.
        @param deviceID - IOFireWireNub to look for.
        @result Returns DRB if successfull else NULL.
	*/
	DRB *getDrbFromDeviceID(LCB *lcb, void *deviceID);
	
	/*!
		@function getMulticastArb
		@abstract Locates the corresponding multicast ARB (Address resolution block) for ipaddress
		@param lcb - the firewire link control block for this interface.
        @param ipAddress - destination ipaddress to send the multicast packet.
        @result Returns ARB if successfull else NULL.
	*/
	ARB *getMulticastArb(LCB *lcb, UInt32 ipAddress);
	
	/*!
		@function getUnicastArb
		@abstract Locates the corresponding unicast ARB (Address resolution block) for ipaddress
		@param lcb - the firewire link control block for this interface.
        @param ipAddress - destination ipaddress to send the unicast packet.
        @result Returns ARB if successfull else NULL.
	*/
	ARB *getUnicastArb(LCB *lcb, UInt32 ipAddress);
	
	/*!
		@function getRcb
		@abstract Locates a reassembly control block.
		@param lcb - the firewire link control block for this interface.
        @param sourceID - source nodeid which generated the fragmented packet.
        @param dgl - datagram label for the fragmented packet.
        @result Returns RCB if successfull else NULL.
	*/
	RCB *getRcb(LCB *lcb, UInt16 sourceID, UInt16 dgl);
	
	/*!
		@function initializeCBlk
		@abstract Initializes the memory for control blocks.
		@param should pass in the size of the control block.
        @result Returns pointer to the control block if successfull else NULL.
	*/
	void* initializeCBlk(UInt32 memorySize);
	
	/*!
		@function linkCBlk
		@abstract generic function to queue a control block to its corresponding list.
		@param queueHead - queuehead of the rcb, arb or drb.
		@param cBlk - control block of type rcb, arb or drb .
        @result void.
	*/
	void linkCBlk(void *queueHead, void *cBlk);

	/*!
		@function unlinkCBlk
		@abstract generic function to dequeue a control block from its corresponding list.
		@param queueHead - queuehead of the rcb, arb or drb.
		@param cBlk - control block of type rcb, arb or drb .
        @result void.
	*/
	void unlinkCBlk(void *queueHead, void *cBlk);

	/*!
		@function rxUnicastFlush
		@abstract Starts the batch processing of the packets, its
				already on its own workloop.
	*/
	void rxUnicastFlush();

	/*!
		@function rxUnicastComplete
		@abstract triggers the indication workloop to do batch processing
					of incoming packets.
	*/
	static void rxUnicastComplete(void *refcon);

	/*!
		@function rxUnicast
		@abstract block write handler
	*/
	static UInt32 rxUnicast(
            void		*refcon,
            UInt16		nodeID,
            IOFWSpeed	&speed,
            FWAddress	addr,
            UInt32		len,
            const void	*buf,
            IOFWRequestRefCon requestRefcon);
			
	/*!
		@function rxAsyncStream
		@abstract callback for an Asyncstream packet, can be both IP or ARP packet.
				This procedure receives an indication when an asynchronous stream
				packet arrives on the default broadcast channel. The packet "should" be GASP,
				but we perform a few checks to make sure. Once we know these are OK, we check
				the etherType field in the unfragmented encapsulation header. This is necessary
				to dispatch the three types of packet that RFC 2734 permits on the default
				broadcast channel: an IPv4 datagram, and ARP request or response or a multi-
				channel allocation protocol (MCAP) message. The only remaining check, for each
				of these three cases, is to make sure that the packet is large enough to hold
				meaningful data. If so, send the packet to another procedure for further
				processing.  
		@param DCLCommandStruct *callProc.
		@result void.
	*/
	static void rxAsyncStream(DCLCommandStruct *dclProgram);
	
	/*!
		@function rxARP
		@abstract ARP processing routine called from both Asynstream path and Async path.
		@param fwIPObj - IOFireWireIP object.
		@param arp - 1394 arp packet without the GASP or Async header.
		@params flags - indicates broadcast or unicast
        @result IOReturn.
	*/
	IOReturn rxARP(IOFireWireIP *fwIPObj, IP1394_ARP *arp, UInt32 flags);
	
	/*!
		@function rxIP
		@abstract Receive IP packet.
		@param fwIPObj - IOFireWireIP object.
		@param pkt - points to the IP packet without the header.
		@param len - length of the packet.
		@params flags - indicates broadcast or unicast	
		@params type - indicates type of the packet IPv4 or IPv6	
		@result IOReturn.
	*/
	IOReturn rxIP(IOFireWireIP *fwIPObj, void *pkt, UInt32 len, UInt32 flags, UInt16 type);
	
	/*!
		@function rxMCAP
		@abstract called from rxAsyncstream for processing MCAP advertisement.
				When an MCAP advertisement is received, parse all of its descriptors 
				looking for any that match group addreses in our MCAP cache. For those that 
				match, update  the channel number (it may have changed from the default
				broadcast channel or since the last advertisement), update the speed 
				(the MCAP owner may have changed the speed requirements as nodes joined or 
				left the group) and refresh the expiration timer so that the MCAP 
				channel is valid for another number of seconds into the future. 
				Th-th-th-that's all, folks!
		@param lcb - the firewire link control block for this interface.
		@param mcapSourceID - source nodeid which generated the multicast advertisement packet.
		@param mcap - mulitcast advertisment packet without the GASP header.
		@param dataSize - size of the packet.
		@result void.
	*/
	void rxMCAP(LCB *lcb, UInt16 mcapSourceID, IP1394_MCAP *mcap, UInt32 dataSize);
	
	/*!
		@function txARP
		@abstract Transmit ARP request or response.
		@param ifp - ifnet pointer.
		@param m - mbuf containing the ARP packet.
        @result void.
	*/
	void txARP(struct ifnet *ifp, struct mbuf *m);
	
	/*!
		@function txIP
		@abstract Transmit IP packet.
		@param ifp - ifnet pointer.
		@param m - mbuf containing the IP packet.
        @result SInt32 - can be EHOSTUNREACH or 0;
	*/
	SInt32 txIP(struct ifnet *ifp, struct mbuf *m, UInt16 type);
	
	/*!
		@function txMCAP
		@abstract multicast solicitation and advertisement messages.
		@param LCB* - link control block
		@param MCB* - mcb - multicast channel control block
		@param ipAddress - address of the multicast group.
        @result void.
	*/
	void txMCAP(LCB *lcb, MCB *mcb, UInt32 ipAddress);
	
	/*!
		@function txCompleteBlockWrite
		@abstract Callback for the Async write complete 
		@param refcon - callback data.
        @param status - status of the command.
        @param device - device.
        @param fwCmd - command object which generated the transaction.
        @result void.
	*/
	static void txCompleteBlockWrite(void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd);

	/*!
		@function txAsyncStreamComplete
		@abstract Callback for the Async stream transmit complete 
		@param refcon - callback data.
        @param status - status of the command.
        @param bus information.
        @param fwCmd - command object which generated the transaction.
        @result void.
	*/
	static void txCompleteAsyncStream(void *refcon, IOReturn status, IOFireWireBus *bus, IOFWAsyncStreamCommand *fwCmd);

	/*!
		@function busReset
		@abstract Does busreset cleanup of the Link control block
		@param lcb - the firewire link control block for this interface.
		@param flags - ignored.
        @result void
	*/	
	void busReset(LCB *lcb, UInt32 flags);
	
	/*!
		@function createDrbWithDevice
		@abstract create device reference block for a device object.
		@param lcb - the firewire link control block for this interface.
		@param eui64 - global unique id of a device on the bus.
		@param fDevObj - IOFireWireNub that has to be linked with the device reference block.
		@param itsMac - Indicates whether the destination is Macintosh or not.
        @result DRB* - pointer to the device reference block.
	*/
	DRB *createDrbWithDevice(LCB *lcb, UWIDE eui64, IOFireWireNub *fDevObj, bool itsMac);
	

	/*!
		@function cleanFWArbCache
		@abstract cleans the Link control block's stale arb's.
		@param lcb - the firewire link control block for this interface.
        @result void.
	*/
	void cleanFWArbCache(LCB *lcb);
	
	/*!
		@function getMTU
		@abstract returns the MTU (Max Transmission Unit) supported by the IOFireWireIP.
		@param None.
        @result UInt32 - MTU value.
	*/
	UInt32 getMTU();
	
	/*!
		@function getMacAddress
		@abstract returns the mac address of size "len".
		@param srcBuf - source buffer that can hold the macaddress.
		@param len - source buffer size.
        @result void* - return the value of destination.
	*/
	void *getMacAddress(char *srcBuf, UInt32 len);
	

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
	
	/*!
		@function setIPAddress
		@abstract sets the ipaddress for the link control block.
		@param in_addr *sip - ipaddress contained in the in_addr structure.
        @result void.
	*/
	void setIPAddress(register struct in_addr *sip);

	/*!
		@function bufferToMbuf
		@abstract Copies buffer to Mbuf.
		@param m - destination mbuf.
		@param offset - offset into the mbuf data pointer.
		@param srcbuf - source buf.
		@param srcbufLen - source buffer length.
		@result bool - true if success else false.
	*/
	bool bufferToMbuf(struct mbuf	*m, 
						UInt32		offset, 
						UInt8		*srcbuf, 
						UInt32		srcbufLen);
									
	/*!
		@function mbufTobuffer
		@abstract Copies mbuf data into the buffer pointed by IOMemoryDescriptor.
		@param src - source mbuf.
		@param offset - offset into the mbuf data pointer.
		@param dstbuf - destination buf.
		@param dstbufLen - destination buffer length.
		@param length - length to copy.
        @result NULL if copied else should be invoked again till 
					the residual is copied into the buffer.
	*/
	struct mbuf *mbufTobuffer(struct mbuf *src, 
								UInt32 *offset, 
								UInt8  *dstbuf, 
								UInt32 dstbufLen, 
								UInt32 length);
	
	/*!
		@function getMBuf
		@abstract Allocate Mbuf of required size.
		@param size - required size for the allocated mbuf.
        @result NULL if failed else a valid mbuf.
	*/
	struct mbuf *getMBuf(UInt32 size);
	
	/*!
		@function freeMBuf
		@abstract free the allocated mbuf.
		@param struct mbuf *m.
        @result void.
	*/
	void freeMBuf(struct mbuf *m);

	bool addNDPOptions(struct mbuf *m);

	void updateNDPCache(void *buf, UInt16 *len);

	IOReturn startAsyncStreamReceiveClients();
	IOReturn stopAsyncStreamReceiveClients();
	
	void showRcb(RCB *rcb);
	void showArb(ARB *arb);
	void showHandle(TNF_HANDLE *handle);
	void showDrb(DRB *drb);
	void showLcb(); 
	
	void updateStatistics();
	
    // Power management methods:
	virtual IOReturn registerWithPolicyMaker(IOService *policyMaker);
	virtual UInt32   maxCapabilityForDomainState(IOPMPowerFlags state);
	virtual UInt32   initialPowerStateForDomainState(IOPMPowerFlags state);
	virtual UInt32   powerStateForDomainState(IOPMPowerFlags state);
	virtual IOReturn setPowerState(UInt32 powerStateOrdinal,IOService *whatDevice);

	#pragma mark -
	#pragma mark еее Firewire IRM defs еее
	UInt32	initIsocMgmtCmds();
	void	freeIsocMgmtCmds();
	UInt32	acquireChannel(UInt32 *pChannel, Boolean autoReAllocate, UInt32 resourceAllocationFlags);
	void	releaseChannel(UInt32 channel,UInt32 releaseFlags);
	void	reclaimChannels();
	void	channelNotification(UInt32 channel, UInt32 flags);
	void	getChan31();

};

#endif // _IOKIT_IOFIREWIREIP_H

