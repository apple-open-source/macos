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
#include "IOFireWireIP.h"
#include "IOFireWireIPUnit.h"
#include "IOFireWireIPCommand.h"
#include "IOFWAsyncStreamRxCommand.h"

extern "C"
{
struct mbuf * m_getpackets(int num_needed, int num_with_pkthdrs, int how);
void _logMbuf(struct mbuf * m);
void _logPkt(void *pkt, UInt16 len);
void moveMbufWithOffset(SInt32 tempOffset, struct mbuf **srcm, vm_address_t *src, SInt32 *srcLen);
}

#define super IOFWController

struct {
   UCHAR specifierID[3];      // 24-bit RID
   UCHAR version[3];          // 24-bit version
} gaspVal = { {0x00, 0x00, 0x5E}, {0x00, 0x00, 0x01} };

static u_char multicast[3] = {0x01, 0x00, 0x5e};
static u_char fwbroadcastaddr[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

// FireWire bus has two power states, off and on
#define kNumOfPowerStates 2

#define BCOPY(s, d, l) do { bcopy((void *) s, (void *) d, l); } while(0)

// Note: This defines two states. off and on.
static IOPMPowerState ourPowerStates[kNumOfPowerStates] = {
  {1,0,0,0,0,0,0,0,0,0,0,0},
  {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

extern funnel_t * network_flock;

#pragma mark -
#pragma mark еее copy & queue flags еее

// Set this flag to true if you need to copy the payload
bool gDoCopy = false;

// Set this flag to false if you don't need to queue the block write packets 
bool gDoQueue = false; 

// Low water mark for commands in the pre-allocated pool
UInt16 gLowWaterMark = 48;

OSDefineMetaClassAndStructors(IOFireWireIP, IOFWController);

#pragma mark -
#pragma mark еее IOService methods еее

bool IOFireWireIP::start(IOService *provider)
{    
    IOReturn	ioStat = kIOReturnSuccess;
	
    fDevice = OSDynamicCast(IOFireWireNub, provider);

    if(!fDevice)
        return false;

    fControl = fDevice->getController(); 

	if(!fControl)
		return false;

	// Does calculate the fMaxTxAsyncDoubleBuffer & fAsyncRxIsocPacketSize;
    builtIn();
	
    // Initialize the LCB
    fLcb = (LCB*)IOMalloc(sizeof(LCB));
    if(fLcb == NULL)
        return false;
    
    memset(fLcb, 0, sizeof(LCB));

	// Initialize the control blocks memory area
    if(!initializeCBlk(CBLK_MEMORY_SIZE))
        return false;

	fDiagnostics_Symbol = OSSymbol::withCStringNoCopy("Diagnostics");
	
	fDiagnostics = IOFireWireIPDiagnostics::createDiagnostics(this);
	if( fDiagnostics )
	{
		if(fDiagnostics_Symbol)
			setProperty( fDiagnostics_Symbol, fDiagnostics );
		fDiagnostics->release();
	}

	if(ioStat == kIOReturnSuccess) {
	    
		CSRNodeUniqueID	fwuid = fDevice->getUniqueID();
		
		// Construct the ethernet address
		makeEthernetAddress(&fwuid, macAddr, GUID_TYPE);
		// getBytesFromGUID(&fwuid, macAddr, GUID_TYPE);
	}

	// IONetworkingFamily attachments
    if (!super::start(provider))
        return false;
	
    if (getHardwareAddress(&myAddress) != kIOReturnSuccess)
    {	
        return false;
    }

    if(!createMediumState())
	{
		IOLog( "IOFireWireIP::start - Couldn't allocate IONetworkMedium\n" );
		return false;
	}
	
    if (!attachInterface((IONetworkInterface**)&networkInterface, false ))
    {	
        return false;
    }

	// Try setting the dynamic MTU
	if(fDevice != NULL)
	{
		if(bOnLynx == false)
		{
			networkInterface->setIfnetMTU( 1 << fDevice->maxPackLog(true) );
		}
		else
		{
			// If on Lynx, we assume to do only 512 as the MTU
			networkInterface->setIfnetMTU( 1 << 9 );
		}
	}
	
	transmitQueue = (IOGatedOutputQueue*)getOutputQueue();
    if ( !transmitQueue ) 
    {
        IOLog( "IOFireWireIP::start - Output queue initialization failed\n" );
        return false;
    }
    transmitQueue->retain();

	// Create the lock
	if(ioStat == kIOReturnSuccess) {
		// Allocate lock
		ipLock = IOLockAlloc();
		
		if(ipLock == NULL)
			ioStat = kIOReturnNoMemory;
		else
			IOLockInitWithState(ipLock, kIOLockStateUnlocked);
	}
	
	if(ioStat == kIOReturnSuccess) {
		// Allocate Timer event source
		timerSource = IOTimerEventSource::timerEventSource(
														this,
														(IOTimerEventSource::Action)&IOFireWireIP::watchdog);
		if (timerSource == NULL)
		{
			IOLog( "IOFireWireIP::start - Couldn't allocate timer event source\n" );
			return false;
		}
		if (getWorkLoop()->addEventSource(timerSource) != kIOReturnSuccess )
		{
			IOLog( "IOFireWireIP::start - Couldn't add timer event source\n" );        
			return false;
		}
		
#ifdef FIREWIRETODO
		// Allocate a IP receive event source
		ipRxEventSource = new IOFWIPEventSource;
		if(ipRxEventSource == NULL)
		{
			IOLog( "IOFireWireIP::start - Couldn't allocate ipRx event source\n" );
			return false;
		}
		if(ipRxEventSource->init(this) == false)
		{
			IOLog( "IOFireWireIP::start - Couldn't init ipRx event source\n" );
			return false;
		}
		if (getWorkLoop()->addEventSource(ipRxEventSource) != kIOReturnSuccess )
		{
			IOLog( "IOFireWireIP::start - Couldn't add ipRx event source\n" );        
			return false;
		}
#endif
	}

	if(ioStat == kIOReturnSuccess) {
		// Add unit notification for units disappearing
		fIPUnitNotifier = IOService::addNotification(gIOPublishNotification, 
													serviceMatching("IOFireWireIPUnit"), 
													&deviceAttach, this, (void*)IP1394_VERSION, 0);
	}

	if(ioStat == kIOReturnSuccess) {
		// Add unit notification for units disappearing
		fIPv6UnitNotifier = IOService::addNotification(gIOPublishNotification, 
													serviceMatching("IOFireWireIPUnit"), 
													&deviceAttach, this, (void*)IP1394v6_VERSION, 0);
	}

	// Asyncstream hook up to recieve the broadcast packets
	if(ioStat == kIOReturnSuccess) {
		fBroadcastReceiveClient = new IOFWAsyncStreamRxCommand;
		if(fBroadcastReceiveClient == NULL) {
			ioStat = kIOReturnNoMemory;
		}
		else
		{
			if(fBroadcastReceiveClient->initAll(0x1f, rxAsyncStream, fControl, fMaxRxIsocPacketSize, this) == false) {
				ioStat = kIOReturnNoMemory;
			}
		}
	}

	// Create pseudo address space
	if(ioStat == kIOReturnSuccess)
		ioStat = createIPFifoAddress(MAX_FIFO_SIZE);

	// Create config rom entry
	if(ioStat == kIOReturnSuccess)
		ioStat = createIPConfigRomEntry();

	if(ioStat == kIOReturnSuccess)
		initIsocMgmtCmds();

	if(ioStat == kIOReturnSuccess) {
		// Fill information about the IOFireWireLocalNode
		fLcb->ownHandle.deviceID = (UInt32)fDevice;
		fLcb->ownHandle.maxRec = fDevice->maxPackLog(true);       // Maximum asynchronous payload
		fLcb->ownHandle.spd = fDevice->FWSpeed();                 // Maximum speed 
		fLcb->ownHandle.unicastFifoHi = fIP1394Address.addressHi; // Upper 16 bits of unicast FIFO address
		fLcb->ownHandle.unicastFifoLo = fIP1394Address.addressLo; // Lower 32 bits of unicast FIFO address
  
		// fix to enable the arp/dhcp support from network pref pane
		setProperty(kIOFWHWAddr,  (void *) &fLcb->ownHardwareAddress, sizeof(IP1394_HDW_ADDR));
						  
		fDevice->getNodeIDGeneration(fLcb->busGeneration, fLcb->ownNodeID); 
		fLcb->ownMaxSpeed = fDevice->FWSpeed();
		fLcb->maxBroadcastPayload = fDevice->maxPackLog(true);
		fLcb->maxBroadcastSpeed = fDevice->FWSpeed();
		fLcb->ownMaxPayload = fDevice->maxPackLog(true);
	}

	if(ioStat != kIOReturnSuccess)
	{
		IOLog( "IOFireWireIP::start - failed\n" );
		return false;
	}

	// might eventually start the timer
	timerSource->setTimeoutMS(WATCHDOG_TIMER_MS);

	fStarted = true;

	// Set the ifnet's softc to this pointer, useful in if_firewire.cpp
	networkInterface->setIfnetSoftc(this);
	
    networkInterface->registerService();

    return true;
} // end start

bool IOFireWireIP::finalize(IOOptionBits options)
{
	
	return super::finalize(options);
}

void IOFireWireIP::stop(IOService *provider)
{
	if(fDiagnostics_Symbol != NULL)
	{
		fDiagnostics_Symbol->release();		
		fDiagnostics_Symbol = 0;
	}

	if(fPolicyMaker != NULL)
		fPolicyMaker->deRegisterInterestedDriver(this);

    if (ipLock != NULL) 
	{
        IOLockFree(ipLock);
    }

	freeIsocMgmtCmds();

    // Free the firewire stuff
    if (fLocalIP1394v6ConfigDirectory != NULL){
        // clear the unit directory in config rom
        fDevice->getBus()->RemoveUnitDirectory(fLocalIP1394v6ConfigDirectory) ;
		fLocalIP1394v6ConfigDirectory->release();
	} 

    // Free the firewire stuff
    if (fLocalIP1394ConfigDirectory != NULL){
        // clear the unit directory in config rom
        fDevice->getBus()->RemoveUnitDirectory(fLocalIP1394ConfigDirectory) ;
		fLocalIP1394ConfigDirectory->release();
	} 

	if(fwOwnAddr != NULL)
		fwOwnAddr->release();
	
    if (fIP1394AddressSpace != NULL){
        fIP1394AddressSpace->deactivate();
        fIP1394AddressSpace->release();
		fIP1394AddressSpace = NULL;
    }

	// Release the Asyncstream receive broadcast client
	if(fBroadcastReceiveClient != NULL)
	{
		fBroadcastReceiveClient->release();
		fBroadcastReceiveClient = NULL;
	}
	
    freeIPCmdPool();


	if (transmitQueue != NULL)
	{	
		transmitQueue->release();
	}
		
	if(timerSource != NULL) 
	{
		if (workLoop != NULL)
		{
			workLoop->removeEventSource(timerSource);
		}
		timerSource->release();
	}

#ifdef FIREWIRETODO
	if(ipRxEventSource != NULL)
	{
		ipRxEventSource->release();
	}
#endif

	// Remove IOFireWireIPUnit notification
	if(fIPv6UnitNotifier != NULL)
	{
		fIPv6UnitNotifier->remove();
		fIPv6UnitNotifier = NULL;
	}
	
	// Remove IOFireWireIPUnit notification
	if(fIPUnitNotifier != NULL)
	{
		fIPUnitNotifier->remove();
		fIPUnitNotifier = NULL;
	}
	
    if(fLcb != NULL)
        IOFree(fLcb, sizeof(LCB));

    // Clear the memory area for the CBLK's
    if(fMemoryPool != NULL)
        IOFree(fMemoryPool, CBLK_MEMORY_SIZE);

	if (networkInterface != NULL)
	{
        //detachInterface(networkInterface,true);
		networkInterface->release();
	}

	super::stop(provider);
}

void IOFireWireIP::free(void)
{
	return super::free();
}

IOReturn IOFireWireIP::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn res = kIOReturnUnsupported;

    if( kIOReturnUnsupported == res )
    {
        switch (type)
        {                
            case kIOMessageServiceIsTerminated:
                res = kIOReturnSuccess;
				//IOLog(" IOFireWireIP: terminated\n");
                break;

            case kIOMessageServiceIsSuspended:
                res = kIOReturnSuccess;
                if(fStarted == true)
                {
                    stopAsyncStreamReceiveClients();
                }
				//IOLog(" IOFireWireIP: suspended\n");
                break;

            case kIOMessageServiceIsResumed:
                res = kIOReturnSuccess;
                // Busreset function here for handling busreset
                if(fStarted == true)
                {
					busReset(fLcb, 0);
                    
                    startAsyncStreamReceiveClients();
				}
				//IOLog(" IOFireWireIP: resumed\n");
				break;
				
			case kIOMessageServiceIsRequestingClose:
				res = kIOReturnSuccess;
				//IOLog(" IOFireWireIP: kIOMessageServiceIsRequestingClose\n");
				break;
				
            default: // default the action to return kIOReturnUnsupported
				//IOLog(" IOFireWireIP: default %lX\n", type);
                break;
        }
    }
	
	messageClients(type);

    return res;
}

#pragma mark -
#pragma mark еее IOFWController methods еее

IOReturn IOFireWireIP::setMaxPacketSize(UInt32 maxSize)
{

	if (maxSize > kIOFWMaxPacketSize)
	{
		return kIOReturnError;
	}
	
//  IOLog("FireWire: setMaxPacketSize size called\n");

    return kIOReturnSuccess;
}

IOReturn IOFireWireIP::getMaxPacketSize(UInt32 * maxSize) const
{
//	IOLog("IOFireWireIP: getmaxpacket size called\n");
	*maxSize = kIOFWMaxPacketSize;

	return kIOReturnSuccess;
}

IOReturn IOFireWireIP::getHardwareAddress(IOFWAddress *ea)
{
	ea->bytes[0] = macAddr[0];
	ea->bytes[1] = macAddr[1];
	ea->bytes[2] = macAddr[2];
	ea->bytes[3] = macAddr[3];
	ea->bytes[4] = macAddr[4];
	ea->bytes[5] = macAddr[5];
	ea->bytes[6] = macAddr[6];
	ea->bytes[7] = macAddr[7];

    return kIOReturnSuccess;
} // end getHardwareAddress


bool IOFireWireIP::configureInterface( IONetworkInterface *netif )
{
    IONetworkData	 *nd;

	//IOLog("IOFireWireIP: configureInterface - start\n");

    if ( super::configureInterface( netif ) == false )
        return false;

	/* Grab a pointer to the statistics structure in the interface:	*/
	nd = netif->getNetworkData( kIONetworkStatsKey );
    if (!nd || !(fpNetStats = (IONetworkStats *) nd->getBuffer()))
    {
        IOLog("IOFireWireIP: invalid network statistics\n");
        return false;
    }

	// Get the Ethernet statistics structure:
	nd = netif->getParameter( kIOFWStatsKey );
	if ( !nd || !(fpEtherStats = (IOFWStats*)nd->getBuffer()) )
	{
		IOLog( "IOFireWireIP::configureInterface - invalid ethernet statistics\n" );
        return false;
	}

	//IOLog("IOFireWireIP: configureInterface - end\n");

    /*
     * Set the driver/stack reentrancy flag. This is meant to reduce
     * context switches. May become irrelevant in the future.
     */
    return true;
	
} // end configureInterface

void IOFireWireIP::receivePackets(void * pkt, UInt32 pkt_len, UInt32 options)
{
	if(options == true)
		fPacketsQueued = true;

    IOTakeLock(ipLock);
		
	networkInterface->inputPacket((struct mbuf*)pkt, pkt_len, options);
	networkStatAdd(&fpNetStats->inputPackets);
	
    IOUnlock(ipLock);
}

IOOutputQueue* IOFireWireIP::createOutputQueue()
{
	return IOGatedOutputQueue::withTarget(this, getWorkLoop(), TRANSMIT_QUEUE_SIZE);
}

/*-------------------------------------------------------------------------
 * Override IONetworkController::createWorkLoop() method and create
 * a workloop.
 *-------------------------------------------------------------------------*/
bool IOFireWireIP::createWorkLoop()
{
	workLoop = fDevice->getController()->getWorkLoop();
	
    return ( workLoop != 0 );
} // end createWorkLoop


// Override IOService::getWorkLoop() method to return our workloop.
IOWorkLoop* IOFireWireIP::getWorkLoop() const
{
	//IOLog("IOFireWireIP: getWorkLoop - end %p \n", workLoop);

    return workLoop;
} // end getWorkLoop 

UInt32 IOFireWireIP::outputPacket(struct mbuf * pkt, void * param)
{
	// Do the branching between ARP/IPV4/IPV6
	register struct firewire_header *fwh;
	int	status = kIOReturnSuccess;
	UInt16	type;
	BOOLEAN	bRet = false;

	fwh = mtod(pkt, struct firewire_header*);
	type = htons(fwh->ether_type);
	
	switch(type)
	{
		case ETHERTYPE_IPV6:
			bRet = addNDPOptions(pkt);
			if(bRet == true)
			{
				// IOLog("options updated\n"); 
			}
		case ETHERTYPE_IP:
			status = txIP(ifp, pkt, type);
			break;

		case ETHERTYPE_ARP:
			txARP(ifp, pkt);
		
		default :
			freePacket(pkt);
			break;
	}

	if(status == kIOFireWireOutOfTLabels)
	{
		// IOLog("IOFWIP: kIOFireWireOutOfTLabels\n");
		fTxStatus = kIOReturnOutputStall;
		status = kIOReturnOutputStall;
	}
	else
	{
		status = kIOReturnOutputSuccess;
	}

    return status;
} // end outputPacket

/*-------------------------------------------------------------------------
 * Called by IOEthernetInterface client to enable the controller.
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/
IOReturn IOFireWireIP::enable(IONetworkInterface * netif)
{
    /*
     * If an interface client has previously enabled us,
     * and we know there can only be one interface client
     * for this driver, then simply return true.
     */
    if (netifEnabled)
    {
        IOLog("IOFireWireIP: already enabled\n");
        return kIOReturnSuccess;
    }

	// IOLog("IOFireWireIP: enable \n");
    /*
     * Mark the controller as enabled by the interface.
     */
    netifEnabled = true;

	/*
     * Initialize the IP command pool.
     */
//	initIPCmdPool();
	
	
	/*
     * Start our IOOutputQueue object.
     */
    transmitQueue->setCapacity( TRANSMIT_QUEUE_SIZE );
    transmitQueue->start();
	
    return kIOReturnSuccess;
	
}// end enable netif


/*-------------------------------------------------------------------------
 * Called by IOEthernetInterface client to disable the controller.
 * This method is always called while running on the default workloop
 * thread.
 *-------------------------------------------------------------------------*/
IOReturn IOFireWireIP::disable(IONetworkInterface * /*netif*/)
{
    netifEnabled = false;

	// IOLog("IOFireWireIP: disable \n");

	/*
     * Free the IP command pool.
     */
	freeIPCmdPool();
	
	/*
     * Disable our IOOutputQueue object. This will prevent the
     * outputPacket() method from being called.
     */
    transmitQueue->stop();

    /*
     * Flush all packets currently in the output queue.
     */
    transmitQueue->setCapacity( 0 );
    transmitQueue->flush();

    return kIOReturnSuccess;

}// end disable netif


IOReturn IOFireWireIP::getPacketFilters( const OSSymbol	*group, UInt32 *filters ) const
{
	//IOLog("IOFireWireIP: getPacketFilters \n");

	return super::getPacketFilters( group, filters );

}// end getPacketFilters


IOReturn IOFireWireIP::setWakeOnMagicPacket( bool active )
{
	//IOLog("IOFireWireIP: setWakeOnMagicPacket \n");

	return kIOReturnSuccess;

}// end setWakeOnMagicPacket

const OSString * IOFireWireIP::newVendorString() const
{
	//IOLog("IOFireWireIP: newVendorString \n");
	
    return OSString::withCString("Apple");
}

const OSString * IOFireWireIP::newModelString() const
{
	//IOLog("IOFireWireIP: newModelString \n");

    return OSString::withCString("fw+");
}

const OSString * IOFireWireIP::newRevisionString() const
{
	//IOLog("IOFireWireIP: newRevisionString \n");
	
    return OSString::withCString("");

}

IOReturn IOFireWireIP::setPromiscuousMode( bool active )
{

	//IOLog("IOFireWireIP: setPromiscuousMode \n");

	isPromiscuous	= active;

	return kIOReturnSuccess;

} // end setPromiscuousMode

IOReturn IOFireWireIP::setMulticastMode( bool active )
{
	//IOLog("IOFireWireIP: setMulticastMode \n");
	
	multicastEnabled = active;

	return kIOReturnSuccess;
}/* end setMulticastMode */

// FIREWIRETODO
IOReturn IOFireWireIP::setMulticastList(IOFWAddress *addrs, UInt32 count)
{
#ifdef FIREWIRETODO	
	IOLog("IOFireWireIP: +setMulticastList count = %lx \n", count);

    for (UInt32 i = 0; i < count; i++) 
    {
		if(addrs != NULL)
		{
			IOLog(" mca.addr[%lx] %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", 
							i, addrs->bytes[0], addrs->bytes[1], addrs->bytes[2], addrs->bytes[3], 
							addrs->bytes[4], addrs->bytes[5], addrs->bytes[6], addrs->bytes[7]);
			addrs++;
		}
    }

	IOLog("IOFireWireIP: -setMulticastList \n");
#endif
	
    return kIOReturnSuccess;
}

/*!
	@function registerWithPolicyMaker
	@abstract Initialize the driver for power management and register
			  ourselves with policy-maker.
	@param none.
	@result Returns 0 or 1.
*/
IOReturn IOFireWireIP::registerWithPolicyMaker(IOService *policyMaker)
{
	IOReturn	rc;
	
	fPolicyMaker = policyMaker;
//  May be can uncomment this portion when we have native Asyncstream receive support
//	if (fBuiltin)
		rc = policyMaker->registerPowerDriver( this, ourPowerStates, kNumOfPowerStates );
//	else 
//		rc = super::registerWithPolicyMaker( policyMaker );	// return unsupported

	return rc;
}

/*!
	@function maxCapabilityForDomainState
	@abstract returns the maximum state of card power, which would be
			  power on without any attempt to power manager.
	@param none.
	@result Returns 0 or 1.
*/
unsigned long IOFireWireIP::maxCapabilityForDomainState( IOPMPowerFlags domainState )
{
	if (domainState & IOPMPowerOn)
		return kNumOfPowerStates - 1;

	return 0;
}

/*!
	@function initialPowerStateForDomainState
	@abstract The power domain may be changing state.	If power is on in the new
			  state, that will not affect our state at all.  If domain power is off,
			  we can attain only our lowest state, which is off.
	@param none.
	@result Returns 0 or 1.
*/
unsigned long IOFireWireIP::initialPowerStateForDomainState( IOPMPowerFlags domainState )
{
	if (domainState & IOPMPowerOn)
	   return kNumOfPowerStates - 1;

	return 0;
}

/*!
	@function powerStateForDomainState
	@abstract The power domain may be changing state.	If power is on in the new
			  state, that will not affect our state at all.  If domain power is off,
			  we can attain only our lowest state, which is off.
	@param none.
	@result Returns 0 or 1.
*/
unsigned long IOFireWireIP::powerStateForDomainState(IOPMPowerFlags domainState )
{
	if (domainState & IOPMPowerOn)
		return 1;
							
	return 0;
}


/*!
	@function setPowerState
	@abstract 
	@param none.
	@result Returns IOPMAckImplied or IOPMNoSuchState.
*/
IOReturn IOFireWireIP::setPowerState(unsigned long	powerStateOrdinal,
									IOService		*whatDevice)
{
	IOReturn status = kIOReturnSuccess;

	// Do nothing if state invalid
	if (powerStateOrdinal >= kNumOfPowerStates)
		return IOPMNoSuchState;						

	//IOLog("IOFireWireIP: setPowerState \n");
	
	// Fix to TiPBG4 sleep as soon as booted
	if(unitCount == 0)
		return IOPMAckImplied;
	
	if ( powerStateOrdinal == 0 )
    {
		//IOLog(" IOFireWireIP: powering off\n");
		status = stopAsyncStreamReceiveClients();
	}
	else if ( powerStateOrdinal == 1 )
    {
		//IOLog(" IOFireWireIP: powering on\n");
		status = startAsyncStreamReceiveClients();
	}
	else
		IOLog("IOFireWireIP: unknown powerStateOrdinal \n");

	return IOPMAckImplied;
}

/*!
	@function createMediumState
	@abstract 
	@param none.
	@result create a supported medium information and 
			attach to the IONetworkingFamily.
*/
bool IOFireWireIP::createMediumState()
{
	OSDictionary * mediumDict = OSDictionary::withCapacity(5);
	if (!mediumDict) 
	{
		return false;
	}
	
	IONetworkMedium * medium = IONetworkMedium::medium(
								(IOMediumType)kIOMediumEthernetAuto | 
								kIOMediumOptionFullDuplex, fLcb->maxBroadcastSpeed);
	if (medium) 
	{
		mediumDict->setObject(medium->getKey(), medium);
		setCurrentMedium(medium);
		medium->release();
	}
			
	if (!publishMediumDictionary(mediumDict)) 
	{
		return false;
	}
	
	if( mediumDict )
	{
		mediumDict->release();
	}

	// Link status is Valid and inactive
	return setLinkStatus( kIONetworkLinkValid, getCurrentMedium(), 0 ); 
}

#pragma mark -
#pragma mark еее IOFirewireIP methods еее

/*!
	@function builtIn
	@abstract checks whether the FWIM is for builtin H/W, if not 
			  sets appropriate performance related params
	@param none.
	@result Returns void.
*/
void IOFireWireIP::builtIn()
{
    IORegistryEntry* parent = fControl->getParentEntry(gIOServicePlane); 
//	IOLog("controllers parent %s\n", parent->getName(gIOServicePlane));

	if(strcmp(parent->getName(gIOServicePlane), "AppleLynx") == 0)
	{
		bOnLynx = true;
		fMaxRxIsocPacketSize = 2048;
		fMaxTxAsyncDoubleBuffer =  1 << 9;
	} 
	else
	{
		bOnLynx = false;
		fMaxRxIsocPacketSize = 4096;
		fMaxTxAsyncDoubleBuffer = 1 << fDevice->maxPackLog(true);
	}

    parent = parent->getParentEntry(gIOServicePlane);
//	IOLog("FWIM's parent %s\n", parent->getName(gIOServicePlane));
//	fBuiltin = true;
}

/*!
	@function initAsyncCmdPool
	@abstract constructs Asynchronous command objects and queues them in the pool
	@param none.
	@result Returns kIOReturnSuccess if it was successful, else kIOReturnNoMemory.
*/
UInt32 IOFireWireIP::initAsyncCmdPool()
{
    IOFWIPAsyncWriteCommand		*cmd1 = NULL;
    IOReturn status = kIOReturnSuccess;
    int		i = 0;

    // IOLog("IOFireWireIP: initAsyncCmdPool+\n");

	if(fAsyncCmdPool == NULL)
	{
		// Create a command pool
		fAsyncCmdPool = IOCommandPool::withWorkLoop(getWorkLoop());
	}
		
    for(i=0; i<=MAX_ASYNC_WRITE_CMDS; i++){
        
        FWAddress addr;
        // setup block write
        addr.addressHi   = 0xdead;
        addr.addressLo   = 0xbabeface;
        
        // Create a IP Async write command 
        cmd1 = new IOFWIPAsyncWriteCommand;
        if(!cmd1) {
            status = kIOReturnNoMemory;
            break;
        }

        // Initialize the write command
        if(!cmd1->initAll(fDevice, fMaxTxAsyncDoubleBuffer, addr, txCompleteBlockWrite, this, false)) {
            status = kIOReturnNoMemory;
			cmd1->release();
            break;
        }

        // Queue the command in the command pool
        fAsyncCmdPool->returnCommand(cmd1);
    }
	
    // IOLog("IOFireWireIP: initAsyncCmdPool-\n");
	
    return status;
}

/*!
	@function initAsyncStreamCmdPool
	@abstract constructs AsyncStreamcommand objects and queues them in the pool
	@param none.
	@result Returns kIOReturnSuccess if it was successful, else kIOReturnNoMemory.
*/
UInt32 IOFireWireIP::initAsyncStreamCmdPool()
{
    IOReturn status = kIOReturnSuccess;
    int		i = 0;

    // IOLog("IOFireWireIP: initAsyncStreamCmdPool+\n");
	if(fAsyncStreamTxCmdPool == NULL)
	{
		fAsyncStreamTxCmdPool = IOCommandPool::withWorkLoop(getWorkLoop());
	}
    
	IOFWIPAsyncStreamTxCommand *cmd2 = NULL;
	
	for(i=0; i<=MAX_ASYNCSTREAM_TX_CMDS; i++){
        
        // Create a IP Async write command 
        cmd2 = new IOFWIPAsyncStreamTxCommand;
        if(!cmd2) {
            status = kIOReturnNoMemory;
            break;
        }

        // Initialize the write command
        if(!cmd2->initAll(fControl, fLcb->busGeneration, 0, 0, GASP_TAG, fMaxTxAsyncDoubleBuffer, 
						fLcb->maxBroadcastSpeed, txCompleteAsyncStream, this)) {
            status = kIOReturnNoMemory;
			cmd2->release();
            break;
        }

        // Queue the command in the command pool
        fAsyncStreamTxCmdPool->returnCommand(cmd2);
    }
	
    // IOLog("IOFireWireIP: initAsyncStreamCmdPool-\n");
	
    return status;
}


										
/*!
	@function freeIPCmdPool
	@abstract frees both Async and AsyncStream command objects from the pool
	@param none.
	@result void.
*/
void IOFireWireIP::freeIPCmdPool()
{
    IOFWIPAsyncWriteCommand	*cmd1 = NULL;
	UInt32 freeCount = 0;

    // IOLog("IOFireWireIP: freeIPCmdPool+\n");
    
    if(fAsyncCmdPool == NULL)
        return;
	
	// Should block till all outstanding commands are freed
    do
	{
        cmd1 = (IOFWIPAsyncWriteCommand*)fAsyncCmdPool->getCommand(false);
        if(cmd1 != NULL)
		{
			freeCount++;
            // release the command
            cmd1->release();
        }
    }while(cmd1 != NULL);
    
	fAsyncCmdPool->release();
	fAsyncCmdPool = NULL;

	if(fAsyncStreamTxCmdPool == NULL)
		return;

	IOFWIPAsyncStreamTxCommand *cmd2 = NULL;
	freeCount = 0;
	
	// Should block till all outstanding commands are freed
	do{
        cmd2 = (IOFWIPAsyncStreamTxCommand*)fAsyncStreamTxCmdPool->getCommand(false);
        if(cmd2 != NULL)
		{
			freeCount++;
            // release the command
            cmd2->release();
        }
    }while(cmd2 != NULL);
    
	fAsyncStreamTxCmdPool->release();
	fAsyncStreamTxCmdPool = NULL;

    // IOLog("IOFireWireIP: freeIPCmdPool-\n");
	
    return;
}



IOReturn IOFireWireIP::stopAsyncStreamReceiveClients()
{
	if(!fBroadcastReceiveClient)
		return kIOReturnSuccess;
		
	return fBroadcastReceiveClient->stop();
}

IOReturn IOFireWireIP::startAsyncStreamReceiveClients()
{
	if(!fBroadcastReceiveClient)
		return kIOReturnSuccess;

	return fBroadcastReceiveClient->start(fLcb->maxBroadcastSpeed);
}

/*!
	@function createIPConfigRomEntry
	@abstract creates the config rom entry for IP over Firewire.
	@param 	none
	@result IOReturn - kIOReturnSuccess or error if failure.
*/
IOReturn IOFireWireIP::createIPConfigRomEntry()
{
	IOReturn ioStat = kIOReturnSuccess;
    IP1394_HDW_ADDR	hwAddr;
//	UInt8 *a;
	
    // Create entries for UnitSpecID and UnitSwVersion
    fLocalIP1394ConfigDirectory = IOLocalConfigDirectory::create();

    if (!fLocalIP1394ConfigDirectory){
        ioStat =  kIOReturnError;
    } 
	
    if(ioStat == kIOReturnSuccess)
        ioStat = fLocalIP1394ConfigDirectory->addEntry(kConfigUnitSpecIdKey, IP1394_SPEC_ID) ;

    if(ioStat == kIOReturnSuccess)
        ioStat = fLocalIP1394ConfigDirectory->addEntry(kConfigUnitSwVersionKey, IP1394_VERSION) ;

    if(ioStat == kIOReturnSuccess){
		CSRNodeUniqueID	fwuid = fDevice->getUniqueID();

        hwAddr.eui64.hi = (UInt32)(fwuid >> 32);
        hwAddr.eui64.lo = (UInt32)(fwuid & 0xffffffff);
		//bcopy(macAddr, &hwAddr.eui64, FIREWIRE_ADDR_LEN);
#ifdef FIREWIRETODO         
        // Add Mac specific info in the unit directory
		a = (UInt8*)&hwAddr.eui64.hi;
		a[0] = macAddr[0]; 
		a[1] = macAddr[1]; 
		a[2] = macAddr[2]; 
		a[3] = macAddr[3];

		a = (UInt8*)&hwAddr.eui64.lo;
		a[0] = macAddr[4]; 
		a[1] = macAddr[5]; 
		a[2] = macAddr[6]; 
		a[3] = macAddr[7];
#endif
		
        hwAddr.maxRec	= fDevice->maxPackLog(true);              
        hwAddr.spd		= fDevice->FWSpeed();
                         
        hwAddr.unicastFifoHi = fIP1394Address.addressHi;      
        hwAddr.unicastFifoLo = fIP1394Address.addressLo;
        
        // IOLog("IOFireWireIP GUID = 0x%lx:0x%lx \nmaxRec  = %d \nspeed   = %d  \nFifo Hi = 0x%x\nFifo Lo = 0x%lx\n",
        //      hwAddr.eui64.hi, hwAddr.eui64.lo, hwAddr.maxRec,
        //      hwAddr.spd, hwAddr.unicastFifoHi, hwAddr.unicastFifoLo);
        
        // Get a OSData created for storing the whole structure in the config rom .:)
        fwOwnAddr = OSData::withBytes(&hwAddr, sizeof(IP1394_HDW_ADDR));
        
        if(fwOwnAddr != NULL){
            // Create the mac specific data key in the config rom
            ioStat = fLocalIP1394ConfigDirectory->addEntry(kConfigUnitDependentInfoKey, 
                                                           fwOwnAddr, NULL);
        }
    }
    // lets publish it
    if(ioStat == kIOReturnSuccess)
        ioStat = fControl->AddUnitDirectory(fLocalIP1394ConfigDirectory) ;

    // Create entries for IPv6 UnitSpecID and UnitSwVersion
    fLocalIP1394v6ConfigDirectory = IOLocalConfigDirectory::create();

    if (!fLocalIP1394v6ConfigDirectory){
        ioStat =  kIOReturnError;
    } 

	if(ioStat == kIOReturnSuccess)
		ioStat = fLocalIP1394v6ConfigDirectory->addEntry(kConfigUnitSpecIdKey, IP1394_SPEC_ID) ;
	
	if(ioStat == kIOReturnSuccess)
		ioStat = fLocalIP1394v6ConfigDirectory->addEntry(kConfigUnitSwVersionKey, IP1394v6_VERSION) ;

	if(fwOwnAddr != NULL)
	{
		// Create the mac specific data key in the config rom
		ioStat = fLocalIP1394v6ConfigDirectory->addEntry(kConfigUnitDependentInfoKey, 
																		fwOwnAddr, NULL);
	}

    if(ioStat == kIOReturnSuccess)
        ioStat = fControl->AddUnitDirectory(fLocalIP1394v6ConfigDirectory) ;

	memcpy(&fLcb->ownHardwareAddress, &hwAddr, sizeof(IP1394_HDW_ADDR)); 
    // IOLog("IOFireWireIP start status %d, specId %x swversion %x\n",
    //                               ioStat, IP1394_SPEC_ID, IP1394_VERSION );
    
    // end of unit directory addition
	return ioStat;
}

/*!
	@function createDrbWithDevice
	@abstract create device reference block for a device object.
	@param lcb - the firewire link control block for this interface.
	@param eui64 - global unique id of a device on the bus.
	@param fDevObj - IOFireWireNub that has to be linked with the device reference block.
	@param itsMac - Indicates whether the destination is Macintosh or not.
	@result DRB* - pointer to the device reference block.
*/
DRB *IOFireWireIP::createDrbWithDevice(LCB *lcb, UWIDE eui64, IOFireWireNub *fDevObj, bool itsMac)
{
	DRB   *drb = NULL;
	CSRNodeUniqueID fwuid;
	
    // Returns DRB if EUI-64 matches
    drb = getDrbFromEui64(lcb, eui64);   

    // Device reference ID already created
    if (drb != NULL)
	{    
		// IOLog("   DRB Exists --- clear the timer --- \n");
		drb->deviceID = (UInt32)fDevObj;
		
		// get the 64 bit address
		fwuid = fDevObj->getUniqueID();
		if(itsMac)
			makeEthernetAddress(&fwuid, drb->fwaddr, GUID_TYPE);
		else
			getBytesFromGUID((void*)(&fwuid), drb->fwaddr, GUID_TYPE);

        drb->timer = 0;          
		drb->itsMac = itsMac;
		
        // Just return it to caller
        return drb;        
    }
	else if ((drb = (DRB*)allocateCBlk(lcb)) == NULL)
	{    // Get an empty DRB
    	IOLog("   No DRB's - failure: \n");
        return NULL;
	}
	else
	{                         // Success! Initialixe the DRB...
        drb->deviceID = (UInt32)fDevObj;

		// get the 64 bit address
		fwuid = fDevObj->getUniqueID();
		if(itsMac)
			makeEthernetAddress(&fwuid, drb->fwaddr, GUID_TYPE);
		else
			getBytesFromGUID((void*)(&fwuid), drb->fwaddr, GUID_TYPE);
			
		
        drb->timer	= 0;
        drb->lcb	= lcb;
        drb->eui64	= eui64;
		drb->itsMac = itsMac;

        // In case of failure below
        drb->maxSpeed = kFWSpeed100MBit;      
        drb->maxSpeed = fDevObj->FWSpeed();
        //
        // When the packet is transmitted, it will pick the smaller of this value or the 
        // maxRec from the device's ARP.	
	    //
		drb->maxPayload = fDevObj->maxPackLog(true);
		
		linkCBlk(&lcb->activeDrb, drb);
	}
	
    return drb;
}

/*!
	@function createIPFifoAddress
	@abstract creates the pseudo address space for IP over Firewire.
	@param 	UInt32 fifosize - size of the pseudo address space
	@result IOReturn - kIOReturnSuccess or error if failure.
*/
IOReturn IOFireWireIP::createIPFifoAddress(UInt32 fifosize)
{
    IOReturn		ioStat = kIOReturnSuccess;
	/// UInt32			ptr = (UInt32)&fIP1394Address;
	
	// IOLog("fIP1394Address %lX %d\n", ptr, __LINE__);
										
	// add  csr address space
    fIP1394AddressSpace = fControl->createPseudoAddressSpace(&fIP1394Address, fifosize,
                                                            NULL,
                                                            &rxUnicast,
                                                            this);
    if (fIP1394AddressSpace == NULL){
        IOLog("IOFireWireIP PseudoAddressSpace failure status %d\n", ioStat);
        ioStat = kIOReturnNoMemory;
    }

    if(ioStat == kIOReturnSuccess ){
		// change for performance, coalescing incoming writes
		fIP1394AddressSpace->setARxReqIntCompleteHandler(this, &rxUnicastComplete);
	}

    if(ioStat == kIOReturnSuccess ){
        ioStat = fIP1394AddressSpace->activate();
    }
    
    if(ioStat != kIOReturnSuccess){
        IOLog("IOFireWireIP PseudoAddressSpace Activate failure status %d\n", ioStat);
    }
    // end of csr address space
	
	return ioStat;
}

 
/*!
	@function getMTU
	@abstract returns the MTU (Max Transmission Unit) supported by the IOFireWireIP.
	@param None.
	@result UInt32 - MTU value.
*/
UInt32 IOFireWireIP::getMTU()
{
    UInt32	mtu = 0;
    
	mtu = FIREWIRE_MTU;

    return mtu;
}

/*!
	@function getMacAddress
	@abstract returns the mac address of size "len".
	@param srcBuf - source buffer that can hold the macaddress.
	@param len - source buffer size.
	@result UInt32 - size copied.
*/
void *IOFireWireIP::getMacAddress(char	*srcBuf, UInt32 len)
{
	return memcpy(srcBuf, macAddr, FIREWIRE_ADDR_LEN);
}

/*!
	@function setIPAddress
	@abstract sets the ipaddress for the link control block.
	@param in_addr *sip - ipaddress contained in the in_addr structure.
	@result void.
*/
void IOFireWireIP::setIPAddress(register struct in_addr *sip)
{
	fLcb->ownIpAddress = sip->s_addr;
}

/*!
	@function getBytesFromGUID
	@abstract constructs byte array from the GUID.
	@param fwuid - GUID of the node.
	@param bufAddr - pointer to the buffer.
	@result void.
*/
void IOFireWireIP::getBytesFromGUID(void *guid, u_char *bufAddr, UInt8 type)
{
	u_long lo=0, hi=0;

	if(type == GUID_TYPE)
	{
		CSRNodeUniqueID	*fwuid = (CSRNodeUniqueID*)guid;
	    hi	=	(u_long)(*fwuid >> 32);
		lo	=	(u_long)(*fwuid & 0xffffffff);
	}
	else
	{
		UWIDE *eui64 = (UWIDE*)guid;
		hi	=	eui64->hi;
		lo	=	eui64->lo;
	}

	bufAddr[0] = (unsigned char)((hi >> 24) & 0x000000ff);
	bufAddr[1] = (unsigned char)((hi >> 16) & 0x000000ff);
    bufAddr[2] = (unsigned char)((hi >> 8) & 0x000000ff);
    bufAddr[3] = (unsigned char)((hi) & 0x000000ff);
	
	bufAddr[4] = (unsigned char)((lo >> 24) & 0x000000ff);
	bufAddr[5] = (unsigned char)((lo >> 16) & 0x000000ff);
	bufAddr[6] = (unsigned char)((lo >> 8) & 0x000000ff);
    bufAddr[7] = (unsigned char)((lo) & 0x000000ff);
}

/*!
	@function makeEthernetAddress
	@abstract constructs mac address from the GUID.
	@param fwuid - GUID of the node.
	@param bufAddr - pointer to the buffer.
	@param vendorID - vendorID.
	@result void.
*/
void IOFireWireIP::makeEthernetAddress(CSRNodeUniqueID	*fwuid, u_char *bufAddr, UInt32 vendorID)
{
#ifdef FIREWIRETODO
	//
	// Update the guid array for every addition of new Vendor ID 
	//
	UInt8 guids[][6] = {{0x00,0x30,0x65},
						{0x00,0x03,0x93},
						{0x00,0x0a,0x27},
						{0x00,0x05,0x02},
						{0x00,0x50,0xE4},
						{0x00,0x0A,0x95}};
	bool bFound = false;
#endif
        
	getBytesFromGUID(fwuid, bufAddr, GUID_TYPE);

#ifdef FIREWIRETODO
	// Work around the sharing of GUID across ethernet and firewire
	for (UInt32 i = 0; i <= LAST(guids); i++)
		if (memcmp(bufAddr, guids[i], 3) == 0) 
		{  
			// if last one roll back
			(LAST(guids) == i)?i = 0:i++;
				
			// reset to next possible address
			memcpy(bufAddr, guids[i], 3);
			bFound = true;
			break;
		}
	
	if(!bFound)
		memcpy(bufAddr, guids[2], 3);
#endif		
		
}

/*!
	@function deviceAttach
	@abstract Callback for a Unit attached of type IP1394
	@param target - callback data.
	@param refcon - callback data.
	@param newService - handle to the new IP1394 unit created.
	@result bool.
*/
bool IOFireWireIP::deviceAttach(void * target, void * refCon, IOService * newService)
{
    IOFireWireIP *fwIPObject;
    IOFireWireIPUnit *fwIPunit;
    IOFireWireNub	*fDevice = NULL;
    UWIDE eui64;
    ARB *arb;
	DRB *drb;
//	UInt32 unitType = (UInt32)refCon;
	u_char	lladdr[FIREWIRE_ADDR_LEN];
    
	if(target == NULL || newService == NULL)
		return false;

    // IOLog(" IOFireWireIP:: device attach\n");
	fwIPObject = OSDynamicCast(IOFireWireIP, (IOService *)target);
    fwIPunit = OSDynamicCast(IOFireWireIPUnit, (IOService *)newService);
	    
    if(!fwIPObject)
        return false;

    // Create the device reference block for this IP device
    fDevice = fwIPunit->getDevice();
    if(!fDevice){
        IOLog("Colonel ! we have problem !! Null device for legal unit %p\n", fwIPunit);
        return false;
    }

	//
	// If controller does not match the localnode controller then ignore the attach
	// Usefull for multiple firewire link interfaces 
	//
	if(fwIPObject->fControl != fDevice->getController())
		return false;

	// if(unitType == IP1394_VERSION)
	//	IOLog("IPv4 Unit\n");

	// if(unitType == IP1394v6_VERSION)
	//	IOLog("IPv6 Unit\n");
    
	fwIPunit->setLocalNode(fwIPObject);
	
    CSRNodeUniqueID	fwuid = fDevice->getUniqueID();

	// If other unit is Macitosh, then do the OUI rotation, else don't touch  
	if(fwIPunit->isSpecial())
	{
		fwIPObject->makeEthernetAddress(&fwuid, lladdr, GUID_TYPE);
		bcopy((void*)lladdr, &eui64, FIREWIRE_ADDR_LEN);
	}
	else
	{
		eui64.hi = (UInt32)(fwuid >> 32);
		eui64.lo = (UInt32)(fwuid & 0xffffffff);
	}
	
	
    drb = fwIPObject->createDrbWithDevice(fwIPObject->fLcb, eui64, fDevice, fwIPunit->isSpecial());
    
    if(drb == NULL){
        IOLog("No memory !, Can't create Device reference block\n\r");
		return false;
    }
	
	// Update the device reference block in IP Unit
	fwIPunit->setDrb(drb);

    // Create the arb if we recognise a IP unit.
    arb = fwIPObject->getArbFromEui64(fwIPObject->fLcb,  eui64);

	// Update the device object in the address resolution block used in the ARP resolve routine
    if(arb != NULL)
	{
		arb->handle.unicast.deviceID	= (UInt32)fDevice;
		arb->handle.unicast.maxRec		= fDevice->maxPackLog(true); 
		arb->handle.unicast.spd			= fDevice->FWSpeed();
		arb->timer	= 0;
		arb->itsMac = fwIPunit->isSpecial();
    }
	
	if(!fwIPunit->getUnitState())
	{
		fwIPObject->unitCount++;
		fwIPunit->setUnitState(true);
	}
		
	fwIPObject->updateBroadcastValues(false);

	// IOLog("attach unit count %d\n", fwIPObject->unitCount);

	//	IOLog(" DRB for 0x%08lX%08lX and linked for controller %p !!!\n\r", 
	//	eui64.hi, eui64.lo,	
	//	fDevice->getController());
	fwIPObject->updateLinkStatus();

    return true;
}


/*!
	@function deviceDetach
	@abstract Callback for a Unit detach of type IP1394 - Invoked from 
			  IOFireWireIPunit::message(....)
	@param target - callback data.
	@result void.
*/	
void IOFireWireIP::deviceDetach(void *target)
{
    IOFireWireIPUnit	*fwIPUnit = (IOFireWireIPUnit *)target;
    DRB					*drb = NULL; 
	ARB					*arb = NULL;
	int					arpt_keep = (30*60); // lets wait for 30 mins safe !

	drb = fwIPUnit->getDrb();
	
	//IOLog("IOFireWireIP: device detach called + \n");

    if(drb != NULL)
	{
        drb->timer = arpt_keep;
		arb = getArbFromFwAddr(fLcb, drb->fwaddr);
		if(arb != NULL)
			arb->timer = arpt_keep;
    }
	
	if(unitCount > 0)
	{
		unitCount--;
		//IOLog("detach unit count %d\n", unitCount);
	}
	
	if(unitCount == 0)
	{
		// Link status is Valid and inactive
		setLinkStatus( kIONetworkLinkValid, getCurrentMedium(), 0 ); 
		//IOLog("...fw media inactive\n");
	}

	//IOLog("IOFireWireIP: device detach called \n");
	
	updateBroadcastValues(true);
		
	updateLinkStatus();
}

/*!
	@function updateBroadcastValues
	@abstract Updates the max broadcast payload and speed  
	@param reset - useful to know whether to start from beginning.
	@result void.
*/	
void IOFireWireIP::updateBroadcastValues(bool reset)
{
    DRB	*drb = NULL;
    CBLK *cBlk = NULL;
	IOFireWireDevice *remoteDevice = NULL;
	
	if(reset)
	{
		fLcb->maxBroadcastPayload = fDevice->maxPackLog(true);
		fLcb->maxBroadcastSpeed = fDevice->FWSpeed();
	}
		
	IOTakeLock(ipLock);
	
	// Display the active DRB
	if (fLcb->activeDrb == NULL)
	{
		//IOLog(" No active devices\n\r");
	}
	else 
	{
		// IOLog(" Active devices \n\r");
		cBlk = (CBLK*)&fLcb->activeDrb;
		
		while ((cBlk = cBlk->next) != NULL)
		{ 
			// IOLog("  %p\n\r", cBlk);
			drb = (DRB*)cBlk;
			
			// Disappeared device, so lets skip
			if(drb->timer > 0)
				continue;
			
			remoteDevice = (IOFireWireDevice*)drb->deviceID;
			// drb->maxSpeed = fDevice->FWSpeed(remoteDevice);
			// drb->maxPayload = 1 << fDevice->maxPackLog(true, remoteDevice);
			
			// IOLog("maxSpeed = %d & maxPayload = %d \n", drb->maxSpeed, drb->maxPayload);
				
			if(fLcb->maxBroadcastSpeed > drb->maxSpeed)
				fLcb->maxBroadcastSpeed = drb->maxSpeed;
			
			if(fLcb->maxBroadcastPayload > drb->maxPayload)
				fLcb->maxBroadcastPayload = drb->maxPayload;
		}
	}
   
    IOUnlock(ipLock);
}

/*!
	@function updateLinkStatus
	@abstract Updates the link status based on maxbroadcast speed & payload.  
	@param None.
	@result void.
*/	
void IOFireWireIP::updateLinkStatus()
{

	// lets update the link status
	if(getUnitCount() == 0)
	{
		// IOLog("setLinkStatus inactive \n");

		// set medium inactive
		setLinkStatus(kIONetworkLinkValid, getCurrentMedium(), 0); 
	}
	else
	{
		// IOLog("setLinkStatus %d \n", (1 << fLcb->maxBroadcastSpeed) * 100 * 1000000);

		// <fix> 
		// set medium inactive, before setting it to active for radar 3300357
		setLinkStatus(kIONetworkLinkValid, getCurrentMedium(), 0); 
		// </fix>

		// set medium active
		setLinkStatus (kIONetworkLinkActive | kIONetworkLinkValid,
						getCurrentMedium(), 
						(1 << fLcb->maxBroadcastSpeed) * 100 * 1000000);
						
	}	
	fLcb->ownHardwareAddress.spd = fLcb->maxBroadcastSpeed;
	// fix to enable the arp/dhcp support from network pref pane
	setProperty(kIOFWHWAddr,  (void *) &fLcb->ownHardwareAddress, sizeof(IP1394_HDW_ADDR));

}
/*!
	@function txComplete
	@abstract Callback for the Async write complete 
	@param refcon - callback data.
    @param status - status of the command.
    @param device - device that the command was send to.
    @param fwCmd - command object which generated the transaction.
	@result void.
*/
void IOFireWireIP::txCompleteBlockWrite(void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd)
{
    IOFireWireIP			*fwIPObject = (IOFireWireIP *)refcon;
    IOFWIPAsyncWriteCommand *cmd		= (IOFWIPAsyncWriteCommand *)fwCmd;
    struct mbuf * pkt = NULL;
	UInt32	type = UNFRAGMENTED;
	
	//
	// Make sure that we start the queue again,
	// only after we have stalled, not for every 
	// successful completion
	//
	if(fwIPObject->fTxStatus == kIOReturnOutputStall)
	{
		fwIPObject->fStalls++;
		fwIPObject->fTxStatus = kIOReturnOutputSuccess;
	}
	
	//
	// Only in case of kIOFireWireOutOfTLabels, we ignore 
	// freeing of Mbuf
	//
	if(status == kIOReturnSuccess)
	{
		// We get callback 1 packet at a time, so we can increment by 1
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->outputPackets);
		fwIPObject->fTxUni++;
	}
	else 
	{
		// IOLog("IOFireWireIP: txCompleteBlockWrite error %x\n", status);
		// Increment error output packets
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->outputErrors);
		fwIPObject->fCallErrs++;
	}
	
	type = cmd->getLinkFragmentType();
	pkt = cmd->getMbuf();
	if(pkt != NULL)
	{
		if(type == LAST_FRAGMENT || type == UNFRAGMENTED)
		{
			/////
			// If we stalled, don't free Mbuf, the Queue will try retransmit
			/////
			if(status != kIOFireWireOutOfTLabels)
			{
				// free the packet
				fwIPObject->freePacket(pkt);
			}
		}
	}

	cmd->resetDescriptor();

    // Queue the command back into the command pool
	if(fwIPObject->fAsyncCmdPool != NULL)
	{
		fwIPObject->fAsyncCmdPool->returnCommand(cmd);
	}
	else
	{
		IOLog("IOFWIPAsyncWriteCommand got completed after the interface is disabled !!\n");
		cmd->release();
	}

	IOTakeLock(fwIPObject->ipLock);
	fwIPObject->fUsedCmds--;
    IOUnlock(fwIPObject->ipLock);

	//////
	// Fix to over kill servicing the queue 
	//////
	if(status != kIOFireWireOutOfTLabels && status != kIOReturnNoResources && (fwIPObject->fUsedCmds  <= (gLowWaterMark+10)))
	{
		fwIPObject->transmitQueue->service(0x01);
	}
	
    return;
}

