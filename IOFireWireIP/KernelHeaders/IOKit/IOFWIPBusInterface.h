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

#ifndef _IOKIT_IOFWIPBUSINTERFACE_H
#define _IOKIT_IOFWIPBUSINTERFACE_H

#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOLib.h>
#include "IOFireWireIP.h"

#define FIREWIREPRIVATE

#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFWAsyncStreamListener.h>


class IOFireWireController;
class IOFWAsyncStreamListener;
class IOFWIPAsyncWriteCommand;

const int		kWaitSecs				= 5;
const int		kUnicastArbs			= 128;
const int		kMulticastArbs			= 64;
const int		kActiveDrbs				= 128;
const int		kActiveRcbs				= 128;
const int		kMaxChannels			= 64;
const int		kMaxAsyncCommands		= 127;
const int		kMaxAsyncStreamCommands = 5;
const int		kRCBExpirationtime		= 2; // 2 seconds active time for reassembly control blocks, decremented by watchdog

const bool		kCopyBuffers			= false; // Set to true if need to copy the payload
const bool		kQueueCommands			= false; // Set to true if need to queue the block write packets 

const UInt32	kLowWaterMark			= 48;	 // Low water mark for commands in the pre-allocated pool
const UInt32	kWatchDogTimerMS		= 1000;  // Watch dog timeout set to 1 sec = 1000 milli second
const UInt32	kMaxPseudoAddressSize	= 4096;

// BusyX Ack workaround to maximize IPoFW performance
const UInt32 kMaxBusyXAcksPerSecond				= 10;
const UInt32 kMaxSecondsToTurnOffFastRetry		= 60;

class IOFWIPMBufCommand : public IOCommand
{
	OSDeclareDefaultStructors(IOFWIPMBufCommand);
	
private:
    mbuf_t			fMbuf;
    IOFireWireIP	*fIPLocalNode;
	bool			fInited;
	IOReturn		fStatus;
	IOCommandPool	*fPool;

protected:
	void free();
	
public:
	bool init();
	void reinit(mbuf_t pkt, IOFireWireIP *ipNode, IOCommandPool *pool);
	void releaseWithStatus(IOReturn status = kIOReturnSuccess);
	mbuf_t getMBuf();
};

/*!
@class IOFWIPBusInterface
@abstract Object to seperate IP over FireWire's firewire services.
*/
class IOFWIPBusInterface : public IOService
{
    OSDeclareDefaultStructors(IOFWIPBusInterface)

private:
	IOFireWireIP			*fIPLocalNode;	
	IOFireWireController	*fControl;
	LCB						*fLcb;

    IOFWPseudoAddressSpace	*fIP1394AddressSpace;
    FWAddress				fIP1394Address;
	bool					fStarted;
	
    IOCommandPool			*fAsyncCmdPool;
    IOCommandPool			*fMbufCmdPool;
	IOCommandPool			*fRCBCmdPool;
	OSSet					*fAsyncTransitSet;
    IOCommandPool			*fAsyncStreamTxCmdPool;
	OSSet					*fAsyncStreamTransitSet;
	UInt32					fMaxTxAsyncDoubleBuffer;
	IORecursiveLock			*fIPLock;
    IOWorkLoop				*workLoop;
	
	OSSet					*unicastArb;        // Address information from ARP
	OSSet					*multicastArb;      // Address information from MCAP
    OSSet					*activeDrb;         // Devices with valid device IDs
    OSSet					*activeRcb;         // Linked list of datagrams in reassembly
    OSArray					*mcapState;			// Per channel MCAP descriptors
	IOTimerEventSource		*timerSource;
	SInt16					fUnitCount;
	UInt32					fLowWaterMark;
	UInt32					fPrevTransmitCount;
	UInt32					fPrevBusyAcks;
	UInt32					fPrevFastRetryBusyAcks;
	UInt8					fFastRetryUnsetTimer;
	int						fCurrentAsyncIPCommands;
	int						fCurrentMBufCommands;
	int						fCurrentRCBCommands;
	UInt32					fOptimalMTU;
	
protected:	
	IOFWAsyncStreamListener	*fBroadcastReceiveClient;