/*!
	@function txAsyncStreamComplete
	@abstract Callback for the Async stream transmit complete 
	@param refcon - callback 
	@param status - status of the command.
	@param bus information.
	@param fwCmd - command object which generated the transaction.
	@result void.
*/
void IOFireWireIP::txCompleteAsyncStream(void *refcon, IOReturn status, 
										IOFireWireBus *bus, IOFWAsyncStreamCommand *fwCmd)
{
    IOFireWireIP			*fwIPObject = (IOFireWireIP *)refcon;
    IOFWIPAsyncStreamTxCommand *cmd		= (IOFWIPAsyncStreamTxCommand *)fwCmd;
    struct mbuf * pkt = NULL;
	
	//IOLog("IOFireWireIP: txCompleteAsyncStream called\n");

	if(status == kIOReturnSuccess)
	{
		// We send 1 packet at a time, so we can increment by 1
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->outputPackets);
		fwIPObject->fTxBcast++;
	}
	else
	{
		// Error, so we touch the error output packets
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->outputErrors);
	}
    
	pkt = cmd->getMbuf();
	if(pkt != NULL)
		fwIPObject->freePacket(pkt);
	
	cmd->setMbuf(NULL);

	if(fwIPObject->fAsyncStreamTxCmdPool != NULL)
	{
		// Queue the command back into the command pool
		fwIPObject->fAsyncStreamTxCmdPool->returnCommand(cmd);
	}
	else
	{
		IOLog("IOFWIPAsyncStreamTxCommand got completed after the interface is disabled !!\n");
		cmd->release();
	}
	
    return;
}



/*!
	@function txARP
	@abstract Transmit ARP request or response.
	@param ifp - ifnet pointer.
	@param m - mbuf containing the ARP packet.
	@result void.
*/
void IOFireWireIP::txARP(struct ifnet *ifp, struct mbuf *m){
	struct firewire_header *fwh;
	struct mbuf *n;
	UInt8 *buf;
	UInt32	offset = 0;
	IOFWIPAsyncStreamTxCommand	*cmd = NULL;
	UInt32 cmdLen = 0;
	UInt32 dstBufLen = 0;
	UInt32 ret = 0;
	struct arp_packet *fwa_pkt;

	//IOLog("IOFireWireIP: txARP called\n");
	
	fwh = mtod(m, struct firewire_header *);
        
	n = m;

	if(fAsyncStreamTxCmdPool == NULL)
	{
		initAsyncStreamCmdPool();
	}
	
	// Get an async command from the command pool
	cmd = (IOFWIPAsyncStreamTxCommand*)fAsyncStreamTxCmdPool->getCommand(false);
		
	// Lets not block to get a command, IP may retry soon ..:)
	if(cmd == NULL)
	{
		// Error, so we touch the error output packets
		networkStatAdd(&(getNetStats())->outputErrors);
		goto endtxARP;
	}
		
	// Get the buffer pointer from the command pool
	buf = (UInt8*)cmd->getBufferFromDesc();
	dstBufLen = cmd->getMaxBufLen();
	
	offset = sizeof(struct firewire_header);
	cmdLen = m->m_pkthdr.len - sizeof(struct firewire_header);
	
	// Construct the GASP_HDR and Unfragment header
	fwa_pkt = (struct arp_packet*)(buf);
    bzero((caddr_t)fwa_pkt, sizeof(*fwa_pkt));
    
	// Fill the GASP fields 
    fwa_pkt->gaspHdr.sourceID = htons(fLcb->ownNodeID);
    memcpy(&fwa_pkt->gaspHdr.gaspID, &gaspVal, sizeof(GASP_ID));
	
	// Set the unfragmented header information
    fwa_pkt->ip1394Hdr.etherType = htons(ETHERTYPE_ARP);
	// Modify the buffer pointer
	buf += (sizeof(GASP_HDR) + sizeof(IP1394_UNFRAG_HDR));
	// Copy the arp packet into the buffer
	// n = mbufTobuffer(n, &offset, buf, dstBufLen, cmdLen);
	mbufTobuffer(n, &offset, buf, dstBufLen, cmdLen);
	// Update the length to have the GASP and IP1394 Header
	cmdLen += (sizeof(GASP_HDR) + sizeof(IP1394_UNFRAG_HDR));
	
	// IOLog("  CmdLen %lX BcastSpd %d\n\r", cmdLen, fLcb->maxBroadcastSpeed);
	
	// Initialize the command with new values of device object
	ret = cmd->reinit(fLcb->busGeneration, 
					  DEFAULT_BROADCAST_CHANNEL, 
					  cmdLen,
					  fLcb->maxBroadcastSpeed,
					  txCompleteAsyncStream, 
					  this);
					
	if (ret == kIOReturnSuccess)			
		cmd->submit(true);
			
endtxARP:

	return;
}