	// Instance methods:
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
	// IOService overrides
	bool		init(IOFireWireIP *provider);

	bool		finalize(IOOptionBits options);

	void		stop(IOService *provider);

	void		free();

	IOReturn	message(UInt32 type, IOService *provider, void *argument);

	void		processWatchDogTimeout();

	bool		attachIOFireWireIP(IOFireWireIP *provider);

	void		detachIOFireWireIP();
	
	/*!
		@function calculateMaxTransferUnit
		@abstract checks whether the FWIM is for builtin H/W.
		@param none.
		@result Returns void.
	*/
	void	calculateMaxTransferUnit();

	/*!
		@function createIPFifoAddress
		@abstract creates the pseudo address space for IP over Firewire.
		@param 	UInt32 fifosize - size of the pseudo address space
		@result IOReturn - kIOReturnSuccess or error if failure.
	*/
	IOReturn createIPFifoAddress(UInt32 fifosize);
	
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
		
	IOFWIPMBufCommand *getMBufCommand();
		
	IOFWIPAsyncWriteCommand	*getAsyncCommand(bool block, bool *deferNotify);
	
	void	returnAsyncCommand(IOFWIPAsyncWriteCommand *cmd);
	
	/*!
		@function freeIPCmdPool
		@abstract frees the command objects from the pool
		@param none.
        @result void.
	*/
	void	freeAsyncCmdPool();

	/*!
		@function freeAsyncStreamCmdPool
		@abstract frees the command objects from the pool
		@param none.
        @result void.
	*/
	void	freeAsyncStreamCmdPool();

	ARB		*updateARBwithDevice(IOFireWireNub *device, UWIDE eui64);

	DRB		*initDRBwithDevice(UWIDE eui64, IOFireWireNub *fDevObj, bool itsMac);

	static	DRB	*staticInitDRBwithDevice(void *refcon, UWIDE eui64, IOFireWireNub *fDevObj, bool itsMac);

    /*!
		@function incrementUnitCount
		@abstract Increments the unit count
    */	
	void incrementUnitCount();
	
    /*!
		@function decrementUnitCount
		@abstract Decrements the unit count
    */	
	void decrementUnitCount();

	SInt16 getUnitCount();

	/*!
		@function fwIPUnitAttach
		@abstract Callback for a Unit Attach of type IPv4 or IPv6
        @result void.
	*/	
	void fwIPUnitAttach();
	
	/*!
		@function fwIPUnitTerminate
		@abstract Callback for a Unit detach of type IPv4 or IPv6
        @result void.
	*/	
	void fwIPUnitTerminate();
	
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
	
	UInt32	getMTU();

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
		@function txARP
		@abstract Transmit ARP request or response.
		@param ifp - ifnet pointer.
		@param m - mbuf containing the ARP packet.
		@param nodeID - our node id.
        @result void.
	*/
	SInt32  txARP(mbuf_t m, UInt16 nodeID, UInt32 busGeneration, IOFWSpeed speed);
	
	SInt32	txBroadcastIP(const mbuf_t m, UInt16 nodeID, UInt32 busGeneration, UInt16 ownMaxPayload, UInt16 maxBroadcastPayload, IOFWSpeed speed, const UInt16 type, UInt32 channel);
	
	SInt32	txUnicastUnFragmented(IOFireWireNub *device, const FWAddress addr, const mbuf_t m, const UInt16 pktSize, const UInt16 type);
	
	SInt32	txUnicastFragmented(IOFireWireNub *device, const FWAddress addr, const mbuf_t m, 
												const UInt16 pktSize, const UInt16 type, UInt16 maxPayload, UInt16 dgl);
	
	SInt32	txUnicastIP(mbuf_t m, UInt16 nodeID, UInt32 busGeneration, UInt16 ownMaxPayload, IOFWSpeed speed,const UInt16 type);
	
	UInt32	outputPacket(mbuf_t pkt, void * param);
	
	static  UInt32	staticOutputPacket(mbuf_t pkt, void * param);