/*!
	@function txIP
	@abstract Transmit IP packet.
	@param ifp - ifnet pointer.
	@param m - mbuf containing the IP packet.
	@param type - type of the packet (IPv6 or IPv4).
    @result void.
*/
SInt32 IOFireWireIP::txIP(struct ifnet *ifp, struct mbuf *m, UInt16 type)
{
	struct firewire_header *fwh;
	GASP_HDR *gaspHdr;
	IP1394_ENCAP_HDR *ip1394Hdr;
	struct mbuf *n;
	TNF_HANDLE	*handle;
	UInt8 *datagram;
	UInt8 *buf;
    FWAddress addr;
	UInt16	datagramSize = 0;
	ARB *arb = NULL;
	IOFWIPAsyncWriteCommand	*cmd = NULL;
	IOFWIPAsyncStreamTxCommand *asyncStreamCmd = NULL;
 	UInt32 offset = 0;
	UInt32 dstBufLen = 0; 
	UInt32 maxPayload = 0;
	UInt16 residual = 0;
	UInt16 drbMaxPayload = 0;
	UInt16 dgl = 0;
	BOOLEAN unfragmented;
	UInt16 headerSize = 0;
	UInt16 fragmentOffset = 0;
	UInt16 fragmentSize = 0;
	UInt32 cmdLen = 0;
	UInt32 channel = 0;
	SInt32 status = 0;
	IOFireWireNub *device;
	bool broadcast = false;
	bool fDeferNotify = true;
	bool bTxError = false;	
	
	// IOLog("IOFireWireIP: txIP called\n");
	
	fwh = mtod(m, struct firewire_header *);

	if(bcmp(fwh->ether_dhost, fwbroadcastaddr, FIREWIRE_ADDR_LEN) == 0 ||
	   bcmp(fwh->ether_dhost, multicast, 3) == 0)
	{
		// broadcast/multicast
		broadcast = true;
	}
	else
	{
		// unicast
		arb = getArbFromFwAddr(fLcb, fwh->ether_dhost);
	}
	
	
	// If its not a packet header
	if(!(m->m_flags & M_PKTHDR))
	{
		status = kIOReturnError;
		goto endtxIP;
	}
		
	n = m;
	
	datagram = (UInt8*)n->m_data;
	datagramSize = m->m_pkthdr.len - sizeof(struct firewire_header);

	if(datagramSize > fMaxPktSize)
	{
		fMaxPktSize = datagramSize; 
	} 
	
	// If multicast/broadcast, lets send through via asyncstream packet 
	if(broadcast)
	{
		channel = DEFAULT_BROADCAST_CHANNEL;
		// IOLog("IOFireWireIP: MCAST/BCAST size = %d, type = %x \n", datagramSize, type);
		
		maxPayload = 1 << fLcb->ownMaxPayload;
        maxPayload = MIN((UInt32)1 << fLcb->maxBroadcastPayload, maxPayload);

		// Asynchronous stream datagrams are never fragmented!
		if (datagramSize + sizeof(IP1394_UNFRAG_HDR) > maxPayload)
		{
			status = ENOBUFS;
			goto endtxIP;
		}

		if(fAsyncStreamTxCmdPool == NULL)
		{
			// create a command pool on demand
			initAsyncStreamCmdPool();
		}
		
		// Get an async command from the command pool
		asyncStreamCmd = (IOFWIPAsyncStreamTxCommand*)fAsyncStreamTxCmdPool->getCommand(false);
		
		// Lets not block to get a command, IP may retry soon ..:)
		if(asyncStreamCmd == NULL)
		{
			status = ENOBUFS;
			goto endtxIP;
		}
		
		asyncStreamCmd->setMbuf(m);
		
		// Get the buffer pointer from the command pool
		buf = (UInt8*)asyncStreamCmd->getBufferFromDesc();
		dstBufLen = asyncStreamCmd->getMaxBufLen();
		
		// Get it assigned to the header
		gaspHdr = (GASP_HDR *)buf;
        gaspHdr->sourceID = htons(fLcb->ownNodeID);	
		memcpy(&gaspHdr->gaspID, &gaspVal, sizeof(GASP_ID));
        ip1394Hdr = (IP1394_ENCAP_HDR*)((UInt8*)buf + sizeof(GASP_HDR));
        ip1394Hdr->singleFragment.etherType = htons(type);
		ip1394Hdr->singleFragment.reserved = htons(UNFRAGMENTED);
		
		cmdLen = datagramSize;
		offset = sizeof(struct firewire_header);
		headerSize = sizeof(GASP_HDR) + sizeof(IP1394_UNFRAG_HDR);
		
		// Increment the buffer pointer for the unfrag or frag header
		buf = buf + headerSize;
		
		mbufTobuffer(n, &offset, buf, dstBufLen, cmdLen);

        cmdLen += headerSize;

//		IOLog("IOFireWireIP: pkt size = %d maxbroadcastspeed %d \n", cmdLen, fLcb->maxBroadcastSpeed);

		// Initialize the command with new values of device object
		status = asyncStreamCmd->reinit(fLcb->busGeneration, 
										channel, 
										cmdLen,
										fLcb->maxBroadcastSpeed,
										txCompleteAsyncStream, 
										this);
					  
		if(status == kIOReturnSuccess)
			asyncStreamCmd->submit(true);
		
	}
	else
	{
		// IOLog("IOFireWireIP: UNICAST size = %d, type = %x\n", datagramSize, type);
	
		if(arb == NULL)
		{
			status = EHOSTUNREACH;
			/*
			IOLog("IOFireWireIP: Host unreachabele %d\n", __LINE__);
			IOLog("tgtaddr = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", 
									fwh->ether_dhost[0], fwh->ether_dhost[1],
									fwh->ether_dhost[2], fwh->ether_dhost[3],
									fwh->ether_dhost[4], fwh->ether_dhost[5],
									fwh->ether_dhost[6], fwh->ether_dhost[7]);
			*/
			goto endtxIP;
		}
	
		// Node had disappeared, but entry exists for specified timer value
		if(arb != NULL && arb->timer > 1) 
		{
			status = EHOSTUNREACH;
			// IOLog("IOFireWireIP: Host unreachabele %d\n", __LINE__);
			goto endtxIP;
		}
	
		handle = &arb->handle; 
	
		if(handle->unicast.deviceID == 0)
		{
			status = EHOSTUNREACH;
			// IOLog("IOFireWireIP: Host unreachabele %d\n", __LINE__);
			goto endtxIP;
		}
			
	
		//
		// Get the actual length of the packet from the mbuf
		//
		residual = datagramSize;
		
		// setup block write
		addr.addressHi   = handle->unicast.unicastFifoHi;
		addr.addressLo   = handle->unicast.unicastFifoLo;
		device = (IOFireWireNub*)handle->unicast.deviceID;
		
		// Calculate the payload and further down will decide the fragmentation based on that
		drbMaxPayload = 1 << device->maxPackLog(true, addr);
		// to test fragmentation ;
		// drbMaxPayload = 1 << 10;
		maxPayload = 1 << fLcb->ownMaxPayload;
		maxPayload = MIN((UInt32)1 << handle->unicast.maxRec, maxPayload);
		maxPayload = MIN(drbMaxPayload, maxPayload);

		// Only fragments use datagram label
		if (!(unfragmented = ((datagramSize + sizeof(IP1394_UNFRAG_HDR)) <= maxPayload)))
		{
			dgl = fLcb->datagramLabel++; 
		}
	  
		// IOLog("txIP: maxpayload %d drbMaxPayload %d\n", maxPayload, drbMaxPayload);

		offset = sizeof(struct firewire_header);
		
		while (residual) 
		{
			if(fAsyncCmdPool == NULL)
			{
				// Create a command pool on demand
				initAsyncCmdPool();
			}
		
			// Get an async command from the command pool
			cmd = (IOFWIPAsyncWriteCommand	*)fAsyncCmdPool->getCommand(false);
		
			// Lets not block to get a command, IP may retry soon ..:)
			if(cmd == NULL) 
			{
				status = ENOBUFS;
				// IOLog("IOFireWireIP: CMD not available %d\n", __LINE__);
				goto endtxIP;
			}
		

			//if(cmd->getRetainCount() > 1)
			//	IOLog("DeQ RTC %X fUsedCmds %d\n", cmd->getRetainCount(), fUsedCmds);

			// Count the successfully dequeued commands
			IOTakeLock(ipLock);
			fUsedCmds++;
		    IOUnlock(ipLock);

			if(fUsedCmds >= gLowWaterMark)
			{
				fDeferNotify = false;
			}
					
			// false - don't copy , if true - copy the packets
			cmd->setMbuf(m, gDoCopy);
			
			// begin fragmentation
			if (unfragmented)
			{
				//
				// Lets point to the m->m_data
				// 
				headerSize = sizeof(IP1394_UNFRAG_HDR);
				cmd->setOffset(offset, true);
				cmd->setHeaderSize(headerSize);
				cmd->setLinkFragmentType(UNFRAGMENTED);

				// All done in one gulp!
				cmdLen = residual;               
				ip1394Hdr = (IP1394_ENCAP_HDR*)cmd->getDescriptorHeader(unfragmented);
				ip1394Hdr->fragment.datagramSize = htons(UNFRAGMENTED);
				ip1394Hdr->singleFragment.etherType = htons(type);
				ip1394Hdr->singleFragment.reserved = 0;
			} 
			else 
			{   
				fTxFragmentPkts++;

				// Get it assigned to the header
				headerSize = sizeof(IP1394_FRAG_HDR);
				cmd->setHeaderSize(headerSize);
				ip1394Hdr = (IP1394_ENCAP_HDR*)cmd->getDescriptorHeader(unfragmented);

				// Distinguish first, interior and last fragments
				cmdLen = MIN(residual, maxPayload - sizeof(IP1394_FRAG_HDR));
				fragmentSize = cmdLen;
				ip1394Hdr->fragment.datagramSize = htons(datagramSize - 1);
				
				if (fragmentOffset == 0) 
				{
					ip1394Hdr->fragment.datagramSize |= htons(FIRST_FRAGMENT << 14);
					ip1394Hdr->singleFragment.etherType = htons(type);
					//
					// since its first fragment we can use the first 14 bytes of the
					// area in Mbuf, incase we don't copy
					// 
					cmd->setOffset(offset, true);
					cmd->setLinkFragmentType(FIRST_FRAGMENT);
				} 
				else 
				{
					if (fragmentSize < residual)
					{
						ip1394Hdr->fragment.datagramSize |= htons(INTERIOR_FRAGMENT << 14);
						cmd->setLinkFragmentType(INTERIOR_FRAGMENT);
					}
					else
					{
						ip1394Hdr->fragment.datagramSize |= htons(LAST_FRAGMENT << 14);
						cmd->setLinkFragmentType(LAST_FRAGMENT);
					}
					
					ip1394Hdr->fragment.fragmentOffset = htons(fragmentOffset);
					cmd->setOffset(offset, false);
				}
				// Get your datagram labels correct 
				ip1394Hdr->fragment.dgl = htons(dgl);
				ip1394Hdr->fragment.reserved = 0;
			}
			// end fragmentation


        
			if(handle->unicast.deviceID != 0) 
			{ 
			
				status = cmd->initDescriptor(unfragmented, cmdLen);
				if(status != kIOReturnSuccess)
					goto endtxIP;

				// Initialize the command with new values of device object
				status = cmd->reinit((IOFireWireDevice*)handle->unicast.deviceID, 
									cmdLen+headerSize, 
									addr, 
									txCompleteBlockWrite, 
									this, 
									true);

				cmd->setDeferredNotify(fDeferNotify);
				
				if(status == kIOReturnSuccess)
					cmd->submit(gDoQueue);
				
				status = cmd->getStatus();
				
				//
				// FWFamily command overrun, return imm and indicate
				// outputStalled
				//
				if(status == kIOFireWireOutOfTLabels || status == kIOReturnNoResources)
				{
					fSubmitErrs++;
					return status;
				}
				else
				{
					//
					// its a timeout or some unknown error
					//
					bTxError = true;
				}
			}

			fragmentOffset += cmdLen;  // Account for the position and...
			offset += cmdLen;
			residual -= cmdLen;        // ...size of the fragment just sent
			
		} // end next fragment
	}

endtxIP:
	if(status != kIOReturnSuccess)
	{
		//
		// If not a transmit or submit error then we go ahead and free the Mbuf
		// and return the command
		//
		if(bTxError == false)
		{
			// IOLog("IOFireWireIP: freed in txIP %X\n", status);
			if(cmd != NULL)
			{
				cmd->resetDescriptor();
				// Queue the command back into the command pool
				if(fAsyncCmdPool)
					fAsyncCmdPool->returnCommand(cmd);
				IOTakeLock(ipLock);
				fUsedCmds--;	
			    IOUnlock(ipLock);
			}
			freePacket(m);

			// Error, so we touch the error output packets
			networkStatAdd(&(getNetStats())->outputErrors);
		}
	}
	
	return status;
}

/*!
	@function txMCAP
	@abstract This procedure transmits either an MCAP solicitation or advertisement on the
			  default broadcast channel, dependent upon whether or not an MCB is supplied.
			  Note that if more than one multicast address group is associated with a
			  particular channel that multiple MCAP group descriptors are created.
 	@param lcb - link control block of the local node.
	@param mcb - multicast control block.
	@param ipAddress - IP address.
	@result void.
*/
void IOFireWireIP::txMCAP(LCB *lcb, MCB *mcb, UInt32 ipAddress){
	ARB *arb;
	MCAST_DESCR *groupDescriptor;
	IOFWIPAsyncStreamTxCommand *asyncStreamCmd = NULL;
	struct mcap_packet *packet;
	SInt32 status;
	UInt32 cmdLen = 0;
	UInt8 *buf;

    //IOLog("IOFireWireIP: txMCAP called\n");
	if(fAsyncStreamTxCmdPool == NULL)
	{
		initAsyncStreamCmdPool();
	}
	
	// Get an async command from the command pool
	asyncStreamCmd = (IOFWIPAsyncStreamTxCommand*)fAsyncStreamTxCmdPool->getCommand(false);
		
	// Lets not block to get a command, IP may retry soon ..:)
	if(asyncStreamCmd == NULL)
		return;
			
	// Get the buffer pointer from the command pool
	buf = (UInt8*)asyncStreamCmd->getBufferFromDesc();
	// dstBufLen = asyncStreamCmd->getMaxBufLen();

	packet = (struct mcap_packet*)buf;
	memset(packet, 0, sizeof(*packet));
	packet->gaspHdr.sourceID = htons(fLcb->ownNodeID);
	memcpy(&packet->gaspHdr.gaspID, &gaspVal, sizeof(GASP_ID));
	packet->ip1394Hdr.etherType = htons(ETHER_TYPE_MCAP);
	packet->mcap.length = sizeof(*packet);          /* Fix endian-ness later */
	groupDescriptor = packet->mcap.groupDescr;
	
	if (mcb != NULL) {
		packet->mcap.opcode = MCAP_ADVERTISE;
		arb = (ARB*)&lcb->multicastArb;
      
		while ((arb = arb->next) != NULL) {
			if (arb->handle.multicast.channel == mcb->channel) {
				memcpy(&ipAddress, &arb->ipAddress, sizeof(ipAddress));
				IOLog("   Advertise %u.%u.%u.%u channel %u expires %u\n\r",
                      ((UCHAR *) &ipAddress)[0], ((UCHAR *) &ipAddress)[1],
                      ((UCHAR *) &ipAddress)[2], ((UCHAR *) &ipAddress)[3],
                      mcb->channel, mcb->expiration);
				memset(groupDescriptor, 0, sizeof(MCAST_DESCR));
				groupDescriptor->length = sizeof(MCAST_DESCR);
				groupDescriptor->type = MCAST_TYPE;
				groupDescriptor->expiration = mcb->expiration;
				groupDescriptor->channel = mcb->channel;
				groupDescriptor->speed = arb->handle.multicast.spd;
				groupDescriptor->groupAddress = arb->ipAddress;
				groupDescriptor = (MCAST_DESCR*)((UInt32) groupDescriptor + sizeof(MCAST_DESCR));
				packet->mcap.length += sizeof(MCAST_DESCR);
			}
		}
	} else {
		packet->mcap.opcode = MCAP_SOLICIT;
#ifdef FIREWIRETODO		
		IOLog("   Solicit %u.%u.%u.%u\n\r", ((UCHAR *) &ipAddress)[0],
                ((UCHAR *) &ipAddress)[1], ((UCHAR *) &ipAddress)[2],
                ((UCHAR *) &ipAddress)[3]);
#endif			
		memset(groupDescriptor, 0, sizeof(MCAST_DESCR));
		groupDescriptor->length = sizeof(MCAST_DESCR);
		groupDescriptor->type = MCAST_TYPE;
		groupDescriptor->groupAddress = ipAddress;
		packet->mcap.length += sizeof(MCAST_DESCR);
	}

   cmdLen = packet->mcap.length;   // In CPU byte order 
   packet->mcap.length = htons(packet->mcap.length); // Serial Bus order

	asyncStreamCmd->setMbuf(NULL);

	// Initialize the command with new values of device object
	status = asyncStreamCmd->reinit(fLcb->busGeneration, 
						DEFAULT_BROADCAST_CHANNEL, 
						cmdLen,
						fLcb->maxBroadcastSpeed,
						txCompleteAsyncStream, 
						this);
					  
	if(status == kIOReturnSuccess)
		asyncStreamCmd->submit(true);
}

/*!
	@function rxUnicastFlush
	@abstract Starts the batch processing of the packets, its
	          already on its own workloop.
*/
void IOFireWireIP::rxUnicastFlush()
{
	UInt32 count = 0;

    IOTakeLock(ipLock);
	
	if(fPacketsQueued = true)
	{
		count = networkInterface->flushInputQueue();
        if(count > fMaxInputCount)
        {
            fMaxInputCount = count; 
        } 
		fPacketsQueued = false;
	}

    IOUnlock(ipLock);

	return;
}

/*!
	@function rxUnicastComplete
	@abstract triggers the indication workloop to do batch processing
				of incoming packets.
*/
void IOFireWireIP::rxUnicastComplete(void *refcon)
{
	IOFireWireIP *fwIPObject = (IOFireWireIP *)refcon;

	fwIPObject->rxUnicastFlush();

	return;
}

/*!
	@function rxUnicast
	@abstract block write handler. Handles both ARP and IP packet.
*/
UInt32 IOFireWireIP::rxUnicast( void		*refcon,
								UInt16		nodeID,
                                IOFWSpeed	&speed,
                                FWAddress	addr,
                                UInt32		len,
								const void	*buf,
								IOFWRequestRefCon requestRefcon)
{
	void *datagram, *fragment;
	UInt16 datagramSize;
	IP1394_UNFRAG_HDR *ip1394Hdr;
	IP1394_FRAG_HDR *fragmentHdr;
	RCB *currRcb;
	UInt16 fragmentSize;
	UInt16 type;
	UInt8 lf;
	RCB *rcb;
	CBLK *cBlk;
	struct mbuf *rxMBuf;
	IOFireWireIP *fwIPObject = (IOFireWireIP *)refcon;
	struct firewire_header *fwh = NULL;
	ip1394Hdr = (IP1394_UNFRAG_HDR *)buf;

//	IOLog("IOFireWireIP: rxUnicast+\n");

	if(fwIPObject->netifEnabled != true)
		return kIOReturnSuccess;
   
	// Handle the unfragmented packet
	if (ip1394Hdr->reserved == htons(UNFRAGMENTED)) 
	{
		datagram = (void *) ((ULONG) ip1394Hdr + sizeof(IP1394_UNFRAG_HDR));
		datagramSize = len - sizeof(IP1394_UNFRAG_HDR);
		type = ntohs(ip1394Hdr->etherType);
		switch (type) 
		{
			case ETHERTYPE_IPV6:
				// Update the NDP cache
				if (datagramSize >= IPV6_HDR_SIZE)
					fwIPObject->updateNDPCache(datagram, &datagramSize);
				else
					break;
			case ETHERTYPE_IP:
				if (datagramSize >= IPV4_HDR_SIZE)
					fwIPObject->rxIP(fwIPObject,  datagram, datagramSize, FW_M_UCAST, type);
				break;

			case ETHERTYPE_ARP:
				if (datagramSize >= sizeof(IP1394_ARP))
				{
					fwIPObject->rxARP(fwIPObject, (IP1394_ARP*)datagram, FW_M_UCAST);
				}
				break;
			
			default :
				// Unknown packet type
				fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
				break;
		}
	}
	else
	{       
		fwIPObject->fRxFragmentPkts++;
	
		// Sigh! Have to collect and reassemble the fragments
		fragmentHdr = (IP1394_FRAG_HDR*) ip1394Hdr;  // Different header layout
		fragment = (void *) ((UInt32) fragmentHdr + sizeof(IP1394_FRAG_HDR));
		fragmentSize = len - sizeof(IP1394_FRAG_HDR);
		lf = htons(fragmentHdr->datagramSize) >> 14;
		if ((rcb = fwIPObject->getRcb(fwIPObject->fLcb, nodeID, htons(fragmentHdr->dgl))) == NULL) 
		{

			if ((rxMBuf = fwIPObject->getMBuf((htons(fragmentHdr->datagramSize) & 0x3FFF)
												+ 1 + sizeof(firewire_header))) == NULL)
			{
				IOLog("  mbuf not available\n");
				return kIOReturnSuccess;
			}
				
			if ((rcb = (RCB*)fwIPObject->allocateCBlk(fwIPObject->fLcb)) == NULL) 
			{
				IOTakeLock(fwIPObject->ipLock);
				// IOLog("  Cleaning up RCB\n");
				cBlk = (CBLK*)&(fwIPObject->fLcb->activeRcb);
				while ((cBlk = cBlk->next) != NULL) 
				{
					currRcb	= (RCB*)cBlk;
					if (currRcb->timer == 1)
					{
						fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
						if(currRcb->mBuf != NULL)
							fwIPObject->freePacket(currRcb->mBuf, kDelayFree);
						currRcb->sourceID = 0;
						currRcb->dgl = 0;
						currRcb->mBuf = NULL;
						currRcb->residual = 0;
						fwIPObject->unlinkCBlk(&(fwIPObject->fLcb->activeRcb), cBlk);
						fwIPObject->deallocateCBlk(fwIPObject->fLcb, cBlk);
					}
				}
				IOUnlock(fwIPObject->ipLock);
					
				fwIPObject->freePacket(rxMBuf, kDelayFree);
				fwIPObject->releaseFreePackets();				
				//fwIPObject->freeMBuf(rxMBuf);
				return kIOReturnSuccess;
			}
         
			rcb->sourceID = nodeID;
			rcb->dgl = htons(fragmentHdr->dgl);
			rcb->mBuf = rxMBuf;
			rcb->timer = 1;

			// Make space for the firewire header to be helpfull in firewire_demux
			fwh = (struct firewire_header *)rxMBuf->m_data;
			bzero(fwh, sizeof(struct firewire_header));
			rcb->datagram = rxMBuf->m_data  + sizeof(struct firewire_header);
			// when indicating to the top layer
			if (lf == FIRST_FRAGMENT)
			{
				fwh->ether_type = htons(fragmentHdr->fragmentOffset);
			}
			else
			{
			    // TODO : May be we should do something better
				fwh->ether_type = ETHERTYPE_IP;
			}
			rcb->datagramSize = (htons(fragmentHdr->datagramSize) & 0x3FFF) + 1;
			rcb->residual = rcb->datagramSize;
			fwIPObject->linkCBlk(&(fwIPObject->fLcb->activeRcb), rcb);
		}
      
		if (rcb->mBuf == NULL || rcb->datagram == NULL)
		{
			fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
			fwIPObject->showRcb(rcb);
			return kIOReturnError;
		}
		 
		if (lf == FIRST_FRAGMENT) 
		{
			//IOLog("   datagram size %d fragmentSize %d\n\r", rcb->datagramSize, fragmentSize);
			// Actually etherType
			rcb->etherType = htons(fragmentHdr->fragmentOffset);
			// memcpy(rcb->datagram, fragment, MIN(fragmentSize, rcb->datagramSize));
			fwIPObject->bufferToMbuf(rcb->mBuf, 
									sizeof(struct firewire_header), 
									(UInt8*)fragment, 
									MIN(fragmentSize, rcb->datagramSize));

		}
		else 
		{
			fwIPObject->bufferToMbuf(rcb->mBuf, 
									sizeof(struct firewire_header)+htons(fragmentHdr->fragmentOffset), 
									(UInt8*)fragment, 
									MIN(fragmentSize, rcb->datagramSize - htons(fragmentHdr->fragmentOffset)));
		}
      
		// Don't reduce below zero
		rcb->residual -= MIN(fragmentSize, rcb->residual); 
		// Reassembly (probably) complete ?
		if (rcb->residual == 0) 
		{           
			// Legitimate etherType ?
			if (rcb->etherType == ETHERTYPE_IP || rcb->etherType == ETHERTYPE_IPV6) 
			{  
				fwIPObject->receivePackets (rcb->mBuf, 
											rcb->mBuf->m_pkthdr.len,
											true);
			} 
			else
			{          
				// Unknown packet type
				fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
				// IOLog("   All that reassembly work for nothing!\n\r");
				// All that reassembly work for nothing!           
				fwIPObject->freeMBuf(rcb->mBuf);
			}
			rcb->mBuf = NULL;
			rcb->residual = 0;
			rcb->timer = 0;
			// Either case, release the RCB
			fwIPObject->unlinkCBlk(&(fwIPObject->fLcb->activeRcb), rcb);   
			fwIPObject->deallocateCBlk(fwIPObject->fLcb, rcb);
		}
   }

   fwIPObject->fRxUni++;
   
   return kIOReturnSuccess;
}

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
void IOFireWireIP::rxAsyncStream(DCLCommandStruct *callProc){
    
	DCLCallProc 	*ptr = (DCLCallProc*)callProc;
	RXProcData		*proc = (RXProcData*)ptr->procData;
	IOFireWireIP	*fwIPObject = (IOFireWireIP*)proc->obj;
    IOFWAsyncStreamRxCommand	*fwRxAsyncStream;
	UInt8			*buffer = proc->buffer;
	void			*datagram;
	UInt16			datagramSize;
	GASP			*gasp = (GASP*)buffer;
	LCB 			*lcb = fwIPObject->fLcb;
	ISOC_DATA_PKT	*pkt = (ISOC_DATA_PKT*)buffer; 
	UInt16 			type = 0;

    fwRxAsyncStream = (IOFWAsyncStreamRxCommand*)proc->thisObj;

    fwRxAsyncStream->modifyDCLJumps(callProc);
    
	if(fwIPObject->netifEnabled != true)
    {
		return;
    }
    
 // IOLog("IOFireWireIP: rxAsyncStream start called\n");

	if(pkt->tag != GASP_TAG){
		// Error, so we touch the error output packets
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
		IOLog("   GASP tag error\n\r");
		return;
    }
	
	// Minimum size requirement
	if (gasp->dataLength < sizeof(GASP_HDR) + sizeof(IP1394_UNFRAG_HDR)) {
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
		IOLog("   GASP header error\n\r");
		return;
    }

	// Ignore GASP if not specified by RFC 2734
	if (memcmp(&gasp->gaspHdr.gaspID, &gaspVal, sizeof(GASP_ID)) != 0) {
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
		IOLog("   Non-RFC 2734 GASP\n\r");
		return;
    }

	// Also ignore GASP if not from the local bus
	if ((htons(gasp->gaspHdr.sourceID) >> 6) != LOCAL_BUS_ID) {
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
		IOLog("   Remote GASP error\n\r");
		return;
    }
   
	// Broadcast fragmentation not supported
	if (gasp->ip1394Hdr.reserved != htons(UNFRAGMENTED)) {
		fwIPObject->networkStatAdd(&(fwIPObject->getNetStats())->inputErrors);
		IOLog("   Encapsulation header error\n\r");
		return;
    }
   
   datagram = (void *) ((UInt32) buffer + sizeof(GASP));
   datagramSize = gasp->dataLength - (sizeof(GASP_HDR) + sizeof(IP1394_UNFRAG_HDR));
   type = ntohs(gasp->ip1394Hdr.etherType);
//   IOLog("   Ether type 0x%04X (data length %d)\n\r",htons(gasp->ip1394Hdr.etherType), datagramSize);
   
	switch (type) {
		case ETHERTYPE_IPV6:
			// Update the NDP cache
			if (datagramSize >= IPV6_HDR_SIZE)
				fwIPObject->updateNDPCache(datagram, &datagramSize);
			else
				break;
		case ETHERTYPE_IP:
			if (datagramSize >= IPV4_HDR_SIZE)
				fwIPObject->rxIP(fwIPObject,  datagram, datagramSize, FW_M_BCAST, type);
			break;

		case ETHERTYPE_ARP:
			if (datagramSize >= sizeof(IP1394_ARP))
			{
				fwIPObject->rxARP(fwIPObject, (IP1394_ARP*)datagram, FW_M_BCAST);
			}
			break;
			
		case ETHER_TYPE_MCAP:
			if (datagramSize >= sizeof(IP1394_MCAP))
				fwIPObject->rxMCAP(lcb, htons(gasp->gaspHdr.sourceID), 
									(IP1394_MCAP*)datagram, datagramSize - sizeof(IP1394_MCAP));
			break;
	}

 //   IOLog("IOFireWireIP: rxAsyncStream end called\n");
	fwIPObject->fRxBcast++;

	return;
}

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
void IOFireWireIP::rxMCAP(LCB *lcb, UInt16 mcapSourceID, IP1394_MCAP *mcap, UInt32 dataSize){

	ARB *arb;
	UInt32 currentChannel;
	MCAST_DESCR *groupDescr = mcap->groupDescr;
	MCB		*mcb,	*priorMcb;
	IOFWAsyncStreamRxCommand *asyncStreamRxClient;
	IOReturn ioStat = kIOReturnSuccess;

	//IOLog("rxMCAP\n\r");
   
	if ((mcap->opcode != MCAP_ADVERTISE) && (mcap->opcode != MCAP_SOLICIT))
		return;        // Ignore reserved MCAP opcodes
	  
	dataSize = MIN(dataSize, htons(mcap->length) - sizeof(IP1394_MCAP));
   
	while (dataSize >= sizeof(MCAST_DESCR)) 
	{
	
		if (groupDescr->length != sizeof(MCAST_DESCR))
			;           // Skip over malformed MCAP group address descriptors
		else if (groupDescr->type != MCAST_TYPE)
			;           // Skip over unrecognized descriptor types
		else if ((arb = getMulticastArb(lcb, groupDescr->groupAddress)) == NULL)
			;           // Ignore if not in our multicast cache */
		else if (mcap->opcode == MCAP_SOLICIT) {
#ifdef FIREWIRETODO			
			IOLog("   Solicit %u.%u.%u.%u\n\r",
											((UCHAR *) &groupDescr->groupAddress)[0],
											((UCHAR *) &groupDescr->groupAddress)[1],
											((UCHAR *) &groupDescr->groupAddress)[2],
											((UCHAR *) &groupDescr->groupAddress)[3]);
#endif											
			mcb = &lcb->mcapState[arb->handle.multicast.channel];
			if (mcb->ownerNodeID == lcb->ownNodeID)   // Do we own the channel?
				txMCAP(lcb, mcb, 0);             // OK, respond to solicitation
				
		} 
		else if ((groupDescr->channel != DEFAULT_BROADCAST_CHANNEL) 
				&& (groupDescr->channel <= LAST(lcb->mcapState))) 
		{
			IOLog("   Advertise %u.%u.%u.%u channel %u expires %u\n\r",
												((UCHAR *) &groupDescr->groupAddress)[0],
												((UCHAR *) &groupDescr->groupAddress)[1],
												((UCHAR *) &groupDescr->groupAddress)[2],
												((UCHAR *) &groupDescr->groupAddress)[3],
												groupDescr->channel, groupDescr->expiration);
         
			mcb = &lcb->mcapState[groupDescr->channel];
			if (groupDescr->expiration < 60) 
			{
			
				if (mcb->ownerNodeID == mcapSourceID) 
				{
					currentChannel = groupDescr->channel;
				//	acquireChannel(&currentChannel, TRUE, kDoNotAllocate | kNotifyOnSuccess);
					mcb->ownerNodeID = lcb->ownNodeID;  // Take channel ownership
					mcb->nextTransmit = 1;        // Transmit advertisement ASAP
					
				}
			
			} 
			else if (mcb->ownerNodeID == mcapSourceID) 
			{
				mcb->expiration = groupDescr->expiration;
			}
			else if (mcb->ownerNodeID < mcapSourceID || mcb->expiration < 60) 
			{
            	if (mcb->ownerNodeID == lcb->ownNodeID)   // Are we the owner?
					// releaseChannel(groupDescr->channel, kDoNotDeallocate);
					// TNFReleaseChannel(lcb->unspecifiedDeviceID, groupDescr->channel, kDoNotDeallocate);
				
				mcb->ownerNodeID = mcapSourceID;
				mcb->expiration = groupDescr->expiration;
			}
			currentChannel = arb->handle.multicast.channel;
         
			if (currentChannel == DEFAULT_BROADCAST_CHANNEL) 
			{
				if (mcb->asyncStreamID == kInvalidAsyncStreamRefID) 
				{
					if(groupDescr->channel != DEFAULT_BROADCAST_CHANNEL)
					{
						asyncStreamRxClient = new IOFWAsyncStreamRxCommand;
						if(asyncStreamRxClient == NULL) 
						{
							ioStat = kIOReturnNoMemory;
						}
				
						if(asyncStreamRxClient->initAll(groupDescr->channel, rxAsyncStream, fControl, 
																	fMaxRxIsocPacketSize, this) == false) {
							ioStat = kIOReturnNoMemory;
						}
						if(ioStat == kIOReturnSuccess)
							mcb->asyncStreamID = (UInt32)asyncStreamRxClient;
					}
					else
					{
						if(fBroadcastReceiveClient != NULL)
						{
							fBroadcastReceiveClient->retain();
							mcb->asyncStreamID = (UInt32)fBroadcastReceiveClient;
						}
					}
				}
				
				arb->handle.multicast.channel = groupDescr->channel;
				mcb->groupCount++;
				
			} else if (currentChannel != groupDescr->channel) {
				
				priorMcb = &lcb->mcapState[currentChannel];
            
				if (priorMcb->groupCount == 1)
				{   
					// Are we the last user?
					asyncStreamRxClient = (IOFWAsyncStreamRxCommand *)mcb->asyncStreamID;
					if(asyncStreamRxClient != NULL)
						asyncStreamRxClient->release();
					//TNFRemoveAsyncStreamClient(lcb->clientID, mcb->asyncStreamID);
					priorMcb->asyncStreamID = kInvalidAsyncStreamRefID;
					priorMcb->groupCount = 0;
				} else if (priorMcb->groupCount > 0)
					priorMcb->groupCount--;
					
				if (mcb->asyncStreamID == kInvalidAsyncStreamRefID) 
				{
					if(groupDescr->channel != DEFAULT_BROADCAST_CHANNEL)
					{				
						asyncStreamRxClient = new IOFWAsyncStreamRxCommand;
						if(asyncStreamRxClient == NULL) {
							ioStat = kIOReturnNoMemory;
						}
				
						if(asyncStreamRxClient->initAll(groupDescr->channel, rxAsyncStream, fControl, 
																			fMaxRxIsocPacketSize, this) == false) {
							ioStat = kIOReturnNoMemory;
						}
						if(ioStat == kIOReturnSuccess)
							mcb->asyncStreamID = (UInt32)asyncStreamRxClient;
					}
					else
					{
						if(fBroadcastReceiveClient != NULL)
						{
							fBroadcastReceiveClient->retain();
							mcb->asyncStreamID = (UInt32)fBroadcastReceiveClient;
						}
					}
				}
				
				arb->handle.multicast.channel = groupDescr->channel;
				mcb->groupCount++;
			}
			
			if (arb->deletionPending && (mcb->ownerNodeID != lcb->ownNodeID)) {
				unlinkCBlk(&lcb->multicastArb, arb);
				deallocateCBlk(lcb, arb);
			}
		}
		dataSize -= MIN(groupDescr->length, dataSize);
		groupDescr = (MCAST_DESCR*)((ULONG) groupDescr + groupDescr->length);
	}


}

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
IOReturn IOFireWireIP::rxIP(IOFireWireIP *fwIPObj, void *pkt, UInt32 len, UInt32 flags, UInt16 type)
{

	struct mbuf *rxMBuf;
	struct firewire_header *fwh = NULL;
	void	*datagram = NULL;
	bool	queuePkt = false; 

	
	if ((rxMBuf = getMBuf(len + sizeof(struct firewire_header))) != NULL) 
	{
        IOTakeLock(ipLock);
		
        // Make space for the firewire header to be helpfull in firewire_demux
		fwh = (struct firewire_header *)rxMBuf->m_data;
		datagram = rxMBuf->m_data + sizeof(struct firewire_header);
		bzero(fwh, sizeof(struct firewire_header));
		if (flags == FW_M_UCAST)
		{
			bcopy(macAddr, fwh->ether_dhost, FIREWIRE_ADDR_LEN);
			queuePkt = true;
		}
		else
		{
			bcopy(fwbroadcastaddr, fwh->ether_dhost, FIREWIRE_ADDR_LEN);
			queuePkt = false;
		}
		
		// IOLog("rxIP %d, type %x\n", len, type);
		
		fwh->ether_type = type;

        IOUnlock(ipLock);
	
        bufferToMbuf(rxMBuf, sizeof(struct firewire_header), (UInt8*)pkt, len); 			
	
		receivePackets(rxMBuf, rxMBuf->m_pkthdr.len, queuePkt);
	}


	return kIOReturnSuccess;
}

/*!
	@function rxARP
	@abstract ARP processing routine called from both Asynstream path and Async path.
	@param fwIPObj - IOFireWireIP object.
	@param arp - 1394 arp packet without the GASP or Async header.
	@params flags - indicates broadcast or unicast	
	@result IOReturn.
*/
IOReturn IOFireWireIP::rxARP(IOFireWireIP *fwIPObj, IP1394_ARP *arp, UInt32 flags){

	struct mbuf *rxMBuf;
	struct firewire_header *fwh = NULL;
	void	*datagram = NULL;
	
//  IOLog("IOFireWireIP: rxARP called\n");

	if (arp->hardwareType != htons(ARP_HDW_TYPE)
		|| arp->protocolType != htons(ETHERTYPE_IP)
		|| arp->hwAddrLen != sizeof(IP1394_HDW_ADDR)
		|| arp->ipAddrLen != IPV4_ADDR_SIZE)
	{
		IOLog("IOFireWireIP: rxARP ERROR in packet header\n");
		return kIOReturnError;
	}

	if ((rxMBuf = getMBuf(sizeof(*arp) + sizeof(struct firewire_header))) != NULL) 
	{
        IOTakeLock(ipLock);
    
		fwh = (struct firewire_header *)rxMBuf->m_data;
		datagram = rxMBuf->m_data + sizeof(struct firewire_header);
		bzero(fwh, sizeof(struct firewire_header));
		fwh->ether_type = ETHERTYPE_ARP;
		// Copy the data
		memcpy(datagram, arp, sizeof(*arp));
		
        IOUnlock(ipLock);
	
        receivePackets(rxMBuf, rxMBuf->m_pkthdr.len, NULL);
	}
   
 	return kIOReturnSuccess;
}