	IOTransmitPacket		getOutputHandler() const;

	IOUpdateARPCache		getARPCacheHandler() const;
	
	IOUpdateMulticastCache	getMulticastCacheHandler() const;
	
	/*!
		@function txIP
		@abstract Transmit IP packet.
		@param m - mbuf containing the IP packet.
        @result SInt32 - can be EHOSTUNREACH or 0;
	*/
	SInt32 txIP(mbuf_t m, UInt16 nodeID, UInt32 busGeneration, UInt16 ownMaxPayload, UInt16 maxBroadcastPayload, IOFWSpeed speed, UInt16 type);
	
	IOReturn createAsyncStreamRxClient(UInt8 speed, UInt32 channel, MCB *mcb);

	/*!
		@function txMCAP
		@abstract multicast solicitation and advertisement messages.
		@param MCB*			- mcb - multicast channel control block
		@param ipAddress	- address of the multicast group.
        @result void.
	*/
	void txMCAP(MCB *mcb, UInt32 groupAddress);
	
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
	static UInt32	rxUnicast(void *refcon, UInt16 nodeID, IOFWSpeed	&speed, FWAddress addr, UInt32	len, const void *buf, IOFWRequestRefCon requestRefcon);
	
	IOReturn rxFragmentedUnicast(UInt16 nodeID, IP1394_FRAG_HDR *pkt, UInt32 len);
	
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
	static void rxAsyncStream(void *refcon, const void *buf);
	
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
		@function rxIP
		@abstract Receive IP packet.
		@param fwIPObj - IOFireWireIP object.
		@param pkt - points to the IP packet without the header.
		@param len - length of the packet.
		@params flags - indicates broadcast or unicast	
		@params type - indicates type of the packet IPv4 or IPv6	
		@result IOReturn.
	*/
	IOReturn rxIP(void *pkt, UInt32 len, UInt32 flags, UInt16 type);
	
	/*!
		@function rxARP
		@abstract ARP processing routine called from both Asynstream path and Async path.
		@param fwIPObj - IOFireWireIP object.
		@param arp - 1394 arp packet without the GASP or Async header.
		@params flags - indicates broadcast or unicast
        @result IOReturn.
	*/
	IOReturn rxARP(IP1394_ARP *arp, UInt32 flags);

	static bool staticUpdateARPCache(void *refcon, IP1394_ARP *fwa);

	static bool staticUpdateMulticastCache(void *refcon, IOFWAddress *addrs, UInt32 count);
	
	bool updateARPCache(IP1394_ARP *fwa);

	bool wellKnownMulticastAddress(IOFWAddress *addr);

	bool updateMulticastCache(IOFWAddress *addrs, UInt32 count);

	/*!
		@function getRcb
		@abstract Locates a reassembly control block.
		@param lcb - the firewire link control block for this interface.
        @param sourceID - source nodeid which generated the fragmented packet.
        @param dgl - datagram label for the fragmented packet.
        @result Returns RCB if successfull else NULL.
	*/
	RCB *getRcb(UInt16 sourceID, UInt16 dgl);

	RCB *getRCBCommand( UInt16 sourceID, UInt16 dgl, UInt16 etherType, UInt16 datagramSize, mbuf_t m );
	
	/*!
		@function getMulticastArb
		@abstract Locates the corresponding multicast ARB (Address resolution block) for ipaddress
		@param lcb			- the firewire link control block for this interface.
        @param groupAddress - destination ipaddress to send the multicast packet.
        @result Returns ARB if successfull else NULL.
	*/
	MARB *getMulticastArb(UInt32 groupAddress);

	/*!
		@function getDrbFromDeviceID
		@abstract Locates the corresponding DRB (Address resolution block) for IOFireWireNub
		@param lcb - the firewire link control block for this interface.
        @param deviceID - IOFireWireNub to look for.
        @result Returns DRB if successfull else NULL.
	*/
	DRB *getDrbFromDeviceID(void *deviceID);			

	bool addNDPOptions(mbuf_t m);

    bool updateNDPCache(mbuf_t m);