/*!
	@function watchdog
	@abstract cleans the Link control block's stale drb's and rcb's.
			The cleanCache's job is to age (and eventually discard) device objects 
			for FireWireIP devices that have come unplugged. If they do reappear after  
			they have been discarded from the caches, all that is required is a new ARP. 
			The IP network stack handles that automatically
	@param lcb - the firewire link control block for this interface.
	@result void.
*/
void IOFireWireIP::watchdog(IOTimerEventSource *) 
{
	ARB *arb, *priorArb;
	DRB *drb, *priorDrb;
	UNSIGNED i;
	MCB *mcb;
	IOFireWireIP *fwIpObj = (IOFireWireIP*)this;
	LCB *lcb = fwIpObj->getLcb();
	IOFWAsyncStreamRxCommand *asyncStreamRxClient;

	priorDrb = (DRB *) &lcb->activeDrb;

	while ((drb = priorDrb->next) != NULL) 
	{
		if (drb->timer > 1)                    // Still has time remaining?
			drb->timer--;                      // If so, just decrement time
		else if (drb->timer == 1) 
		{            // Expiring on this clock tick?
			priorDrb->next = drb->next;        // Remove from active list
			arb = (ARB *) &lcb->unicastArb;     // Flush ARP cache for this device
			while ((arb = arb->next) != NULL) 
			{
				if (arb->handle.unicast.deviceID == drb->deviceID) 
				{
					arb->handle.unicast.deviceID = kInvalidIPDeviceRefID;
					fwIpObj->unlinkCBlk(&lcb->unicastArb, arb);
					fwIpObj->deallocateCBlk(lcb, arb);
					break;                        // Only one ARB will match
				}
			}
			drb->deviceID = kInvalidIPDeviceRefID;  // Don't notify in future
			//IOLog("IOFireWireIP: Host 0x%lx:0x%lx cleaned \n", (UInt32)drb->eui64.hi, (UInt32)drb->eui64.lo);
			fwIpObj->deallocateCBlk(lcb, drb);
			continue;         // Important to recheck ->next after unlinkCBlk!
		}
		priorDrb = drb;		  // Continue to next DRB in linked list
	}
	
	for (i = 0; i <= LAST(lcb->mcapState); i++) 
	{
		mcb = &lcb->mcapState[i];
		if (mcb->expiration > 1)      // Life in this channel allocation yet?
		{
			mcb->expiration--;         // Yes, but the clock is ticking...
		}
		else if (mcb->expiration == 1) // Dead in the water?
		{ 
			mcb->expiration = 0;          // Yes, mark it expired
			asyncStreamRxClient = (IOFWAsyncStreamRxCommand *)mcb->asyncStreamID;
			if(asyncStreamRxClient != NULL)
				asyncStreamRxClient->release();
			mcb->asyncStreamID = kInvalidAsyncStreamRefID;
			if (mcb->ownerNodeID == lcb->ownNodeID) // We own the channel?
			{  
				mcb->finalWarning = 4;  // Yes, four final advertisements
				mcb->nextTransmit = 1;  // Starting right now... 
			}
		}
		if (mcb->ownerNodeID != lcb->ownNodeID)
			continue;                     // Cycle to next array entry 
		else if (mcb->nextTransmit > 1)  // Time left before next transmit? 
			mcb->nextTransmit--;                         // Keep on ticking... 
		else if (mcb->nextTransmit == 1) 
		{              // Due to expire now? 
			if (mcb->groupCount > 0)      // Still in use at this machine? 
				mcb->expiration = 60;      // Renew this channel's lease
				
			fwIpObj->txMCAP(lcb, mcb, 0);          // Broadcast the MCAP advertisement
			
			if (mcb->expiration > 0)
				mcb->nextTransmit = 10;    // Send MCAP again in ten seconds 
			else if (--mcb->finalWarning > 0)
				mcb->nextTransmit = 10;    // Channel deallocation warning 
			else 
			{
				mcb->ownerNodeID = MCAP_UNOWNED; // Reliquish our ownership 
				mcb->nextTransmit = 0;           // We're really, really done! 
				//TNFReleaseChannel(lcb->unspecifiedDeviceID, mcb->channel, 0);
				//fwIpObj->releaseChannel(mcb->channel, 0);
				priorArb = (ARB *) &lcb->multicastArb;
				while ((arb = priorArb->next) != NULL) 
				{
					if (arb->handle.multicast.channel == mcb->channel)
					{
						if (arb->deletionPending) 
						{
							priorArb->next = arb->next;   // Unlink the ARB 
							fwIpObj->deallocateCBlk(lcb, arb);     // And release it 
							continue;      // Important to recheck priorArb->next //
						} 
						else            // Revert to default channel //
							arb->handle.multicast.channel = DEFAULT_BROADCAST_CHANNEL;
					}
					priorArb = arb;      // Advance to next ARB in linked list 
				}
			}
		}
	}
	fwIpObj->cleanFWArbCache(lcb);
	
	//
	// Restart the watchdog timer
	//
	timerSource->setTimeoutMS(WATCHDOG_TIMER_MS);
}

/*!
	@function busReset
	@abstract Does busreset cleanup of the Link control block
	@param lcb - the firewire link control block for this interface.
	@param flags - ignored.
	@result void
*/	
void IOFireWireIP::busReset(LCB *lcb, UInt32 flags){
    CBLK	*cBlk;
    UInt32	i;
	RCB		*rcb;
	
    // Update our own max payload
    lcb->ownMaxPayload = fDevice->maxPackLog(true);
    
    // Update the nodeID
    fDevice->getNodeIDGeneration(lcb->busGeneration, lcb->ownNodeID);
	
    // Update the speed
    lcb->ownMaxSpeed = fDevice->FWSpeed();

    IOTakeLock(ipLock);
	cBlk = (CBLK*)&lcb->activeRcb;
	while ((cBlk = cBlk->next) != NULL) 
	{
		rcb		= (RCB*)cBlk;
        unlinkCBlk(&lcb->activeRcb, cBlk);
		networkStatAdd(&(getNetStats())->inputErrors);
		if(rcb->mBuf != NULL)
			freePacket(rcb->mBuf, kDelayFree);
		rcb->sourceID = 0;
		rcb->dgl = 0;
		rcb->mBuf = NULL;
		rcb->residual = 0;
        deallocateCBlk(lcb, cBlk);
	}
    IOUnlock(ipLock);
	releaseFreePackets();
   
    // Suspend MCAP for now
    for (i = 0; i <= LAST(lcb->mcapState); i++)
        lcb->mcapState[i].nextTransmit = 0;     

	updateBroadcastValues(true);

	updateLinkStatus();

	return;
}

#pragma mark -
#pragma mark еее IPv6 NDP routines  еее

bool IOFireWireIP::addNDPOptions(struct mbuf *m)
{
	struct icmp6_hdr			*icp	= NULL;
	struct ip6_hdr				*ip6;
	struct nd_neighbor_advert	*nd_na	= NULL;
	struct nd_neighbor_solicit	*nd_ns	= NULL;
 	
	IP1394_NDP	*fwndp	= NULL;
	BOOLEAN		modify  = false;
	
	UInt8			*buf = NULL;
	vm_address_t 	src = 0;
	struct mbuf		*temp = NULL;
	
	int		*pktLen = NULL;
	long	*length = NULL;
	
	if (m->m_flags & M_PKTHDR)
	{
		// IOLog("m_pkthdr.len  : %d\n", (UInt) m->m_pkthdr.len);
		pktLen = &m->m_pkthdr.len;
	}
	else
		return false;
			
	src = mtod(m, vm_offset_t);
	if(src == NULL)
		return false;

	// check whether len equals ether header
	if(m->m_len == sizeof(firewire_header))
	{
		temp = m->m_next;
		if(temp == NULL)
			return false;

		src =  mtod(temp, vm_offset_t);
		
		if(temp->m_len < (int)(sizeof(struct ip6_hdr)))
		{
			// IOLog("pkt too small %d\n", __LINE__);
			return false;
		}
		
		if(m_trailingspace(temp) < (int)sizeof(IP1394_NDP))
		{
			// IOLog("no space in mbuf %d\n", __LINE__);
			return false;
		}
		
		buf = (UInt8*)(src);
		length = &temp->m_len;
	}
	else
	{
		if(m->m_len < (int)(sizeof(firewire_header) + sizeof(struct ip6_hdr)))
		{
			// IOLog("pkt too small %d\n", __LINE__);
			return false;
		}	

		if(m_trailingspace(m) < (int)sizeof(IP1394_NDP))
		{
			// IOLog("no space in mbuf %d\n", __LINE__);
			return false;
		}

		buf = (UInt8*)(src + sizeof(firewire_header));
		length = &m->m_len;
	}

	

	// IOLog("IOFireWireIP: addNDPOptions+\n");
	// show type of ICMPV6 packets being sent
	ip6		= (struct ip6_hdr*)buf;
	icp		= (struct icmp6_hdr*)(ip6 + 1);
	nd_na	= (struct nd_neighbor_advert*)icp;
	nd_ns	= (struct nd_neighbor_solicit*)icp;

	if(nd_ns->nd_ns_type == ND_NEIGHBOR_SOLICIT)
	{		
		// neighbor solicitation
		// IOLog("IOFireWireIP: neighbor solicitation+\n");
		fwndp = (IP1394_NDP*)((UInt8*)nd_ns + sizeof(struct nd_neighbor_solicit));
		if(fwndp->type == 1)
		{
			modify = true;
			/*
			IOLog("+type = %d | +len = %d | +srclladdr = %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n", 
				fwndp->type, fwndp->len, fwndp->lladdr[0], fwndp->lladdr[1], fwndp->lladdr[2],
				fwndp->lladdr[3], fwndp->lladdr[4], fwndp->lladdr[5], fwndp->lladdr[6], fwndp->lladdr[7]);
			*/
		}
		if(fwndp->type == 2)
		{
			/*
			IOLog("+type = %d | +len = %d | +tgtlladdr = %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n", 
				fwndp->type, fwndp->len, fwndp->lladdr[0], fwndp->lladdr[1], fwndp->lladdr[2],
				fwndp->lladdr[3], fwndp->lladdr[4], fwndp->lladdr[5], fwndp->lladdr[6], fwndp->lladdr[7]);
			*/
		}
		// IOLog("IOFireWireIP: neighbor solicitation-\n");
	}
	
	if(nd_na->nd_na_type == ND_NEIGHBOR_ADVERT)
	{
		// neighbor advertisment
		// IOLog("IOFireWireIP: neighbor advertisment+\n");
		fwndp =  (IP1394_NDP*)((UInt8*)nd_na + sizeof(struct nd_neighbor_advert));
		
		// we don't touch this option
		if(fwndp->type == 1)
		{
			/*
			IOLog("+type = %d | +len = %d | +srclladdr = %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n", 
				fwndp->type, fwndp->len, fwndp->lladdr[0], fwndp->lladdr[1], fwndp->lladdr[2],
				fwndp->lladdr[3], fwndp->lladdr[4], fwndp->lladdr[5], fwndp->lladdr[6], fwndp->lladdr[7]);
			*/
		}
		
		if(fwndp->type == 2)
		{
			modify = true;
			/*
			IOLog("+type = %d | +len = %d | +tgtlladdr = %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n", 
				fwndp->type, fwndp->len, fwndp->lladdr[0], fwndp->lladdr[1], fwndp->lladdr[2],
				fwndp->lladdr[3], fwndp->lladdr[4], fwndp->lladdr[5], fwndp->lladdr[6], fwndp->lladdr[7]);
			*/
		}
	}
	
	if(modify)
	{
		fwndp->len = 3;       									// len in units of 8 octets
		bzero(fwndp->reserved, 6);								// reserved by the RFC 3146
		fwndp->senderMaxRec = fLcb->ownHardwareAddress.maxRec;	// Maximum payload (2 ** senderMaxRec)
		fwndp->sspd = fLcb->ownHardwareAddress.spd;				// Maximum speed
		fwndp->senderUnicastFifoHi = htons(fLcb->ownHardwareAddress.unicastFifoHi);	// Most significant 16 bits of FIFO address
		fwndp->senderUnicastFifoLo = htonl(fLcb->ownHardwareAddress.unicastFifoLo);	// Least significant 32 bits of FIFO address
		
		// IOLog("+len = %d \n", *length);
		 		
		*length += 8;
		*pktLen += 8;
		
		// IOLog("+len = %d | sizeof(fwndp) = %d\n", *length , sizeof(IP1394_NDP)); 

		return true;
	}

	return false;
}

void IOFireWireIP::updateNDPCache(void *buf, UInt16	*len)
{
	struct icmp6_hdr			*icp	= NULL;
	struct ip6_hdr				*ip6;
	struct nd_neighbor_advert	*nd_na	= NULL;
	struct nd_neighbor_solicit	*nd_ns	= NULL;
	
	ARB			*arb	= NULL;
	IP1394_NDP	*fwndp	= NULL;
	BOOLEAN		update  = false;
	
	// IOLog("IOFireWireIP: updateNDPCache+\n");
	ip6		= (struct ip6_hdr*)buf;
	icp		= (struct icmp6_hdr*)(ip6 + 1);
	nd_na	= (struct nd_neighbor_advert*)icp;
	nd_ns	= (struct nd_neighbor_solicit*)icp;

	if(nd_ns->nd_ns_type == ND_NEIGHBOR_SOLICIT)
	{		
		// neighbor solicitation
		// IOLog("IOFireWireIP: neighbor solicitation+\n");
		fwndp = (IP1394_NDP*)((UInt8*)nd_ns + sizeof(struct nd_neighbor_solicit));
		if(fwndp->type == 1)
		{
			update = true;
			/*
			IOLog("+type = %d | +len = %d | +srclladdr = %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n", 
				fwndp->type, fwndp->len, fwndp->lladdr[0], fwndp->lladdr[1], fwndp->lladdr[2],
				fwndp->lladdr[3], fwndp->lladdr[4], fwndp->lladdr[5], fwndp->lladdr[6], fwndp->lladdr[7]);
			*/
		}
		if(fwndp->type == 2)
		{
			/*
			IOLog("+type = %d | +len = %d | +tgtlladdr = %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n", 
				fwndp->type, fwndp->len, fwndp->lladdr[0], fwndp->lladdr[1], fwndp->lladdr[2],
				fwndp->lladdr[3], fwndp->lladdr[4], fwndp->lladdr[5], fwndp->lladdr[6], fwndp->lladdr[7]);
			*/
		}
		// IOLog("IOFireWireIP: neighbor solicitation-\n");
	}
	
	if(nd_na->nd_na_type == ND_NEIGHBOR_ADVERT)
	{
		// neighbor advertisment
		// IOLog("IOFireWireIP: neighbor advertisment+\n");
		fwndp =  (IP1394_NDP*)((UInt8*)nd_na + sizeof(struct nd_neighbor_advert));
		
		// we don't touch this option
		if(fwndp->type == 1)
		{
			/*
			IOLog("+type = %d | +len = %d | +srclladdr = %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n", 
				fwndp->type, fwndp->len, fwndp->lladdr[0], fwndp->lladdr[1], fwndp->lladdr[2],
				fwndp->lladdr[3], fwndp->lladdr[4], fwndp->lladdr[5], fwndp->lladdr[6], fwndp->lladdr[7]);
			*/
		}
		
		if(fwndp->type == 2)
		{
			update = true;
			/*
			IOLog("+type = %d | +len = %d | +tgtlladdr = %02x:%02x:%02x:%02x:%02x:%02x:%02x::%02x\n", 
				fwndp->type, fwndp->len, fwndp->lladdr[0], fwndp->lladdr[1], fwndp->lladdr[2],
				fwndp->lladdr[3], fwndp->lladdr[4], fwndp->lladdr[5], fwndp->lladdr[6], fwndp->lladdr[7]);
			*/
		}
	}
	
	
	if(update && fwndp != NULL && fwndp->len > 2)
	{
		arb = getArbFromFwAddr(fLcb, fwndp->lladdr);
		if(arb == NULL)
		{
			arb = (ARB*)allocateCBlk(fLcb);
			if(arb == NULL)
			{
				// IOLog("IOFireWireIP: No ARB's !!\n");
				return;
			}
			linkCBlk(&fLcb->unicastArb, arb);  
		}
		
		if(arb != NULL)
		{
			bcopy(fwndp->lladdr, &arb->eui64, FIREWIRE_ADDR_LEN);
			bcopy(fwndp->lladdr, arb->fwaddr, FIREWIRE_ADDR_LEN);
			arb->handle.unicast.maxRec = fwndp->senderMaxRec;
			arb->handle.unicast.spd = fwndp->sspd;
			arb->handle.unicast.unicastFifoHi = htons(fwndp->senderUnicastFifoHi);
			arb->handle.unicast.unicastFifoLo = htonl(fwndp->senderUnicastFifoLo); 
			arb->timer = 0;
			arb->datagramPending = FALSE;
			arb->handle.unicast.deviceID = getDeviceID(fLcb, arb->eui64, &arb->itsMac);
			
			// Reset the packet
			*len -= 8;
			fwndp->len = 2;       	// len in units of 8 octets
			fwndp->senderMaxRec = 0;
			fwndp->sspd = 0;
			fwndp->senderUnicastFifoHi = 0;
			fwndp->senderUnicastFifoLo = 0;
		}
	}
	// IOLog("IOFireWireIP: updateNDPCache-\n");

	return;
}

#pragma mark -
#pragma mark еее IOFireWireIP utility routines  еее

/*!
	@function initializeCBlk
	@abstract Initializes the memory for control blocks.
	@param  memorySize size of the control block.
	@result Returns pointer to the control block if successfull else NULL.
*/
void* IOFireWireIP::initializeCBlk(UInt32	memorySize)
{
    UInt32	*memoryPool = 0;
	UInt32	i	= 0;
	UInt32	*ptr;

    // memorySize = CBLK_MEMORY_SIZE;
	fMemoryPool = memoryPool = (UInt32*)IOMalloc(memorySize);

    if(memoryPool == NULL){
        IOLog("IOFireWireIP:: memory pool for CBLKS not allocated \n");
        return NULL;
    }
    
    // Set all of the area to 0
    memset(memoryPool, 0, sizeof(memorySize));

	for (i = 0; i < N_CBLK; i++) 
	{
		*((void **) memoryPool) = fLcb->freeCBlk;
		fLcb->freeCBlk = (struct cblk*)memoryPool;
		memoryPool = (UInt32 *) ((ULONG) memoryPool + MAX_CBLK_SIZE);
		memorySize -= MAX_CBLK_SIZE;
	}
	
    fLcb->cFreeCBlk = fLcb->minFreeCBlk = fLcb->nCBlk = N_CBLK;

	ptr = (UInt32*)&fLcb->mcapState;
	memset(ptr, 0, sizeof(MCB)*MAX_CHANNEL_DES);
    
    return memoryPool;
}

/*!
	@function allocateCBlk
	@abstract allocates a generic control block
	@param lcb - the firewire link control block for this interface.
	@result returns a preallocated control block
*/	
void *IOFireWireIP::allocateCBlk(LCB *lcb) {   /* Grab a free CBLK and return it */

    CBLK *cBlk;

    IOTakeLock(ipLock);

    if ((cBlk = lcb->freeCBlk) != NULL) {
        lcb->freeCBlk = cBlk->next;
        lcb->cFreeCBlk--;
        memset(cBlk, 0, MAX_CBLK_SIZE);
    }
   
    if ((lcb->freeCBlk == NULL) != (lcb->cFreeCBlk == 0))
        IOLog("allocateCBlk error: lcb->freeCBlk %08lX lcb->cFreeCBlk %04X\n\r",
                    (ULONG) lcb->freeCBlk, lcb->cFreeCBlk);
   
   IOUnlock(ipLock);
   
   lcb->minFreeCBlk = MIN(lcb->minFreeCBlk, lcb->cFreeCBlk);
   
   if (cBlk == NULL)
      IOLog("ERROR: No CBLK available!\n\r");
      
   return(cBlk);

}


/*!
	@function deallocateCBlk
	@abstract deallocates a generic control block
	@param lcb - the firewire link control block for this interface.
	@param cBlk - a control block i.e: arb, rcb or drb.
	@result returns a preallocated control block
*/	
void IOFireWireIP::deallocateCBlk(LCB *lcb, void *cBlk) {  

    IOTakeLock(ipLock);
   
    if (cBlk != NULL) {
        ((CBLK *) cBlk)->next = lcb->freeCBlk;
        lcb->freeCBlk = (CBLK *)cBlk;
        lcb->cFreeCBlk++;
    }
    
    if ((lcb->freeCBlk == NULL) != (lcb->cFreeCBlk == 0))
	{
        IOLog("deallocateCBlk error: lcb->freeCBlk %08lX lcb->cFreeCBlk %04X\n\r",
                                    (ULONG) lcb->freeCBlk, lcb->cFreeCBlk);
	}
    
    IOUnlock(ipLock);
}

/*!
	@function cleanFWArbCache
	@abstract cleans the Link control block's stale arb's. Invoked from the 
				ArpTimer to clean up FW specific ARB cleanup. Unresolved ARB's
				are returned to the free CBLKs
	@param lcb - the firewire link control block for this interface.
	@result void.
*/
void IOFireWireIP::cleanFWArbCache(LCB *lcb)
{
    ARB *arb = (ARB *) &lcb->unicastArb;

    IOTakeLock(ipLock);
        
    while ((arb = arb->next) != NULL)
	{
        if(arb->datagramPending == TRUE)
		{
            if(arb->timer > 1)
			{
                // The IP Address never resolved, lets decrement the timer
                arb->timer--;
            }
			else if (arb->timer == 1) 
			{
				IOLog("IOFireWireIP: unresolved arb cleaned up of ipaddress 0x%lx \n", arb->ipAddress);
				arb->handle.unicast.deviceID = kInvalidIPDeviceRefID;
                // time to clean up
                unlinkCBlk(&lcb->unicastArb, arb);
                deallocateCBlk(lcb, arb);
            }
        }
    }
   
    IOUnlock(ipLock);
}

/*!
	@function getDeviceID
	@abstract returns a fireWire device object for the GUID
	@param lcb - the firewire link control block for this interface.
    @param eui64 - global unique id of a device on the bus.
    @param itsMac - destination is Mac or not.
	@result Returns IOFireWireNub if successfull else 0.
*/
UInt32 IOFireWireIP::getDeviceID(LCB *lcb, UWIDE eui64, BOOLEAN *itsMac) {  

    // Returns DRB if EUI-64 matches
    DRB *drb = getDrbFromEui64(lcb, eui64);   

    // IOLog(" getDeviceID for EUI-64 0x%08X%08X DRB %x \n\r", eui64.hi, eui64.lo, drb);

    // Device reference ID already created
    if (drb != NULL) 
	{              
		*itsMac = drb->itsMac;
        // Just return it to caller
        return(drb->deviceID);        
    }
    else 
	{
		*itsMac = false;
        // Get an empty DRB
        return(kInvalidIPDeviceRefID);
    }
}

/*! 
	@function getArbFromFwAddr
	@abstract Locates the corresponding Unicast ARB (Address resolution block) for GUID
	@param lcb - the firewire link control block for this interface.
	@param FwAddr - global unique id of a device on the bus.
	@result Returns ARB if successfull else NULL.
*/
ARB *IOFireWireIP::getArbFromFwAddr(LCB *lcb, u_char *fwaddr) 
{
	ARB *arb = (ARB *) &lcb->unicastArb;
	
	IOTakeLock(ipLock);
	
	while ((arb = arb->next) != NULL)
		if (bcmp(fwaddr, arb->fwaddr, FIREWIRE_ADDR_LEN) == 0)
					break;
				
	IOUnlock(ipLock);
	
	return(arb);
}

/*!
	@function getDrbFromEui64
	@abstract Locates the corresponding DRB (device reference block) for GUID
	@param lcb - the firewire link control block for this interface.
	@param eui64 - global unique id of a device on the bus.
	@result Returns DRB if successfull else NULL.
*/
DRB *IOFireWireIP::getDrbFromEui64(LCB *lcb, UWIDE eui64) {  

    DRB *drb = (DRB *) &lcb->activeDrb;
    
    IOTakeLock(ipLock);
   
    while ((drb = drb->next) != NULL)
        if (drb->eui64.hi == eui64.hi && drb->eui64.lo == eui64.lo)
            break;
   
    IOUnlock(ipLock);
   
    return(drb);
}

/*!
	@function getDrbFromFwAddr
	@abstract Locates the corresponding DRB (device reference block) for GUID
	@param lcb - the firewire link control block for this interface.
	@param fwaddr - global unique id of a device on the bus.
	@result Returns DRB if successfull else NULL.
*/
DRB *IOFireWireIP::getDrbFromFwAddr(LCB *lcb, u_char *fwaddr) 
{  
    DRB *drb = (DRB *) &lcb->activeDrb;
    
    IOTakeLock(ipLock);
   
    while ((drb = drb->next) != NULL)
        if (bcmp(fwaddr, drb->fwaddr, FIREWIRE_ADDR_LEN) == 0)
            break;
   
    IOUnlock(ipLock);
   
    return(drb);
}

/*! 
	@function getArbFromEui64
	@abstract Locates the corresponding Unicast ARB (Address resolution block) for GUID
	@param lcb - the firewire link control block for this interface.
	@param eui64 - global unique id of a device on the bus.
	@result Returns ARB if successfull else NULL.
*/
ARB *IOFireWireIP::getArbFromEui64(LCB *lcb, UWIDE eui64) {  

    ARB *arb = (ARB *) &lcb->unicastArb;

    IOTakeLock(ipLock);
   
    while ((arb = arb->next) != NULL)
        if (arb->eui64.hi == eui64.hi && arb->eui64.lo == eui64.lo)
            break;
   
    IOUnlock(ipLock);
   
    return(arb);
}


/*!
	@function getDrbFromDeviceID
	@abstract Locates the corresponding DRB (Address resolution block) for IOFireWireNub
	@param lcb - the firewire link control block for this interface.
    @param deviceID - IOFireWireNub to look for.
	@result Returns DRB if successfull else NULL.
*/
DRB *IOFireWireIP::getDrbFromDeviceID(LCB *lcb, void *deviceID){   

    DRB *drb = (DRB *) &lcb->activeDrb;

    IOTakeLock(ipLock);
   
    while ((drb = drb->next) != NULL)
        if (drb->deviceID == (UInt32)deviceID)
            break;
    
    IOUnlock(ipLock);
   
    return(drb);
}

/*!
	@function getMulticastArb
	@abstract Locates the corresponding multicast ARB (Address resolution block) for ipaddress
	@param lcb - the firewire link control block for this interface.
	@param ipAddress - destination ipaddress to send the multicast packet.
	@result Returns ARB if successfull else NULL.
*/
ARB *IOFireWireIP::getMulticastArb(LCB *lcb, UInt32 ipAddress){  

    ARB *arb = (ARB *) &lcb->multicastArb;

    while ((arb = arb->next) != NULL)
        if (arb->ipAddress == ipAddress)
            break;
         
    return(arb);
}

/*!
	@function getUnicastArb
	@abstract Locates the corresponding unicast ARB (Address resolution block) for ipaddress
	@param lcb - the firewire link control block for this interface.
    @param ipAddress - destination ipaddress to send the unicast packet.
	@result Returns ARB if successfull else NULL.
*/
ARB *IOFireWireIP::getUnicastArb(LCB *lcb, UInt32 ipAddress){
    
    ARB *arb = (ARB *) &lcb->unicastArb;
    
	// IOLog(" getUnicastArb called %x \n\r", ipAddress);
	
    IOTakeLock(ipLock);
    
    while ((arb = arb->next) != NULL)
        if (arb->ipAddress == ipAddress)
            break;
    
    IOUnlock(ipLock);
    
	// IOLog(" getUnicastArb exit %x \n\r", arb);
	
    return(arb);
}

/*!
	@function getRcb
	@abstract Locates a reassembly control block.
	@param lcb - the firewire link control block for this interface.
    @param sourceID - source nodeid which generated the fragmented packet.
    @param dgl - datagram label for the fragmented packet.
	@result Returns RCB if successfull else NULL.
*/
RCB *IOFireWireIP::getRcb(LCB *lcb, UInt16 sourceID, UInt16 dgl){

    RCB *rcb = (RCB *) &lcb->activeRcb;

    IOTakeLock(ipLock);
   
    while ((rcb = rcb->next) != NULL) {
        if (rcb->sourceID == sourceID && rcb->dgl == dgl)
            break;
    }
   
    IOUnlock(ipLock);

    return(rcb);
}

/*!
	@function linkCBlk
	@abstract generic function to queue a control block to its corresponding list.
	@param queueHead - queuehead of the rcb, arb or drb.
	@param cBlk - control block of type rcb, arb or drb .
	@result void.
*/
void IOFireWireIP::linkCBlk(void *queueHead, void *cBlk) { 

    IOTakeLock(ipLock);
    
    ((CBLK *) cBlk)->next = (CBLK*)(*((void **) queueHead));
    *((void **) queueHead) = cBlk;
   
    IOUnlock(ipLock);
}

/*!
	@function unlinkCBlk
	@abstract generic function to dequeue a control block from its corresponding list.
	@param queueHead - queuehead of the rcb, arb or drb.
	@param cBlk - control block of type rcb, arb or drb .
	@result void.
*/
void IOFireWireIP::unlinkCBlk(void *queueHead, void *cBlk) { 
    
    IOTakeLock(ipLock);
    while (*((void **) queueHead) != NULL)
        if (*((void **) queueHead) == cBlk) {
            *((void **) queueHead) = ((CBLK *) cBlk)->next;
            ((CBLK *) cBlk)->next = NULL;
            break;
        } else
            queueHead = *((void **) queueHead);
   
   IOUnlock(ipLock);
}

// Display the reassembly control block
void IOFireWireIP::showRcb(RCB *rcb) {
	if (rcb != NULL) {
      IOLog("RCB %p\n\r", rcb);
      IOLog(" sourceID %04X dgl %u etherType %04X mBlk %p\n\r", rcb->sourceID, rcb->dgl, rcb->etherType, rcb->mBuf);
      IOLog(" datagram %p datagramSize %u residual %u\n\r", rcb->datagram, rcb->datagramSize, rcb->residual);
	}
}

void IOFireWireIP::showArb(ARB *arb) {

   u_char ipAddress[4];

   IOLog("ARB %p\n\r", arb);
   memcpy(ipAddress, &arb->ipAddress, sizeof(ipAddress));
   IOLog(" IP address %u.%u.%u.%u EUI-64 %08lX %08lX\n\r", ipAddress[0],
          ipAddress[1], ipAddress[2], ipAddress[3], arb->eui64.hi,
          arb->eui64.lo);
   IOLog(" fwAddr  %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n\r", arb->fwaddr[0],
          arb->fwaddr[1], arb->fwaddr[2], arb->fwaddr[3], arb->fwaddr[4],
          arb->fwaddr[5], arb->fwaddr[6], arb->fwaddr[7]);
   IOLog(" Handle: %08lX %02X %02X %04X%08lX\n\r", arb->handle.unicast.deviceID,
          arb->handle.unicast.maxRec, arb->handle.unicast.spd,
          arb->handle.unicast.unicastFifoHi, arb->handle.unicast.unicastFifoLo);
   IOLog(" Timer %d datagramPending %d\n\r", arb->timer, arb->datagramPending);

}

void IOFireWireIP::showHandle(TNF_HANDLE *handle) {

   if (handle->unicast.deviceID != kInvalidIPDeviceRefID)
      IOLog("   Unicast handle: %08lX %02X %02X %04X%08lX\n\r",
             handle->unicast.deviceID, handle->unicast.maxRec,
             handle->unicast.spd, handle->unicast.unicastFifoHi,
             handle->unicast.unicastFifoLo);
   else
      IOLog("   Multicast handle: 00000000 %02X %02X %02X %08lX\n\r",
             handle->multicast.maxRec, handle->multicast.spd,
             handle->multicast.channel, htonl(handle->multicast.groupAddress));

}

void IOFireWireIP::showDrb(DRB *drb) 
{
   if (drb != NULL) {
      IOLog("DRB 0x%p (associated with LCB 0x%p)\n\r", drb, drb->lcb);
      IOLog(" Device ID %08lX EUI-64 %08lX %08lX\n\r", drb->deviceID, drb->eui64.hi, drb->eui64.lo);
      IOLog(" timer %08lX maxPayload %d maxSpeed %d\n\r", drb->timer, drb->maxPayload, drb->maxSpeed);
   }
}

void IOFireWireIP::showLcb() {

   CBLK *cBlk;
   UNSIGNED cCBlk = 0;

   IOLog("LCB at %p (driver object at %p)\n\r", fLcb,
          fLcb->driverObject);
   IOLog(" Node ID %04X maxPayload %u maxSpeed %u busGeneration 0x%08lX\n\r",
          fLcb->ownNodeID, fLcb->ownMaxPayload,
          fLcb->ownMaxSpeed, fLcb->busGeneration);
   IOLog(" Free CBLKs %u (of %u in pool)\n\r", fLcb->cFreeCBlk,
          fLcb->nCBlk);
   IOLog(" CBLK Low water mark %u\n\r", fLcb->minFreeCBlk);
   
   // Display the arb's
   if (fLcb->unicastArb == NULL)
      IOLog(" No unicast ARBs\n\r");
   else {
      IOLog(" Unicast ARBs\n\r");
      cBlk = (CBLK*)&fLcb->unicastArb;
      while ((cBlk = cBlk->next) != NULL) {
         cCBlk++;
         IOLog("  %p\n\r", cBlk);
		 showArb((ARB*)cBlk);
      }
   }
   
   // Display the multicast arb's
   if (fLcb->multicastArb == NULL)
      IOLog(" No multicast ARBs\n\r");
   else {
      IOLog(" Multicast ARBs\n\r");
      cBlk = (CBLK*)&fLcb->multicastArb;
      while ((cBlk = cBlk->next) != NULL) {
         cCBlk++;
         IOLog("  %p\n\r", cBlk);
      }
   }
   
   // Display the active DRB
   if (fLcb->activeDrb == NULL)
      IOLog(" No active DRBs\n\r");
   else {
      IOLog(" Active DRBs\n\r");
      cBlk = (CBLK*)&fLcb->activeDrb;
      while ((cBlk = cBlk->next) != NULL) {
         cCBlk++;
         IOLog("  %p\n\r", cBlk);
		 showDrb((DRB*)cBlk);
      }
   }
   
   // Display the active RCB
   if (fLcb->activeRcb == NULL)
      IOLog(" No active RCBs\n\r");
   else {
      IOLog(" Active RCBs\n\r");
      cBlk = (CBLK*)&fLcb->activeRcb;
      while ((cBlk = cBlk->next) != NULL) {
         cCBlk++;
         IOLog("  %p\n\r", cBlk);
		 showRcb((RCB*)cBlk);
      }
   }
   
   IOLog(" %u CBLKs in use\n\r", cCBlk);
   if (cCBlk + fLcb->cFreeCBlk != fLcb->nCBlk)
      IOLog(" CBLK accounting error!\n\r");

}

void IOFireWireIP::updateStatistics()
{
	if(fBroadcastReceiveClient != NULL)
	{
		fIsoRxOverrun = fBroadcastReceiveClient->getOverrunCounter();
	}
}

#pragma mark -
#pragma mark еее mbuf utility routines еее

//---------------------------------------------------------------------------
// Allocates a mbuf chain. Each mbuf in the chain is aligned according to
// the constraints that are currently ignored.
// The last mbuf in the chain will be guaranteed to be length aligned if 
// the 'size' argument is a multiple of the length alignment.
//
// The m->m_len and m->pkthdr.len fields are updated by this function.
// This allows the driver to pass the mbuf chain obtained through this
// function to the IOMbufMemoryCursor object directly.
//
// If (size + alignments) is smaller than MCLBYTES, then this function
// will always return a single mbuf header or cluster.
//
// The allocation is guaranteed not to block. If a packet cannot be
// allocated, this function will return NULL.

#define IO_APPEND_MBUF(head, tail, m) {   \
    if (tail) {                           \
        (tail)->m_next = (m);             \
        (tail) = (m);                     \
    }                                     \
    else {                                \
        (head) = (tail) = (m);            \
        (head)->m_pkthdr.len = 0;         \
    }                                     \
}

#define IO_ALIGN_MBUF_START(m, mask) {                                 \
    if ( (mask) & mtod((m), vm_address_t) ) {                          \
        (m)->m_data = (caddr_t) (( mtod((m), vm_address_t) + (mask) )  \
                                 & ~(mask));                           \
    }                                                                  \
}

#define IO_ALIGN_MBUF(m, size, smask, lmask) {   \
    IO_ALIGN_MBUF_START((m), (smask));           \
    (m)->m_len = ((size) - (smask)) & ~(lmask);  \
}

static struct mbuf * allocateMbuf( UInt32 size,
                                   UInt32 how,
                                   UInt32 smask,
                                   UInt32 lmask )
{
    struct mbuf * m;
    struct mbuf * head = 0;
    struct mbuf * tail = 0;
    UInt32        capacity;

    while ( size )
    {
        // Allocate a mbuf. For the initial mbuf segment, allocate a
        // mbuf header.

        if ( head == 0 )
        {
            MGETHDR( m, how, MT_DATA );
            capacity = MHLEN;
        }
        else
        {
            MGET( m, how, MT_DATA );
            capacity = MLEN;
        }

        if ( m == 0 ) goto error;  // mbuf allocation error

        // Append the new mbuf to the tail of the mbuf chain.

        IO_APPEND_MBUF( head, tail, m );

        // If the remaining size exceed the buffer size of a normal mbuf,
        // then promote it to a cluster. Currently, the cluster size is
        // fixed to MCLBYTES bytes.

        if ( ( size + smask + lmask ) > capacity )
        {
            MCLGET( m, how );
            if ( (m->m_flags & M_EXT) == 0 ) goto error;
            capacity = MCLBYTES;
        }

        // Align the mbuf per driver's specifications.

        IO_ALIGN_MBUF( m, capacity, smask, lmask );

        // Compute the number of bytes needed after accounting for the
        // current mbuf allocation.

        if ( (UInt) m->m_len > size )
            m->m_len = size;

        size -= m->m_len;

        // Update the total length in the packet header.

        head->m_pkthdr.len += m->m_len;
    }

    return head;

error:
    if ( head ) m_freem(head);
    return 0;
}

static struct mbuf * getPacket( UInt32 size,
                                UInt32 how,
                                UInt32 smask,
                                UInt32 lmask )
{
    struct mbuf * m = NULL;

    do {
        // Handle the simple case where the requested size is small
        // enough for a single mbuf. Otherwise, go to the more costly
        // route and call the generic mbuf allocation routine.

        if ( ( size + smask ) <= MCLBYTES )
        {
            if ( ( size + smask ) > MHLEN )
            {
                /* MGETHDR+MCLGET under one single lock */
                m = m_getpackets( 1, 1, how );
            }
            else
            {
                MGETHDR( m, how, MT_DATA );
            }
            if ( m == 0 ) break;

            // Align start of mbuf buffer.

            IO_ALIGN_MBUF_START( m, smask );

            // No length adjustment for single mbuf.
            // Driver gets what it asked for.

            m->m_pkthdr.len = m->m_len = size;
        }
        else
        {
            m = allocateMbuf( size, how, smask, lmask );
        }
    }
    while ( false );

    return m;
}

void moveMbufWithOffset(SInt32 tempOffset, struct mbuf **srcm, vm_address_t *src, SInt32 *srcLen)
{
    struct mbuf *temp = NULL;

	for(;;) 
	{

		if(tempOffset == 0)
			break;

		if(*srcm == NULL)
			break;

		if(*srcLen < tempOffset) 
		{
			tempOffset = tempOffset - *srcLen;
			temp = (*srcm)->m_next; 
			*srcm = temp;
			if(*srcm != NULL)
				*srcLen = (*srcm)->m_len;
			continue;
		} 
		else if (*srcLen > tempOffset) 
		{
			*srcLen = (*srcm)->m_len;
			*src = mtod(*srcm, vm_offset_t);
			*src += tempOffset;
			*srcLen -= tempOffset;
			break;
		} 
		else if (*srcLen == tempOffset) 
		{
			temp = (*srcm)->m_next; 
			*srcm = temp;
			if(*srcm != NULL) 
			{
				*srcLen = (*srcm)->m_len;
				*src = mtod(*srcm, vm_offset_t);
			}
			break;
		}
	}
}

/*!
	@function getMBuf
	@abstract Allocate Mbuf of required size.
	@param size - required size for the allocated mbuf.
	@result NULL if failed else a valid mbuf.
*/
struct mbuf * IOFireWireIP::getMBuf(UInt32 size)
{
	struct mbuf *m;

    IOTakeLock(ipLock);

	m = getPacket(size, M_DONTWAIT, 0, 0);

	if (m != NULL)
    {
		// Set the interface pointer to our ifnet pointer
		m->m_pkthdr.rcvif = ifp; 
	}

    IOUnlock(ipLock);

	return m;
}