	void updateNDPCache(void *buf, UInt16 *len);

	/*!
		@function getARBFromEui64
		@abstract Locates the corresponding Unicast ARB (Address resolution block) for GUID
        @param eui64 - global unique id of a device on the bus.
        @result Returns ARB if successfull else NULL.
	*/
	ARB *getARBFromEui64(UWIDE eui64);

	static ARB *staticGetARBFromEui64(void *refcon, UWIDE eui64);

	/*!
		@function getDeviceID
		@abstract returns a fireWire device object for the GUID
		@param lcb - the firewire link control block for this interface.
		@param eui64 - global unique id of a device on the bus.
		@param itsMac - destination is Mac or not.
		@result Returns IOFireWireNub if successfull else 0.
	*/
	void* getDeviceID(UWIDE eui64, bool *itsMac);

	/*!
		@function getDrbFromEui64
		@abstract Locates the corresponding DRB (device reference block) for GUID
		@param lcb - the firewire link control block for this interface.
        @param eui64 - global unique id of a device on the bus.
        @result Returns DRB if successfull else NULL.
	*/
	DRB *getDrbFromEui64(UWIDE eui64);


	/*!
		@function getDrbFromFwAddr
		@abstract Locates the corresponding DRB (device reference block) for GUID
		@param lcb - the firewire link control block for this interface.
		@param fwaddr - global unique id of a device on the bus.
		@result Returns DRB if successfull else NULL.
	*/
	DRB *getDrbFromFwAddr(UInt8 *fwaddr);
	
	/*! 
		@function getArbFromFwAddr
		@abstract Locates the corresponding Unicast ARB (Address resolution block) for GUID
		@param FwAddr - global unique id of a device on the bus.
		@result Returns ARB if successfull else NULL.
	*/
	ARB *getArbFromFwAddr(UInt8 *fwaddr);

	static ARB *staticGetArbFromFwAddr(void *refcon, UInt8 *fwaddr);

	/*!
		@function getUnicastArb
		@abstract Locates the corresponding unicast ARB (Address resolution block) for ipaddress
        @param ipAddress - destination ipaddress to send the unicast packet.
        @result Returns ARB if successfull else NULL.
	*/
	ARB *getUnicastArb(UInt32 ipAddress);
	
	/*!
		@function cleanRCBCache
		@abstract cleans the Link control block's stale rcb's. UnAssembled RCB's
					are returned to the free CBLKs
		@param none.
		@result void.
	*/
	void cleanRCBCache();

	void releaseRCB(RCB	*rcb, bool freeMbuf = true);

	void resetMARBCache();
	
	void resetRCBCache();
	
	void resetMcapState();
	
	void releaseDRB(UInt8 *fwaddr);

	void releaseDRB(UInt32 deviceID);

	void releaseARB(UInt8 *fwaddr);

	void updateMcapState();
	
	void releaseMulticastARB(MCB *mcb);
	
    mbuf_t allocateMbuf(UInt32 size);
	
	/*!
		@function bufferToMbuf
		@abstract Copies buffer to Mbuf.
		@param m - destination mbuf.
		@param offset - offset into the mbuf data pointer.
		@param srcbuf - source buf.
		@param srcbufLen - source buffer length.
		@result bool - true if success else false.
	*/
	bool bufferToMbuf(mbuf_t		m, 
					  UInt32		offset, 
					  vm_address_t	*srcbuf, 
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
	mbuf_t mbufTobuffer(const mbuf_t	src, 
						UInt32			*offset, 
						vm_address_t	*dstbuf, 
						UInt32			dstbufLen, 
						UInt32			length);
						
	void moveMbufWithOffset(SInt32 tempOffset, mbuf_t *srcm, vm_address_t *src, SInt32 *srcLen);	

#ifdef DEBUG
	void showRcb(RCB *rcb);
	void showMinRcb(RCB *rcb);
	void showArb(ARB *arb);
	void showHandle(TNF_HANDLE *handle);
	void showDrb(DRB *drb);
	void showLcb(); 
#endif
};

#endif 