/*!
	@function bufferToMbuf
	@abstract Copies buffer to Mbuf.
	@param m - destination mbuf.
	@param offset - offset into the mbuf data pointer.
	@param srcbuf - source buf.
	@param srcbufLen - source buffer length.
	@result bool - true if success else false.
*/
bool IOFireWireIP::bufferToMbuf(struct mbuf *m, 
								UInt32 offset, 
								UInt8  *srcbuf, 
								UInt32 srcbufLen)
{
	vm_address_t src, dst;
    SInt32 srcLen, dstLen, copylen, tempOffset;
    struct mbuf *temp;
    struct mbuf *srcm;

    IOTakeLock(ipLock);

    temp = NULL;
    srcm = NULL;

	// Get the source
	srcm = m; 
	srcLen = srcm->m_len;
    src = mtod(srcm, vm_offset_t);

	//IOLog("bufferToMbuf+\n\r");
	
	//
	// Mbuf manipulated to point at the correct offset
	//
	tempOffset = offset;

	moveMbufWithOffset(tempOffset, &srcm, &src, &srcLen);

	// Modify according to our fragmentation
	// src += *offset;
	// srcLen -= *offset;

	dstLen = srcbufLen;
    copylen = dstLen;
	dst = (vm_address_t)srcbuf;
	
	
    for (;;) {
	
		//IOLog("  offset %d mbuflen %d pktlen %d \n\r", offset, srcLen, dstLen);


        if (srcLen < dstLen) {

            // Copy remainder of buffer to current mbuf upto m_len.
            BCOPY(dst, src, srcLen);
            dst += srcLen;
            dstLen -= srcLen;
			copylen -= srcLen;
			// set the offset
			// offset = offset + srcLen; 
			
			//IOLog("srcLen < dstLen : copyLen = %d\n", copylen);

			if(copylen == 0){
				// set the new mbuf to point to the new chain
				temp = srcm->m_next; 
				srcm = temp;
				break;
			}
            // Move on to the next source mbuf.
            temp = srcm->m_next; assert(temp);
            srcm = temp;
            srcLen = srcm->m_len;
            src = mtod(srcm, vm_offset_t);
        }
        else if (srcLen > dstLen) {
			//
            // Copy some of buffer to src mbuf, since mbuf 
			// has more space.
			//
            BCOPY(dst, src, dstLen);
            src += dstLen;
            srcLen -= dstLen;
            copylen -= dstLen;
			// set the offset
			// offset = offset + dstLen; 
            // Move on to the next destination mbuf.
			// IOLog("srcLen > dstLen : copyLen = %d\n", copylen);
			
			if(copylen == 0){
				// set the new mbuf to point to the new chain
	//			temp = srcm; 
	//			srcm = temp;
				break;
			}
        }
        else {  /* (srcLen == dstLen) */
            // copy remainder of src into remaining space of current mbuffer
            BCOPY(dst, src, srcLen);
			copylen -= srcLen;

			// IOLog("srcLen == dstLen : copyLen = %d\n", copylen);
			
			if(copylen == 0){
				// set the offset
				// offset = 0; 
				// set the new mbuf to point to the new chain
				temp = srcm->m_next; 
				srcm = temp;
				break;
			}
            // Free current mbuf and move the current onto the next
            srcm = srcm->m_next;

            // Do we have any data left to copy?
            if (dstLen == 0)
				break;

            srcLen = srcm->m_len;
            src = mtod(srcm, vm_offset_t);
        }
    }
	// IOLog("bufferToMbuf-\n\r");
    IOUnlock(ipLock);
	
	return true;
}

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
struct mbuf *IOFireWireIP::mbufTobuffer(struct mbuf *m, 
								UInt32 *offset, 
								UInt8  *dstbuf, 
								UInt32 dstbufLen, 
								UInt32 length)
{
	vm_address_t src, dst;
    SInt32 srcLen, dstLen, copylen, tempOffset;
    struct mbuf *temp = NULL;
    struct mbuf *srcm = NULL;

	// Get the source
	srcm = m; 
	srcLen = srcm->m_len;
    src = mtod(srcm, vm_offset_t);
	
	//
	// Mbuf manipulated to point at the correct offset
	//
	tempOffset = *offset;

	moveMbufWithOffset(tempOffset, &srcm, &src, &srcLen);

	// Modify according to our fragmentation
	dstLen = length;
    copylen = dstLen;
	dst = (vm_address_t)dstbuf;
	
	
    for (;;) {
	
		//IOLog("  mbuf->buf::offset %d srcLen %d dstLen %d \n\r", *offset, srcLen, dstLen);

        if (srcLen < dstLen) {

            // Copy remainder of src mbuf to current dst.
            BCOPY(src, dst, srcLen);
            dst += srcLen;
            dstLen -= srcLen;
			copylen -= srcLen;
			// set the offset
			*offset = *offset + srcLen; 
			
			if(copylen == 0){
				// set the new mbuf to point to the new chain
				temp = srcm->m_next; 
				srcm = temp;
				break;
			}
            // Move on to the next source mbuf.
            temp = srcm->m_next; assert(temp);
            srcm = temp;
            srcLen = srcm->m_len;
            src = mtod(srcm, vm_offset_t);
        }
        else if (srcLen > dstLen) {
            // Copy some of src mbuf to remaining space in dst mbuf.
            BCOPY(src, dst, dstLen);
            src += dstLen;
            srcLen -= dstLen;
            copylen -= dstLen;
			// set the offset
			*offset = *offset + dstLen; 

            // Move on to the next destination mbuf.
			if(copylen == 0){
				// set the new mbuf to point to the new chain
//				temp = srcm; 
//				srcm = temp;
				break;
			}
        }
        else {  /* (srcLen == dstLen) */
            // copy remainder of src into remaining space of current dst
            BCOPY(src, dst, srcLen);
			copylen -= srcLen;
			
			if(copylen == 0){
				// set the offset
				*offset = 0; 
				// set the new mbuf to point to the new chain
				temp = srcm->m_next; 
				srcm = temp;
				break;
			}
            // Free current mbuf and move the current onto the next
            srcm = srcm->m_next;

            // Do we have any data left to copy?
            if (dstLen == 0)
				break;

            srcLen = srcm->m_len;
            src = mtod(srcm, vm_offset_t);
        }
    }
	return temp;
}

/*!
	@function freeMBuf
	@abstract free the allocated mbuf.
	@param struct mbuf *m.
	@result void.
*/
void IOFireWireIP::freeMBuf(struct mbuf *m) {

    IOTakeLock(ipLock);

	//_logMbuf(m);
	m_freem(m);

    IOUnlock(ipLock);
}

//---------------------------------------------------------------------------
// Used for debugging only. Log the mbuf fields.
void _logMbuf(struct mbuf * m)
{
	UInt8	*bytePtr;
	
    if (!m) {
        IOLog("logMbuf: NULL mbuf\n");
        return;
    }
    
    while (m) {
        IOLog("m_next   : %08x\n", (UInt) m->m_next);
        IOLog("m_nextpkt: %08x\n", (UInt) m->m_nextpkt);
        IOLog("m_len    : %d\n",   (UInt) m->m_len);
        IOLog("m_data   : %08x\n", (UInt) m->m_data);
        IOLog("m_type   : %08x\n", (UInt) m->m_type);
        IOLog("m_flags  : %08x\n", (UInt) m->m_flags);
        
        if (m->m_flags & M_PKTHDR)
            IOLog("m_pkthdr.len  : %d\n", (UInt) m->m_pkthdr.len);

        if (m->m_flags & M_EXT) {
            IOLog("m_ext.ext_buf : %08x\n", (UInt) m->m_ext.ext_buf);
            IOLog("m_ext.ext_size: %d\n", (UInt) m->m_ext.ext_size);
        }
		
		IOLog("m_data -> \t\t") ;
		
		if(m->m_data != NULL){
		
			bytePtr = (UInt8*)m->m_data;
						
			for(SInt32 index=0; index < min(m->m_len, 12); index++)
			{
				if ((index & 0x3) == 0)
				{
					IOLog(" ") ;
					if ((index & 0x7) == 0)
					{
						IOLog("   ") ;
						if ((index & 0xF) == 0)
							IOLog("\n\t\t") ;
					} 
				}
				IOLog("%02X", (unsigned char)bytePtr[index]) ;
			}
			IOLog("\n\n") ;
		}
        
        m = m->m_next;
    }
    IOLog("\n");
}
void _logPkt(void *pkt, UInt16 len)
{
	UInt8 	*bytePtr;

	///
	// start log code
	///
	bytePtr = (UInt8*)pkt;
	
	IOLog("pkt {\n") ;

	for(SInt32 index=0; index<len; index++)
	{
		if ((index & 0x3) == 0)
		{
			IOLog(" ") ;
			if ((index & 0x7) == 0)
			{
				IOLog("   ") ;
				if ((index & 0xF) == 0)
					IOLog("\n\t\t") ;
			} 
		}
		IOLog("%02X", (unsigned char)bytePtr[index]) ;
	}
	IOLog("}\n\n") ;
	//////
	// end log code
	//////
}

#pragma mark -
#pragma mark еее firewire irm routines еее

UInt32 IOFireWireIP::initIsocMgmtCmds()
{
	// Should be moved to start and free methods !!
	fReadCmd = new IOFWReadQuadCommand;
    if(fReadCmd)
        fReadCmd->initAll(fControl, 0, FWAddress(), NULL, 0, NULL, NULL);

    fLockCmd = new IOFWCompareAndSwapCommand;
    if(fLockCmd)
        fLockCmd->initAll(fControl, 0, FWAddress(), NULL, NULL, 0, NULL, NULL);
	
	return fReadCmd != NULL && fLockCmd != NULL; 
}

void IOFireWireIP::freeIsocMgmtCmds()
{
	if(fReadCmd) 
	{
		//IOLog("%d fReadCmd retain count %p fReadCmd %p %d\n", fReadCmd->getRetainCount(),  fReadCmd, &fReadCmd, __LINE__);
        fReadCmd->release();
	}
    
	if(fLockCmd)
	{
		//IOLog("%d fLockCmd retain count %p fLockCmd %p %d\n", fLockCmd->getRetainCount(),  fLockCmd, &fLockCmd, __LINE__);
		fLockCmd->release();
	}
}

void IOFireWireIP::getChan31()
{
	UInt32 		newVal ;
	FWAddress 	addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable) ;
	UInt32 		oldIRM[3] ;
	UInt32 		channel ;
	IOReturn	err = kIOReturnSuccess;
	//UInt64 		inAllowedChans;
    UInt32 		generation;	
	
	// contact irm and allocate one of the available channel
	fControl->getIRMNodeID(generation, addr.nodeID);

	fReadCmd->reinit(generation, addr, oldIRM, 3);
	fReadCmd->setMaxPacket(4);		
	err = fReadCmd->submit();

	channel = 31;
	
	if (!err)
	{
		UInt32*		oldPtr;

		// Claim channel
		if(channel < 32)
		{
			addr.addressLo = kCSRChannelsAvailable31_0;
			oldPtr = &oldIRM[1];
			newVal = *oldPtr & ~(1<<(31 - channel));
		}
		else
		{
			addr.addressLo = kCSRChannelsAvailable63_32;
			oldPtr = &oldIRM[2];
			newVal = *oldPtr & ~( (UInt64)1 << (63 - channel) );
		}
	
		fLockCmd->reinit(generation, addr, oldPtr, &newVal, 1);
		err = fLockCmd->submit();
		
		if (!err && !fLockCmd->locked(oldPtr))
			err = kIOReturnCannotLock ;
	}
}

//
// Receive async stream packets
// Just set the required channel in channelRxMask variable  
//
UInt32 IOFireWireIP::acquireChannel(UInt32 *pChannel, Boolean autoReAllocate, UInt32	resourceAllocationFlags)
{
	UInt32 		newVal ;
	FWAddress 	addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable) ;
	UInt32 		oldIRM[3] ;
	UInt32 		channel = 0;
	IOReturn	err = kIOReturnSuccess;
	UInt64 		inAllowedChans;
    UInt32 		generation;	
	
	inAllowedChans = ~(UInt64)0;

	IOLog(" %s %d\n", __FILE__, __LINE__);
	
	if(autoReAllocate & kDoNotAllocate) // its for receive
	{
		// set the required channel bit in channelRxMask
		
		//
		// we are right now not worried about who allocated
		// the channel, just start listening
		//
	}
	else // its for send
	{
		// contact irm and allocate one of the available channel
		fControl->getIRMNodeID(generation, addr.nodeID);
	
		fReadCmd->reinit(generation, addr, oldIRM, 3);
		fReadCmd->setMaxPacket(4);		
		err = fReadCmd->submit();

		if (!err)
		{
			// mask inAllowedChans by channels IRM has available
			inAllowedChans &= (UInt64)(oldIRM[2]) | ((UInt64)oldIRM[1] << 32);
		}
#ifdef FIREWIRETODO // Should be moved to channels available function and can set the available channels
	
		// if we have an error here, the bandwidth wasn't allocated
		if (!err)
		{
			for(channel=0; channel<64; channel++)
			{
				if( inAllowedChans & ((UInt64)1 << ( 63 - channel )) )
					break;
			}

			if(channel == 64) {
				IOLog("IOFireWireIP: No resources, will use 0x1f chan");
				err = kIOReturnNoResources;
			}
		}
#endif
		if (!err)
		{
			// mask inAllowedChans by channels IRM has available
			inAllowedChans &= (UInt64)(oldIRM[2]) | ((UInt64)oldIRM[1] << 32);
		}

		// if we have an error here, the bandwidth wasn't allocated
		if (!err)
		{
			channel = *pChannel;
			if( inAllowedChans & ((UInt64)1 << ( 63 - channel )) != 0){
				IOLog("IOFireWireIP: No resources, will use 0x1f chan");
				err = kIOReturnNoResources;
			}
		}
	
		if (!err)
		{
			UInt32*		oldPtr;

			// Claim channel
			if(channel < 32)
			{
				addr.addressLo = kCSRChannelsAvailable31_0;
				oldPtr = &oldIRM[1];
				newVal = *oldPtr & ~(1<<(31 - channel));
			}
			else
			{
				addr.addressLo = kCSRChannelsAvailable63_32;
				oldPtr = &oldIRM[2];
				newVal = *oldPtr & ~( (UInt64)1 << (63 - channel) );
			}
		
			fLockCmd->reinit(generation, addr, oldPtr, &newVal, 1);
			err = fLockCmd->submit();
			if (!err && !fLockCmd->locked(oldPtr))
				err = kIOReturnCannotLock ;
		}

		if (!err)
		{ 
			// *pChannel = channel;
			if(channel >= 32)
				fReAllocateChannel.hi |= (kChannelPrime >> (channel - 32));
			else
				fReAllocateChannel.lo |= (kChannelPrime >> channel);
		}
	}
	
	return err;
}

void IOFireWireIP::releaseChannel(UInt32 channel,UInt32 releaseFlags)
{
	FWAddress addr(kCSRRegisterSpaceBaseAddressHi, kCSRBandwidthAvailable);
    UInt32 generation = 0;
    UInt32 newVal;
    UInt32 oldVal;
    IOReturn result = kIOReturnSuccess;
    bool tryAgain;
	bool claim = false;
	UInt32 fChannel = channel;

	IOLog(" %s %d\n", __FILE__, __LINE__);

	if(channel == DEFAULT_BROADCAST_CHANNEL)
		return;

	//
	// If we allocated with the IRM then we free it only
	// happens when we advertise and transmit packet in 
	// a specific channel 
	//
	if (!(releaseFlags & kDoNotDeallocate)) { 
		if(fChannel != 64) {
			UInt32 mask;
			if(fChannel <= 31) {
				addr.addressLo = kCSRChannelsAvailable31_0;
				mask = 1 << (31-fChannel);
			}
			else {
				addr.addressLo = kCSRChannelsAvailable63_32;
				mask = 1 << (63-fChannel);
			}
			fReadCmd->reinit(generation, addr, &oldVal, 1);
			result = fReadCmd->submit();
			if(kIOReturnSuccess != result) {
				return;
			}
			do {
				if(claim) {
					newVal = oldVal & ~mask;
					if(newVal == oldVal) {
						// Channel already allocated!
						result = kIOReturnNoSpace;
						break;
					}
				}
				else {
					newVal = oldVal | mask;
				}
				fLockCmd->reinit(generation, addr, &oldVal, &newVal, 1);
				result = fLockCmd->submit();
				if(kIOReturnSuccess != result) {
					IOLog("channel update result 0x%x\n", result);
					break;
				}
				tryAgain = !fLockCmd->locked(&oldVal);
			} while (tryAgain);
		}
		if(channel >= 32)
			fReAllocateChannel.hi &= ~(kChannelPrime >> (channel - 32));
		else
			fReAllocateChannel.lo &= ~(kChannelPrime >> channel);
	}
	else 
	{
		// Free the mask for the receive async stream client channels
		
	}
}

//
// Called from bus reset function to reallocate the previosuly allocated
// channels
//
void IOFireWireIP::reclaimChannels()
{
	UInt32 channel = 0;
	UInt32 chnlBit;
	UInt32 *pChannelReg;
	IOReturn err = kIOReturnSuccess ;
	
	pChannelReg = &fReAllocateChannel.lo;
	for(channel = 0; ;)
	{
		for (chnlBit = kChannelPrime; chnlBit != 0; chnlBit >>= 1, channel++) 
		{
			if ((*pChannelReg & chnlBit) != 0) 
			{
				IOLog(" %s %d\n", __FILE__, __LINE__);
				// Reallocate the channel
				err = acquireChannel(&channel, TRUE, 0);
				
				if(err != kIOReturnSuccess)
					channelNotification(channel, kDoNotNotifyOnFailure);
				else
					channelNotification(channel, kNotifyOnSuccess);
			}
		}
		if (pChannelReg == &fReAllocateChannel.hi)
			break;

		pChannelReg = &fReAllocateChannel.hi;
	}
}

/*-----------------------------------------------------------------------------
 This procedure is called, rain or shine, after a bus reset necessitates the
 reallocation of any channels for which we are the MCAP owner. Note that the
 channel parameter has a high-order bit that encodes the result, success or
 failure of the reallocation. First, see if we still own the channel. If we
 don't, we must cease our attempts to reallocate it in the future but nothing
 else need be done. Otherwise, if we are the owner and reallocation succeeded,
 announce it to the world with an MCAP advertisement as soon as we can. If
 reallocation failed, drastic steps are called for. Release the channel, mark
 the channel unowned, stop receiving asynchronous streams on the channel and
 suppress future MCAP transmission. Then sift through the multicast ARBs for
 any that use the channel we just lost; the channel number reverts to 31, the
 default broadcast channel. If an ARB had been marked for deletion, though,
 just get rid of it. */
void IOFireWireIP::channelNotification(UInt32 channel, UInt32 flags) {
	ARB *arb, *priorArb;
	MCB *mcb;
	IOFWAsyncStreamRxCommand *asyncStreamRxClient;
	
	IOLog(" %s %d\n", __FILE__, __LINE__);
	
	mcb = &fLcb->mcapState[channel];
	
	if (mcb->ownerNodeID != fLcb->ownNodeID)	// Are we no longer the owner?
		releaseChannel(mcb->channel, kDoNotDeallocate);
	else if (flags & kNotifyOnSuccess)			// Was the reallocation OK?
		mcb->nextTransmit = 1;					// Broadcast MCAP advertisement ASAP */
	else {										// Trouble in River City! Abandon ship! */
		releaseChannel(mcb->channel, kDoNotDeallocate);
		mcb->ownerNodeID = MCAP_UNOWNED;		// Relinquish our claim...
		
		asyncStreamRxClient = (IOFWAsyncStreamRxCommand *)mcb->asyncStreamID;
		if(asyncStreamRxClient != NULL)
			asyncStreamRxClient->release();
		
		//TNFRemoveAsyncStreamClient(fLcb->clientID, mcb->asyncStreamID);
		mcb->asyncStreamID = kInvalidAsyncStreamRefID;
		mcb->expiration = mcb->nextTransmit = mcb->finalWarning = 0;
		priorArb = (ARB *) &fLcb->multicastArb; /* Tidy up this channel's ARBs */
		while ((arb = priorArb->next) != NULL) {
			if (arb->handle.multicast.channel == mcb->channel)
				if (arb->deletionPending) {
					priorArb->next = arb->next;   /* Unlink the ARB */
					deallocateCBlk(fLcb, arb);     /* And release it */
					continue;      /* Important to recheck priorArb->next */
				} else            /* Revert to default channel */
					arb->handle.multicast.channel = DEFAULT_BROADCAST_CHANNEL;
			priorArb = arb;      /* Advance to next ARB in linked list */
		}
		mcb->groupCount = 0;    /* No ARBs left that reference the channel */
	}
}
