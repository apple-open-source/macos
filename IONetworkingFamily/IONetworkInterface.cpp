/*
 * Copyright (c) 1998-2008 Apple Inc. All rights reserved.
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
/*
 * IONetworkInterface.cpp
 *
 * HISTORY
 * 8-Jan-1999       Joe Liu (jliu) created.
 *
 */

extern "C" {
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/dlil.h>
#include <net/if_dl.h>
#include <net/kpi_interface.h>
#include <sys/kern_event.h>

#define _IP_VHL

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
}

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include "IONetworkUserClient.h"
#include "IONetworkStack.h"
#include "IONetworkControllerPrivate.h"
#include "IONetworkDebug.h"
#include "IOMbufQueue.h"

#include <TargetConditionals.h>

//------------------------------------------------------------------------------

#define super IOService

OSDefineMetaClassAndAbstractStructors( IONetworkInterface, IOService )
OSMetaClassDefineReservedUsed( IONetworkInterface,  5);
OSMetaClassDefineReservedUsed( IONetworkInterface,  6);
OSMetaClassDefineReservedUsed( IONetworkInterface,  7);
OSMetaClassDefineReservedUsed( IONetworkInterface,  8);
OSMetaClassDefineReservedUsed( IONetworkInterface,  9);
OSMetaClassDefineReservedUsed( IONetworkInterface, 10);
OSMetaClassDefineReservedUnused( IONetworkInterface, 11);
OSMetaClassDefineReservedUnused( IONetworkInterface, 12);
OSMetaClassDefineReservedUnused( IONetworkInterface, 13);
OSMetaClassDefineReservedUnused( IONetworkInterface, 14);
OSMetaClassDefineReservedUnused( IONetworkInterface, 15);

//------------------------------------------------------------------------------
// Macros

#define IFNET_TO_THIS(x)        ((IONetworkInterface *) ifnet_softc(ifp))

#define WAITING_FOR_DETACH(n)   ((n)->_clientVar[1])

#define _unit                   _reserved->unit
#define _type                   _reserved->type
#define _mtu                    _reserved->mtu
#define _flags                  _reserved->flags
#define _eflags                 _reserved->eflags
#define _addrlen                _reserved->addrlen
#define _hdrlen                 _reserved->hdrlen
#define _outputQueueModel       _reserved->outputQueueModel
#define _inputDeltas            _reserved->inputDeltas
#define _driverStats            _reserved->driverStats
#define _lastDriverStats        _reserved->lastDriverStats
#define _publicLock             _reserved->publicLock
#define _remote_NMI_pattern     _reserved->remote_NMI_pattern
#define _remote_NMI_len         _reserved->remote_NMI_len
#define _controller             _reserved->controller
#define _configFlags            _reserved->configFlags
#define _txRingSize             _reserved->txRingSize
#define _txPullOptions          _reserved->txPullOptions
#define _txQueueSize            _reserved->txQueueSize
#define _txSchedulingModel      _reserved->txSchedulingModel
#define _txThreadState          _reserved->txThreadState
#define _txThreadFlags          _reserved->txThreadFlags
#define _txThreadSignal         _reserved->txThreadSignal
#define _txThreadSignalLast     _reserved->txThreadSignalLast
#define _txStartThread          _reserved->txStartThread
#define _txStartAction          _reserved->txStartAction
#define _txWorkLoop             _reserved->txWorkLoop
#define _rxRingSize             _reserved->rxRingSize
#define _rxPollOptions          _reserved->rxPollOptions
#define _rxPollModel            _reserved->rxPollModel
#define _rxPollAction           _reserved->rxPollAction
#define _rxCtlAction            _reserved->rxCtlAction
#define _rxPollEmpty            _reserved->rxPollEmpty
#define _rxPollTotal            _reserved->rxPollTotal
#define _peqHandler             _reserved->peqHandler
#define _peqTarget              _reserved->peqTarget
#define _peqRefcon              _reserved->peqRefcon

#define kRemoteNMI                  "remote_nmi"
#define REMOTE_NMI_PATTERN_LEN      32

// _txThreadState
#define kTxThreadStateInit          0x00000001  // initial state
#define kTxThreadStateStop          0x00000100  // temporarily disable
#define kTxThreadStateDetach        0x00000200  // permanently disable
#define kTxThreadStateHalted        0x00000800  // stop confirmation
#define kTxThreadStatePurge         0x00001000  // purge new packets

// _txThreadFlags
#define kTxThreadWakeupEnable       0x00000001  // enable ifnet_start call
#define kTxThreadWakeupSignal       0x00000002  // driver signaled
#define kTxThreadWakeupMask         0x00000003  // wakeup bits

enum {
    kConfigFrozen       = 0x01,
    kConfigTxPull       = 0x02,
    kConfigRxPoll       = 0x04,
    kConfigDataRates    = 0x08,
    kConfigPreEnqueue   = 0x10
};

//------------------------------------------------------------------------------
// Initialize an IONetworkInterface instance.
//
// Returns true if initialized successfully, false otherwise.

bool IONetworkInterface::init( IONetworkController * controller )
{
    IONetworkData * nd;
#if TARGET_OS_EMBEDDED
	OSString *      networkType;
#endif

    // Propagate the init() call to our superclass.

    if ( super::init() == false )
        goto fail;

	// A non-null value at this point means the subclass is pre-kpi
    // and allocated its ifnet/arpcom itself.
	// We can't work with such a sublcass but at least we can fail gracefully.

    if (getIfnet())
	{
		IOLog("\33[00m\33[31m%s: IONetworkingFamily interface KPIs do not support IONetworkInterface subclasses that use ifnets\n\33[00m", getName());
		goto fail;
	}
	
    // The controller object provided must be valid.

    if ( OSDynamicCast(IONetworkController, controller) == 0 )
        goto fail;

    _driver = controller;
    _driver->retain();

    // Allocate memory for the ExpansionData structure.

    _reserved = IONew( ExpansionData, 1 );
    if ( _reserved == 0 )
        goto fail;

    // Initialize the fields in the ExpansionData structure.

    bzero(_reserved, sizeof(ExpansionData));

	_privateLock = IOLockAlloc();
	if ( _privateLock == 0)
		goto fail;

    _publicLock = IORecursiveLockAlloc();
    if ( _publicLock == 0 )
        goto fail;

    _controller = controller;
    _rxPollModel = IFNET_MODEL_INPUT_POLL_OFF;

    _inputPushQueue = IONew(IOMbufQueue, 1);
    bzero(_inputPushQueue, sizeof(*_inputPushQueue));

    // Set initial queue state before attaching to network stack.
    _txThreadState = kTxThreadStateInit | kTxThreadStateHalted;

    // Create an OSNumber to store interface state bits.

    _stateBits = OSNumber::withNumber((UInt64) 0, 32);
    if ( _stateBits == 0 )
        goto fail;
    setProperty( kIOInterfaceState, _stateBits );

    // Create an OSSet to store client objects. Initial capacity
    // (which can grow) is set at 2 clients.

    _clientSet = OSSet::withCapacity(2);
    if ( _clientSet == 0 )
        goto fail;

    // Dictionary to store network data.

    if ( (_dataDict = OSDictionary::withCapacity(5)) == 0 )
        goto fail;

    nd = IONetworkData::withExternalBuffer(
                            kIONetworkStatsKey,
                            sizeof(IONetworkStats),
                            &_driverStats);
    if ( nd )
    {
        addNetworkData(nd);
        nd->release();
    }

    // Register default output handler (not used for pull transmit model)

    if (!registerOutputHandler(controller, controller->getOutputHandler()))
        goto fail;

    // Set the kIOInterfaceNamePrefix and kIOPrimaryInterface properties.
    // These may be used by an user space agent as hints when assigning a
    // BSD name for the interface.

    setProperty( kIOInterfaceNamePrefix, getNamePrefix() );

#if TARGET_OS_EMBEDDED
    networkType = OSDynamicCast(OSString, controller->getProperty( "IONetworkRootType" ));
    if (networkType)
		setProperty( "IONetworkRootType", networkType );
#endif /* TARGET_OS_EMBEDDED */

    if (IOService *provider = controller->getProvider())
    {
        bool        gotLocation = false;
        OSData *    locationAsData;
        OSData *    locationAsCstr;
        OSString *  locationAsString;

        setProperty(kIOBuiltin, (bool)(provider->getProperty("built-in")));

        if ((locationAsData = OSDynamicCast(OSData, provider->getProperty("location"))))
        {
			// the data may be null terminated...but to be on the safe side,
            // let's assume it's not, and add a null...
            // create a copy that we can convert to C string
			if ((locationAsCstr = OSData::withData(locationAsData)))
            {
                locationAsCstr->appendByte(0, 1);
                // now create the OSString
                if ((locationAsString = OSString::withCString(
                     (const char *) locationAsCstr->getBytesNoCopy())))
                {
                    gotLocation = true;
                    setProperty(kIOLocation, locationAsString);
                    locationAsString->release(); //setProperty took a ref
                }
                locationAsCstr->release();
            }
        }
        if (!gotLocation)
            setProperty(kIOLocation, "");
    }

    DLOG("IONetworkInterface::init(%p, %s)\n", this, controller->getName());
    return true;

fail:
    LOG("IONetworkInterface::init(%p, %s) failed\n", this, controller->getName());
    return false;
}

//------------------------------------------------------------------------------
// Destroy the interface. Release all allocated resources.

void IONetworkInterface::free( void )
{
    DLOG("IONetworkInterface::free(%p)\n", this);

    if (_driver)
    {
        _driver->release();
        _driver = 0;
    }

    if ( _clientSet )
    {
        // Should not have any clients.
        assert(_clientSet->getCount() == 0);
        _clientSet->release();
        _clientSet = 0;
    }

    if ( _dataDict  )
    {
        _dataDict->release();
        _dataDict = 0;
    }

    if ( _stateBits )
    {
        _stateBits->release();
        _stateBits = 0;
    }

    if ( _inputPushQueue )
    {
		clearInputQueue();
        IODelete(_inputPushQueue, IOMbufQueue, 1);
        _inputPushQueue = 0;
    }

    if ( _privateLock )
    {
        IOLockFree(_privateLock);
        _privateLock = 0;
    }

    if (_backingIfnet)
    {
        ifnet_release(_backingIfnet);
        _backingIfnet = 0;
    }

    // Free resources referenced through fields in the ExpansionData
    // structure, and also the structure itself.

    if ( _reserved )
    {
        if ( _publicLock )
        {
            IORecursiveLockFree(_publicLock);
            _publicLock = 0;
        }

        if (_remote_NMI_pattern) {
            IOFree(_remote_NMI_pattern, _remote_NMI_len);
        }

        if (_txWorkLoop)
            _txWorkLoop->release();

        memset(_reserved, 0, sizeof(ExpansionData));
        IODelete(_reserved, ExpansionData, 1);
        _reserved = 0;
    }

    super::free();
}

//------------------------------------------------------------------------------
// Returns true if the receiver of this method is the system's primary
// network interface.

bool IONetworkInterface::isPrimaryInterface() const
{
    IOService * provider  = _driver;
    bool        isPrimary = false;

    if ( provider ) provider = provider->getProvider();

    // Look for the built-in property in the ethernet entry.

    if ( provider && provider->getProperty("built-in") && getUnitNumber() == 0)
    {
        isPrimary = true;
    }

    return isPrimary;
}

//------------------------------------------------------------------------------

IONetworkController * IONetworkInterface::getController( void ) const
{
    return _controller;
}

//------------------------------------------------------------------------------
// Get the value that should be set in the hwassist field in the ifnet
// structure. Currently, this field is solely used for advertising the
// hardware checksumming support.

static UInt32 getIfnetHardwareAssistValue(
    IONetworkController * driver )
{
    UInt32  input;
    UInt32  output;
    UInt32  hwassist = 0;
    UInt32  driverFeatures = driver->getFeatures();

    do {
        if ( driver->getChecksumSupport(
                     &input,
                     IONetworkController::kChecksumFamilyTCPIP,
                     false ) != kIOReturnSuccess ) break;
        
        if ( driver->getChecksumSupport(
                     &output,
                     IONetworkController::kChecksumFamilyTCPIP,
                     true ) != kIOReturnSuccess ) break;

        if ( input & output & IONetworkController::kChecksumIP )
        {
            hwassist |= IFNET_CSUM_IP;
        }
        
        if ( ( input  & ( IONetworkController::kChecksumTCP |
                          IONetworkController::kChecksumTCPNoPseudoHeader ) )
        &&   ( output & ( IONetworkController::kChecksumTCP ) ) )
        {
            hwassist |= IFNET_CSUM_TCP;
        }
        
        if ( ( input  & ( IONetworkController::kChecksumUDP | 
                          IONetworkController::kChecksumUDPNoPseudoHeader ) )
        &&   ( output & ( IONetworkController::kChecksumUDP ) ) )
        {
            hwassist |= IFNET_CSUM_UDP;
        }

        if ( input & output & IONetworkController::kChecksumTCPSum16 )
        {
            hwassist |= ( IFNET_CSUM_SUM16 | IFNET_CSUM_TCP | IFNET_CSUM_UDP );
        }

        if ( input & output & IONetworkController::kChecksumTCPIPv6 )
        {
            hwassist |= IFNET_CSUM_TCPIPV6;
        }
        if ( input & output & IONetworkController::kChecksumUDPIPv6 )
        {
            hwassist |= IFNET_CSUM_UDPIPV6;
        }        
    }
    while ( false );

	if( driverFeatures & kIONetworkFeatureHardwareVlan)
		hwassist |= IFNET_VLAN_TAGGING;
	
	if( driverFeatures & kIONetworkFeatureSoftwareVlan)
		hwassist |= IFNET_VLAN_MTU;
	
	if( driverFeatures & kIONetworkFeatureMultiPages)
		hwassist |= IFNET_MULTIPAGES;

	if( driverFeatures & kIONetworkFeatureTSOIPv4)
		hwassist |= IFNET_TSO_IPV4;

	if( driverFeatures & kIONetworkFeatureTSOIPv6)
		hwassist |= IFNET_TSO_IPV6;

    return hwassist;
}

//------------------------------------------------------------------------------
// Initialize the ifnet structure.
    
bool IONetworkInterface::initIfnet( struct ifnet * ifp )
{
	return false;   // deprecated - replaced by initIfnetParams() 
}

OSMetaClassDefineReservedUsed(IONetworkInterface, 4);

bool IONetworkInterface::initIfnetParams( struct ifnet_init_params *params )
{
    // Register our 'shim' functions. These function pointers
    // points to static member functions inside this class.
	params->name		= (char *) getNamePrefix();
	params->type		= _type;
	params->unit		= _unit;
	params->output		= if_output;
	params->ioctl		= if_ioctl;
	params->set_bpf_tap = if_set_bpf_tap;
	params->detach		= if_detach;
	params->softc		= this;

    return true;
}

//------------------------------------------------------------------------------
// Take/release the interface lock.

void IONetworkInterface::lock()
{
    IORecursiveLockLock(_publicLock);
}

void IONetworkInterface::unlock()
{
    IORecursiveLockUnlock(_publicLock);
}

//------------------------------------------------------------------------------
// Inspect the controller after it has been opened.

bool IONetworkInterface::controllerDidOpen( IONetworkController * controller )
{
    return true;   // by default, always accept the controller open.
}

//------------------------------------------------------------------------------
// Perform cleanup before the controller is closed.

void IONetworkInterface::controllerWillClose( IONetworkController * controller )
{
}

//------------------------------------------------------------------------------
// Handle a client open on the interface (IONetworkStack or user client)

bool IONetworkInterface::handleOpen( IOService *  client,
                                     IOOptionBits options,
                                     void *       argument )
{
    bool  accept         = false;
    bool  controllerOpen = false;

    do {
        // Was this object already registered as our client?

        if ( _clientSet->containsObject(client) )
        {
            DLOG("%s: rejected open from existing client %s\n",
                getName(), client->getName());
            accept = true;
            break;
        }

        // If the interface has not received a client open, which also
        // implies that the interface has not yet opened the controller,
        // then open the controller upon receiving the first open from
        // a client. If the controller open fails, the client open will
        // be rejected.

        if (( getInterfaceState() & kIONetworkInterfaceOpenedState ) == 0)
        {
            if (!_driver ||
                 ((controllerOpen = _driver->open(this)) == false) ||
                 (controllerDidOpen(_driver) == false))
                break;
        }

        // Allow subclasses to intercept.

        if ( handleClientOpen(client, options, argument) == false )
            break;

        // Add the new client object to our client set.

        if ( _clientSet->setObject(client) == false )
        {
            handleClientClose(client, 0);
            break;
        }

        accept = true;
    }
    while (false);

    // If provider was opened above, but an error has caused us to refuse
    // the client open, then close our provider.

    if ( controllerOpen )
    {
        if (accept)
        {
            setInterfaceState( kIONetworkInterfaceOpenedState );
            _driver->registerInterestedDriver( this );
        }
        else
        {
            controllerWillClose(_driver);
            _driver->close(this);
        }
    }

    return accept;
}

//------------------------------------------------------------------------------
// Handle a client close on the interface.

void IONetworkInterface::handleClose( IOService * client, IOOptionBits options )
{
    if ( _clientSet->containsObject(client) )
    {
        // Call handleClientClose() for subclass to handle the client close.

        handleClientClose( client, options );

        // Close our provider on last client close.

        if ( _clientSet->getCount() == 1 )
        {
            _driver->deRegisterInterestedDriver( this );
            controllerWillClose( _driver );
            _driver->close( this );
            setInterfaceState( 0, kIONetworkInterfaceOpenedState );

            // Closed by IONetworkStack after detaching interface,
            // drop the driver retain from init().

            if (!isRegistered())
            {
                _driver->release();
                _driver = 0;
            }
        }
        _clientSet->removeObject(client);
    }
}

//------------------------------------------------------------------------------
// Query whether a client has an open on the interface.

bool IONetworkInterface::handleIsOpen( const IOService * client ) const
{
    if (client)
        return _clientSet->containsObject(client);
    else
        return (_clientSet->getCount() > 0);
}

//------------------------------------------------------------------------------
// Handle a client open on the interface.

bool IONetworkInterface::handleClientOpen(IOService *  client,
                                          IOOptionBits options,
                                          void *       argument)
{
    return true;
}

//------------------------------------------------------------------------------
// Handle a client close on the interface.

void IONetworkInterface::handleClientClose(IOService *  client,
                                           IOOptionBits options)
{
}

//------------------------------------------------------------------------------
// Register the output packet handler.

bool IONetworkInterface::registerOutputHandler(OSObject *      target,
                                               IOOutputAction  action)
{
    IOLockLock(_privateLock);

    // Sanity check the arguments.

    if ( (getInterfaceState() & kIONetworkInterfaceOpenedState) ||
         !target || !action )
    {
        IOLockUnlock(_privateLock);
        return false;
    }

    _outTarget = target;
    _outAction = action;

    IOLockUnlock(_privateLock);

    return true;
}

//------------------------------------------------------------------------------
// Feed packets to the input/output BPF packet filter taps.

OSMetaClassDefineReservedUsed(IONetworkInterface, 2);

void IONetworkInterface::feedPacketInputTap(mbuf_t  m)
{
    // PR3662433 we're not protected from this getting changed out from under us
    // so use a local ptr and double check for NULL to avoid the race
	bpf_packet_func inFilter = _inputFilterFunc; 
	if (inFilter)
		inFilter(_backingIfnet, m); 
}

OSMetaClassDefineReservedUsed(IONetworkInterface, 3);

void IONetworkInterface::feedPacketOutputTap(mbuf_t m)
{
    // see comment in feedPacketInputTap
	bpf_packet_func outFilter = _outputFilterFunc;
	if (outFilter)
		outFilter(_backingIfnet, m);
}

//------------------------------------------------------------------------------

#define ABS(a)  ((a) < 0 ? -(a) : (a))

void IONetworkInterface::pushInputQueue( IOMbufQueue * queue )
{
    uint32_t    temp;
    int         delta;

    _inputDeltas.packets_in = queue->count;
    _inputDeltas.bytes_in   = queue->bytes;

    // Report our packet count rather than rely on the driver stats.
    // ifnet_input_extended() requires the count to be accurate.

    temp = _driverStats.inputErrors;
    delta =  temp - _lastDriverStats.inputErrors;
    _inputDeltas.errors_in = ABS(delta);
    _lastDriverStats.inputErrors = temp;

    temp = _driverStats.collisions;
    delta = temp - _lastDriverStats.collisions;
    _inputDeltas.collisions = ABS(delta);
    _lastDriverStats.collisions = temp;

    ifnet_input_extended(_backingIfnet, queue->head, queue->tail, &_inputDeltas);
    IOMbufQueueInit(queue);
}

void IONetworkInterface::pushInputPacket( mbuf_t packet, uint32_t length )
{
    uint32_t    temp;
    int         delta;

    _inputDeltas.packets_in = 1;
    _inputDeltas.bytes_in   = length;

    // Report our packet count rather than rely on the driver stats.
    // ifnet_input_extended() requires the count to be accurate.

    temp = _driverStats.inputErrors;
    delta =  temp - _lastDriverStats.inputErrors;
    _inputDeltas.errors_in = ABS(delta);
    _lastDriverStats.inputErrors = temp;

    temp = _driverStats.collisions;
    delta = temp - _lastDriverStats.collisions;
    _inputDeltas.collisions = ABS(delta);
    _lastDriverStats.collisions = temp;

    ifnet_input_extended(_backingIfnet, packet, packet, &_inputDeltas);
}

UInt32 IONetworkInterface::flushInputQueue( void )
{
    UInt32 count = _inputPushQueue->count;

    if (count)
    {
        //DLOG("push pkt cnt %u\n", count);
        pushInputQueue(_inputPushQueue);
    }

    return count;
}

UInt32 IONetworkInterface::clearInputQueue( void )
{
    UInt32 count = _inputPushQueue->count;

    if (count)
        mbuf_freem_list(_inputPushQueue->head);

    IOMbufQueueInit(_inputPushQueue);
    return count;
}

inline static const char *get_icmp_data(mbuf_t *pkt, int hdrlen, int datalen)
{
    struct ip   *ip;
    struct icmp *icmp;
    int hlen, icmplen;
    
    icmplen = sizeof(*icmp) + sizeof(struct timeval);

    /* make sure hdrlen is sane with respect to mbuf */
    if (mbuf_len(*pkt) < (sizeof(*ip) + hdrlen))
        goto error;    

    /* only work for IPv4 packets */
    ip = (struct ip *) ((char *) mbuf_data(*pkt) + hdrlen);
    if (IP_VHL_V(ip->ip_vhl) != IPVERSION)
        goto error;

    hlen = IP_VHL_HL(ip->ip_vhl) << 2;

    /* make sure header and data is contiguous */
    if (mbuf_pullup(pkt, hlen + icmplen) != 0)
        goto error;

    /* refresh the pointer and hlen in case buffer was shifted */
    ip = (struct ip *) ((char *) mbuf_data(*pkt) + hdrlen); 
        hlen = IP_VHL_HL(ip->ip_vhl) << 2;

    if (ip->ip_p != IPPROTO_ICMP)
        goto error;

    if (ip->ip_len < (icmplen + datalen))
        goto error;

    icmp = (struct icmp *) (((char *) mbuf_data(*pkt) + hdrlen) + hlen);
    if (icmp->icmp_type != ICMP_ECHO)
        goto error;

    return (const char *) (((char *) mbuf_data(*pkt) + hdrlen) + icmplen);

error:
    return NULL;
}

UInt32 IONetworkInterface::inputPacket( mbuf_t          packet,
                                        UInt32          length,
                                        IOOptionBits    options,
                                        void *          param )
{
	const UInt32    hdrlen = _hdrlen;
    UInt32          count;
    void *          mdata;

    assert(packet);
    assert(_backingIfnet);
    if (!packet || !_backingIfnet)
        return 0;

    assert((mbuf_flags(packet) & MBUF_PKTHDR));
    assert((mbuf_nextpkt(packet) == 0));

	mbuf_pkthdr_setrcvif(packet, _backingIfnet);

    if (length)
    {
        // Driver wants interface to set the mbuf length.
        if (mbuf_next(packet) == 0)
        {
            mbuf_pkthdr_setlen(packet, length);
			mbuf_setlen(packet, length);
        }
        else
        {
            // rare case of single packet but multiple mbufs.
            mbuf_t      m = packet;
            uint32_t    remain = length;
            mbuf_pkthdr_setlen(packet, remain);
            do {
                if (remain < (UInt32) mbuf_len(m))
                    mbuf_setlen(m, remain);
                remain -= mbuf_len(m);
            } while ((m = mbuf_next(m)));
            assert(remain == 0);
        }
    }
    else
    {
        length = mbuf_pkthdr_len(packet);
    }

    // check for special debugger packet 
    if (_remote_NMI_len) {
       const char *data = get_icmp_data(&packet, hdrlen, _remote_NMI_len);

       if (data && (memcmp(data, _remote_NMI_pattern, _remote_NMI_len) == 0)) {
           IOKernelDebugger::signalDebugger();
       } 
    }
	
    // input BPF tap
	if (_inputFilterFunc)
		feedPacketInputTap(packet);

    // frame header at start of mbuf data
    mdata = mbuf_data(packet);
	mbuf_pkthdr_setheader(packet, mdata);

    // packet length does not include the frame header
    mbuf_pkthdr_setlen(packet, length - hdrlen);

    // adjust the mbuf data and length to skip over the frame header
    mbuf_setdata(packet, (char *)mdata + hdrlen, mbuf_len(packet) - hdrlen);

    if ( options & kInputOptionQueuePacket )
    {
        IOMbufQueueTailAdd(_inputPushQueue, packet, length);
        count = 0;
    }
    else
    {
        if (!IOMbufQueueIsEmpty(_inputPushQueue))
        {
            IOMbufQueueTailAdd(_inputPushQueue, packet, length);
            count = _inputPushQueue->count;
            pushInputQueue(_inputPushQueue);
        }
        else
        {
            pushInputPacket(packet, length);
            count = 1;
        }
    }

    return count;
}

//------------------------------------------------------------------------------
// Deliver an event to the network layer.

bool IONetworkInterface::inputEvent(UInt32 type, void * data)
{
    bool      success = true;

    struct {
        kern_event_msg  header;
        uint32_t        unit;
        char            if_name[IFNAMSIZ];
    } event;

    switch (type)
    {
        // Deliver an IOKit defined event.

        case kIONetworkEventTypeLinkUp:        
        case kIONetworkEventTypeLinkDown:
        case kIONetworkEventTypeLinkSpeedChange:
            // Send an event only if DLIL has a reference to this
            // interface.
            if (!_backingIfnet)
                break;

            // Use link speed to report bandwidth for legacy drivers
            if (data && ((_configFlags & kConfigDataRates) == 0))
            {
                if_bandwidths_t bw;
                uint64_t *      bps = (uint64_t *) data;

                bw.max_bw = *bps;
                bw.eff_bw = *bps;
                ifnet_set_bandwidths(_backingIfnet, &bw, &bw);
            }

            if ((type == kIONetworkEventTypeLinkUp) ||
                (type == kIONetworkEventTypeLinkDown))
            {
				bzero((void *) &event, sizeof(event));
                event.header.total_size    = sizeof(event);
                event.header.vendor_code   = KEV_VENDOR_APPLE;
                event.header.kev_class     = KEV_NETWORK_CLASS;
                event.header.kev_subclass  = KEV_DL_SUBCLASS;
                event.header.event_code    = (type == kIONetworkEventTypeLinkUp) ?
                                             KEV_DL_LINK_ON : KEV_DL_LINK_OFF;
				event.header.event_data[0] = ifnet_family(_backingIfnet);
                event.unit                 = ifnet_unit(_backingIfnet);
                strncpy(&event.if_name[0], ifnet_name(_backingIfnet), IFNAMSIZ);

                ifnet_event(_backingIfnet, &event.header);
            }            
            break;

        // Deliver a raw kernel event to DLIL.
        // The data argument must point to a kern_event_msg structure.

        case kIONetworkEventTypeDLIL:
            ifnet_event(_backingIfnet, (struct kern_event_msg *) data);
            break;

        case kIONetworkEventWakeOnLANSupportChanged:
            break;

        default:
            IOLog("IONetworkInterface: unknown event type %x\n", (uint32_t) type);
            success = false;
            break;
    }

    return success;
}

//------------------------------------------------------------------------------
// SIOCSIFMTU (set interface MTU) ioctl handler.

SInt32 IONetworkInterface::syncSIOCSIFMTU(IONetworkController * ctr,
                                          struct ifreq *        ifr)
{   
    SInt32  error;
    UInt32  newMTU = ifr->ifr_mtu;

    // If change is not necessary, return success without getting the
    // controller involved.

    if ( getMaxTransferUnit() == newMTU )
        return 0;

    // Request the controller to switch MTU size.

    error = errnoFromReturn( ctr->setMaxPacketSize(newMTU) );

    if ( error == 0 )
    {
        // Controller reports success. Update the interface MTU size
        // property.

        setMaxTransferUnit(newMTU);
    }

    return error;
}

//------------------------------------------------------------------------------
// SIOCSIFMEDIA (SET interface media) ioctl handler.

SInt32 IONetworkInterface::syncSIOCSIFMEDIA(IONetworkController * ctr,
                                            struct ifreq *        ifr)
{
    OSDictionary *    mediumDict;
    IONetworkMedium * medium;
    SInt32            error;
   
    mediumDict = ctr->copyMediumDictionary();  // creates a copy
    if ( mediumDict == 0 )
    {
        // unable to allocate memory, or no medium dictionary.
        return EOPNOTSUPP;
    }

    do {
        // Look for an exact match on the media type.

        medium = IONetworkMedium::getMediumWithType(
                                  mediumDict,
                                  ifr->ifr_media );
        if ( medium ) break;

        // Try a partial match. ifconfig tool sets the media type and media
        // options separately. When media options are changed, the options
        // bits are set or cleared based on the current media selection.

        OSSymbol * selMediumName = (OSSymbol *)
                                   ctr->copyProperty( kIOSelectedMedium );
        if ( selMediumName )
        {
            UInt32            modMediumBits;
            IONetworkMedium * selMedium = (IONetworkMedium *)
                              mediumDict->getObject( selMediumName );

            if ( selMedium &&
                (modMediumBits = selMedium->getType() ^ ifr->ifr_media) )
            {
                medium = IONetworkMedium::getMediumWithType(
                                  mediumDict,
                                  ifr->ifr_media,
                                  ~( IFM_TMASK | IFM_NMASK | modMediumBits));                
            }

            selMediumName->release();
        }
        if ( medium ) break;

        // Still no match, look for a medium that matches the network type
        // and sub-type.
        
        medium = IONetworkMedium::getMediumWithType(
                                  mediumDict,
                                  ifr->ifr_media,
                                  ~( IFM_TMASK | IFM_NMASK ));
    } while ( false );

    // It may be possible for the controller to update the medium
    // dictionary and perhaps delete the medium entry that we have
    // selected from our copy of the stale dictionary. This is
    // harmless since IONetworkController will filter out invalid
    // selections before calling the driver.

    if ( medium )
        error = errnoFromReturn(
                ctr->selectMediumWithName(medium->getName()) );
    else
        error = EINVAL;

    mediumDict->release();

    return error;
}

//------------------------------------------------------------------------------
// SIOCGIFMEDIA (GET interface media) ioctl handler.

SInt32 IONetworkInterface::syncSIOCGIFMEDIA(IONetworkController * ctr,
                                            struct ifreq *        ifr,
					    unsigned long cmd)
{
    OSDictionary *          mediumDict  = 0;
    UInt                    mediumCount = 0;
    UInt                    maxCount;
    OSCollectionIterator *  iter = 0;
    UInt32 *                typeList;
    UInt                    typeListSize;
    OSSymbol *              keyObject;
    SInt32                  error = 0;
    struct ifmediareq *     ifmr = (struct ifmediareq *) ifr;
    
	// Maximum number of medium types that the caller will accept.
    //
    maxCount = ifmr->ifm_count;

    do {
        mediumDict = ctr->copyMediumDictionary();  // creates a copy
        if (mediumDict == 0)
        {
            error = EOPNOTSUPP;
            break;  // unable to allocate memory, or no medium dictionary.
        }

        if ((mediumCount = mediumDict->getCount()) == 0)
        {
            error = EOPNOTSUPP;
            break;  // no medium in the medium dictionary
        }

        if (maxCount == 0)
            break;  //  caller is only probing for support and media count.

        if (maxCount < mediumCount)
        {
            // user buffer is too small to hold all medium entries.
            error = E2BIG;

            // Proceed with partial copy on E2BIG. This follows the
            // SIOCGIFMEDIA handling practice in bsd/net/if_media.c.
            //
            // break;
        }

        // Create an iterator to loop through the medium entries in the
        // dictionary.
        //
        iter = OSCollectionIterator::withCollection(mediumDict);
        if (!iter)
        {
            error = ENOMEM;
            break;
        }

        // Allocate memory for the copyout buffer.
        //
        typeListSize = maxCount * sizeof(UInt32);
        typeList = (UInt32 *) IOMalloc(typeListSize);
        if (!typeList)
        {
            error = ENOMEM;
            break;
        }
        bzero(typeList, typeListSize);

        // Iterate through the medium dictionary and copy the type of
        // each medium entry to typeList[].
        //
        mediumCount = 0;
        while ( (keyObject = (OSSymbol *) iter->getNextObject()) &&
                (mediumCount < maxCount) )
        {
            IONetworkMedium * medium = (IONetworkMedium *) 
                                       mediumDict->getObject(keyObject);
            if (!medium)
                continue;   // should not happen!

            typeList[mediumCount++] = medium->getType();
        }

        if (mediumCount)
        {
	    user_addr_t srcaddr;
	    // here's where the difference in ioctls needs to be accounted for.
	    srcaddr = (cmd == SIOCGIFMEDIA64) ?
	        ((struct ifmediareq64 *)ifmr)->ifmu_ulist :
		CAST_USER_ADDR_T(((struct ifmediareq32 *)ifmr)->ifmu_ulist);
	    if (srcaddr != USER_ADDR_NULL) {
                error = copyout((caddr_t) typeList, srcaddr, typeListSize);
	    }
        }

        IOFree(typeList, typeListSize);
    }
    while (0);

    ifmr->ifm_active = ifmr->ifm_current = IFM_NONE;
    ifmr->ifm_status = 0;
    ifmr->ifm_count  = mediumCount;

    // Get a copy of the controller's property table and read the
    // link status, current, and active medium.

    OSDictionary * pTable = ctr->dictionaryWithProperties();
    if (pTable)
    {
        OSNumber * linkStatus = (OSNumber *) 
                                pTable->getObject(kIOLinkStatus);
        if (linkStatus)
            ifmr->ifm_status = linkStatus->unsigned32BitValue();
        
        if (mediumDict)
        {
            IONetworkMedium * medium;
            OSSymbol *        mediumName;

            if ((mediumName = (OSSymbol *) pTable->getObject(kIOSelectedMedium)) 
               && (medium = (IONetworkMedium *) 
                            mediumDict->getObject(mediumName)))
            {
                ifmr->ifm_current = medium->getType();
            }

            if ((mediumName = (OSSymbol *) pTable->getObject(kIOActiveMedium)) 
               && (medium = (IONetworkMedium *) 
                            mediumDict->getObject(mediumName)))
            {
                ifmr->ifm_active = medium->getType();
            }
        }
        pTable->release();
    }

    if (iter)
        iter->release();

    if (mediumDict)
        mediumDict->release();

    return error;
}

//------------------------------------------------------------------------------
// Handle ioctl commands sent to the network interface.

SInt32 IONetworkInterface::performCommand(IONetworkController * ctr,
                                          unsigned long         cmd,
                                          void *                arg0,
                                          void *                arg1)
{
    struct ifreq *  ifr = (struct ifreq *) arg1;
    SInt32          ret = EOPNOTSUPP;

    if ( (ifr == 0) || (ctr == 0) )
        return EINVAL;

    switch ( cmd )
    {
        // Get interface MTU.

        case SIOCGIFMTU:
            ifr->ifr_mtu = getMaxTransferUnit();
            ret = 0;    // no error
            break;

        // Get interface media type and status.

        case SIOCGIFMEDIA32:
        case SIOCGIFMEDIA64:
            ret = syncSIOCGIFMEDIA(ctr, ifr, cmd);
            break;

        case SIOCSIFMTU:
        case SIOCSIFMEDIA:
        case SIOCSIFLLADDR:
            ret = (int) ctr->executeCommand(
                             this,            /* client */
                             (IONetworkController::Action)
                                &IONetworkInterface::performGatedCommand,
                             this,            /* target */
                             ctr,             /* param0 */
                             (void *) cmd,    /* param1 */
                             arg0,            /* param2 */
                             arg1 );          /* param3 */
            break;

        default:
            // DLOG(%s: command not handled (%08lx), getName(), cmd);
            break;
    }

    return ret;
}

//------------------------------------------------------------------------------
// Perform an ioctl command on the controller's workloop context.

int IONetworkInterface::performGatedCommand(void * target,
                                            void * arg1_ctr,
                                            void * arg2_cmd,
                                            void * arg3_0,
                                            void * arg4_1)
{
    IONetworkInterface *  self = (IONetworkInterface *)  target;
    IONetworkController * ctr  = (IONetworkController *) arg1_ctr;
    struct ifreq *        ifr  = (struct ifreq *) arg4_1;
    SInt32                ret  = EOPNOTSUPP;

    // Refuse to issue I/O to the controller if it is in a power state
    // that renders it "unusable".

    if ( self->getInterfaceState() & kIONetworkInterfaceDisabledState )
        return EPWROFF;

    switch ( (uintptr_t) arg2_cmd )
    {
        // Set interface MTU.

        case SIOCSIFMTU:
            ret = self->syncSIOCSIFMTU(ctr, ifr);
            break;

        // Set interface (controller) media type.

        case SIOCSIFMEDIA:
            ret = self->syncSIOCSIFMEDIA(ctr, ifr);
            break;

        // Set link layer address.

        case SIOCSIFLLADDR:
            ret = ctr->errnoFromReturn(
                  ctr->setHardwareAddress( ifr->ifr_addr.sa_data,
                                           ifr->ifr_addr.sa_len ) );
            break;
    }

    return ret;
}

//------------------------------------------------------------------------------
// if_ioctl() handler - Calls performCommand() when we receive an ioctl
// from DLIL.

errno_t
IONetworkInterface::if_ioctl( ifnet_t ifp, unsigned long cmd, void * data )
{
	IONetworkInterface *    self = IFNET_TO_THIS(ifp);
    IONetworkController *   driver;
    errno_t                 err;
    
    if (!self || self->isInactive())
    {
        return EOPNOTSUPP;
    }

    driver = self->_driver;
    if (!driver)
    {
        return EINVAL;
    }

    assert(ifp == self->_backingIfnet);

    err = self->performCommand( driver, cmd, (void *) ifp, data );
    return err;
}

//------------------------------------------------------------------------------
// if_output() handler.
//
// Handle a call from the network stack to transmit the given mbuf.
// For now, we can assume that the mbuf is singular, and never chained.

int IONetworkInterface::if_output( ifnet_t ifp, mbuf_t m )
{
	UInt32      noraceTemp;
	int         delta;
	u_int32_t   outPackets, outErrors;

	IONetworkInterface * self = IFNET_TO_THIS(ifp);
    
    assert(ifp == self->_backingIfnet);

    if ( m == 0 )
    {
        DLOG("%s: NULL output mbuf\n", self->getName());
        return EINVAL;
    }

    if ( (mbuf_flags(m) & MBUF_PKTHDR) == 0 )
    {
        DLOG("%s: MBUF_PKTHDR bit not set\n", self->getName());
        mbuf_freem(m);
        return EINVAL;
    }

    // Increment output related statistics.	
	// update the stats that the driver maintains	
	noraceTemp = self->_driverStats.outputErrors;
	delta = noraceTemp - self->_lastDriverStats.outputErrors;
	outErrors = ABS(delta);
	self->_lastDriverStats.outputErrors = noraceTemp;
	
	noraceTemp = self->_driverStats.outputPackets;
	delta = noraceTemp - self->_lastDriverStats.outputPackets;
	outPackets = ABS(delta);
	self->_lastDriverStats.outputPackets = noraceTemp;
	
	// update the stats in the interface
	ifnet_stat_increment_out(self->getIfnet(), outPackets, mbuf_pkthdr_len(m), outErrors);
	
    // Feed the output filter tap.
	if(self->_outputFilterFunc)
		self->feedPacketOutputTap(m);

	// Forward the packet to the registered output packet handler.
	return ((self->_outTarget)->*(self->_outAction))(m, 0);
}

//------------------------------------------------------------------------------
// if_set_bpf_tap() handler. Handles request from the DLIL to enable or
// disable the input/output filter taps.
//
// FIXME - locking may be needed.

errno_t IONetworkInterface::if_set_bpf_tap(
    ifnet_t             ifp,
    bpf_tap_mode        mode,
    bpf_packet_func     func)
{
	IONetworkInterface * self = IFNET_TO_THIS(ifp);

    assert(ifp == self->_backingIfnet);

    switch ( mode )
    {
        case BPF_TAP_DISABLE:
            self->_inputFilterFunc = self->_outputFilterFunc = 0;
            break;

        case BPF_TAP_INPUT:
            assert(func);
            self->_inputFilterFunc = func;
            break;

        case BPF_TAP_OUTPUT:
            assert(func);
            self->_outputFilterFunc = func;
            break;
        
        case BPF_TAP_INPUT_OUTPUT:
            assert(func);
            self->_inputFilterFunc = self->_outputFilterFunc = func;
            break;

        default:
            DLOG("%s: Unknown BPF tap mode %d\n", self->getName(), mode);
            break;
    }

    return 0;
}

void IONetworkInterface::if_detach( ifnet_t ifp )
{
	IONetworkInterface * self = IFNET_TO_THIS(ifp);

    assert(WAITING_FOR_DETACH(self) == 1);
    self->retain();
	IOLockLock(self->_privateLock);
	WAITING_FOR_DETACH(self) = 0;
	thread_wakeup((void *) self);
	IOLockUnlock(self->_privateLock);
    self->release();
}

//------------------------------------------------------------------------------
// ifnet field (and property table) getter/setter.

#define IO_IFNET_GET(func, type, field, kpi)                    \
type IONetworkInterface::func(void) const                       \
{                                                               \
    type val = (_backingIfnet != NULL) ?                        \
               kpi(_backingIfnet) : _##field;                   \
	return val;                                                 \
}

#define IO_IFNET_SET(func, type, field, kpi)                    \
bool IONetworkInterface::func(type value)                       \
{                                                               \
    if (_backingIfnet)                                          \
        kpi(_backingIfnet, value);                              \
    else                                                        \
        _##field = value;                                       \
	return true;                                                \
}

#define IO_IFNET_RMW(func, type, field, kpi)                    \
bool IONetworkInterface::func(type set, type clear)             \
{                                                               \
    if (_backingIfnet)                                          \
        kpi(_backingIfnet, set, set | clear);                   \
    else                                                        \
        _##field = (_##field & ~clear) | set;                   \
	return true;                                                \
}

//------------------------------------------------------------------------------
// Interface type accessors (ifp->if_type). The list of interface types is
// defined in <bsd/net/if_types.h>.

bool IONetworkInterface::setInterfaceType(UInt8 type)
{
	// once attached to dlil, we can't change the interface type
	if (_backingIfnet)
		return false;
	else
		_type = type;
	return true;
}

UInt8 IONetworkInterface::getInterfaceType(void) const
{
    return _type;
}

IO_IFNET_SET(setMaxTransferUnit, UInt32, mtu, ifnet_set_mtu)
IO_IFNET_GET(getMaxTransferUnit, UInt32, mtu, ifnet_mtu)

IO_IFNET_RMW(setFlags, UInt16, flags, ifnet_set_flags)
IO_IFNET_GET(getFlags, UInt16, flags, ifnet_flags)

bool IONetworkInterface::setExtraFlags( UInt32 set, UInt32 clear )
{
    _eflags = (_eflags & ~clear) | set;
    return true;
}

IO_IFNET_GET(getExtraFlags, UInt32, eflags, ifnet_eflags)

IO_IFNET_SET(setMediaAddressLength, UInt8, addrlen, ifnet_set_addrlen)
IO_IFNET_GET(getMediaAddressLength, UInt8, addrlen, ifnet_addrlen)

IO_IFNET_SET(setMediaHeaderLength, UInt8, hdrlen, ifnet_set_hdrlen)
IO_IFNET_GET(getMediaHeaderLength, UInt8, hdrlen, ifnet_hdrlen)

IO_IFNET_GET(getUnitNumber, UInt16, unit, ifnet_unit)

bool IONetworkInterface::setUnitNumber( UInt16 value )
{
    // once we've attached to dlil, unit can't be changed
	if (_backingIfnet)
		return false;
	
    if (setProperty( kIOInterfaceUnit, value, 32 ))
    {
        char name[128];

        _unit = value;
		setProperty( kIOPrimaryInterface, isPrimaryInterface() );
        snprintf(name, sizeof(name), "%s%u", getNamePrefix(), _unit);
        setName(name);
        return true;
    }
    else
        return false;
}

//------------------------------------------------------------------------------
// Return true if the interface has been registered with the network layer,
// false otherwise.

bool IONetworkInterface::isRegistered() const
{
    return (bool)(getInterfaceState() & kIONetworkInterfaceRegisteredState);
}

//------------------------------------------------------------------------------
// serialize

bool IONetworkInterface::serializeProperties( OSSerialize * s ) const
{
    IONetworkInterface * self = (IONetworkInterface *) this;

    self->setProperty( kIOInterfaceType,       getInterfaceType(),       8 );
    self->setProperty( kIOMaxTransferUnit,     getMaxTransferUnit(),    32 );
    self->setProperty( kIOInterfaceFlags,      getFlags(),              16 );
    self->setProperty( kIOInterfaceExtraFlags, getExtraFlags(),         32 );
    self->setProperty( kIOMediaAddressLength,  getMediaAddressLength(),  8 );
    self->setProperty( kIOMediaHeaderLength,   getMediaHeaderLength(),   8 );

    return super::serializeProperties( s );
}

//------------------------------------------------------------------------------
// Return the interface state flags.

UInt32 IONetworkInterface::getInterfaceState() const
{
    return _stateBits->unsigned32BitValue();
}

//------------------------------------------------------------------------------
// Set (or clear) the interface state flags.

UInt32 IONetworkInterface::setInterfaceState( UInt32 set,
                                              UInt32 clear )
{
    UInt32  val;

    assert( _stateBits );

    IOLockLock(_privateLock);

    val = ( _stateBits->unsigned32BitValue() | set ) & ~clear;
    _stateBits->setValue( val );

    IOLockUnlock(_privateLock);

    return val;
}

//------------------------------------------------------------------------------
// Perform a lookup of the dictionary kept by the interface,
// and return an entry that matches the specified string key.
//
// key: Search for an IONetworkData entry with this key.
//
// Returns the matching entry, or 0 if no match was found.

IONetworkData * IONetworkInterface::getNetworkData(const OSSymbol * key) const
{
    return OSDynamicCast(IONetworkData, _dataDict->getObject(key));
}

IONetworkData * IONetworkInterface::getNetworkData(const char * key) const
{
    return OSDynamicCast(IONetworkData, _dataDict->getObject(key));
}

//------------------------------------------------------------------------------
// A private function to copy the data dictionary to the property table.

bool IONetworkInterface::_syncNetworkDataDict()
{
    OSDictionary * aCopy = OSDictionary::withDictionary(_dataDict);
    bool           ret   = false;

    if (aCopy) {
        ret = setProperty(kIONetworkData, aCopy);
        aCopy->release();
    }

    return ret;
}

//------------------------------------------------------------------------------
// Remove an entry from the IONetworkData dictionary managed by the interface.
// The removed object is released.

bool IONetworkInterface::removeNetworkData(const OSSymbol * aKey)
{
    bool ret;

    IOLockLock(_privateLock);
    _dataDict->removeObject(aKey);
    ret = _syncNetworkDataDict();
    IOLockUnlock(_privateLock);
    return ret;
}

bool IONetworkInterface::removeNetworkData(const char * aKey)
{
    bool ret;

    IOLockLock(_privateLock);
    _dataDict->removeObject(aKey);
    ret = _syncNetworkDataDict();
    IOLockUnlock(_privateLock);
    return ret;
}

//------------------------------------------------------------------------------
// Add an IONetworkData object to a dictionary managed by the interface.

bool IONetworkInterface::addNetworkData(IONetworkData * aData)
{
    bool ret = false;

    if (OSDynamicCast(IONetworkData, aData) == 0)
        return false;

    IOLockLock(_privateLock);
    if (_dataDict->setObject(aData->getKey(), aData))
        ret = _syncNetworkDataDict();
    IOLockUnlock(_privateLock);
    return ret;
}

//------------------------------------------------------------------------------
// Create a new IOUserClient to handle client requests. The default
// implementation will create an IONetworkUserClient instance if
// the type given is kIONetworkUserClientTypeID.

IOReturn IONetworkInterface::newUserClient(task_t           owningTask,
                                           void *         /*security_id*/,
                                           UInt32           type,
                                           IOUserClient **  handler)
{
    IOReturn              err = kIOReturnSuccess;
    IONetworkUserClient * client;

    if (type != kIONetworkUserClientTypeID)
        return kIOReturnBadArgument;

    client = IONetworkUserClient::withTask(owningTask);

    if (!client || !client->attach(this) || !client->start(this))
    {
        if (client)
        {
            client->detach(this);
            client->release();
            client = 0;
        }
        err = kIOReturnNoMemory;
    }

    *handler = client;

    return err;
}

//------------------------------------------------------------------------------
// Power change notices are posted by the controller's policy-maker to
// inform the interface that the controller is changing power states.
// There are two notifications for each state change, delivered prior
// to the state change, and after the state change has occurred.

struct IONetworkPowerChangeNotice {
    IOService *    policyMaker;
    IOPMPowerFlags powerFlags;
    uint32_t       stateNumber;
    uint32_t       phase;
};

enum {
    kPhasePowerStateWillChange = 0x01,
    kPhasePowerStateDidChange  = 0x02
};

//------------------------------------------------------------------------------
// Handle controller's power state transitions.

IOReturn
IONetworkInterface::controllerWillChangePowerState(
                              IONetworkController * controller,
                              IOPMPowerFlags        flags,
                              UInt32                stateNumber,
                              IOService *           policyMaker )
{
    if ( ( flags & IOPMDeviceUsable ) == 0 )
    {
        setInterfaceState( kIONetworkInterfaceDisabledState );
    }
    return kIOReturnSuccess;
}

IOReturn
IONetworkInterface::controllerDidChangePowerState(
                              IONetworkController * controller,
                              IOPMPowerFlags        flags,
                              UInt32                stateNumber,
                              IOService *           policyMaker )
{
    if ( flags & IOPMDeviceUsable )
    {
        setInterfaceState( 0, kIONetworkInterfaceDisabledState );
    }
    return kIOReturnSuccess;
}

//------------------------------------------------------------------------------
// Static member functions called by power-management notification handlers.
// Act as stub functions that will simply forward the call to virtual member
// functions.

void
IONetworkInterface::powerChangeHandler( void * target,
                                        void * param0,
                                        void * param1  )
{
    IONetworkInterface * self = (IONetworkInterface *) target;
	IONetworkPowerChangeNotice * notice;

    assert( self );
    if (!self->_driver)
        return;

    if ( param1 == 0 )
    {
        // Issue a call to this same function synchronized with the
        // work loop thread.

        self->_driver->executeCommand(
              /* client */ self,
              /* action */ (IONetworkController::Action) &powerChangeHandler,
              /* target */ self,
              /* param0 */ param0,
              /* param1 */ (void *) true );

        return;
    }

    notice = (IONetworkPowerChangeNotice *) param0;
    assert( notice );

    DLOG("%s: power change flags:%08x state:%d from:%p phase:%d\n",
         self->getName(), (uint32_t) notice->powerFlags,
         notice->stateNumber, notice->policyMaker, notice->phase );

    if ( notice->phase == kPhasePowerStateWillChange )
    {
        self->controllerWillChangePowerState(
              self->_driver,
              notice->powerFlags,
              notice->stateNumber,
              notice->policyMaker );
    }
    else if ( notice->phase == kPhasePowerStateDidChange )
    {
        self->controllerDidChangePowerState(
              self->_driver,
              notice->powerFlags,
              notice->stateNumber,
              notice->policyMaker );
    }
}

//------------------------------------------------------------------------------
// Handle notitifications triggered by controller's power state change.

IOReturn
IONetworkInterface::powerStateWillChangeTo( IOPMPowerFlags  powerFlags,
                                            unsigned long   stateNumber,
                                            IOService *     policyMaker )
{
    IONetworkPowerChangeNotice  notice;

    notice.policyMaker = policyMaker;
    notice.powerFlags  = powerFlags;
    notice.stateNumber = stateNumber;
    notice.phase       = kPhasePowerStateWillChange;

    powerChangeHandler( (void *) this, (void *) &notice,
                        /* param1: inside gate */ (void *) false );
    return kIOPMAckImplied;
}

IOReturn
IONetworkInterface::powerStateDidChangeTo( IOPMPowerFlags  powerFlags,
                                           unsigned long   stateNumber,
                                           IOService *     policyMaker )
{
    IONetworkPowerChangeNotice  notice;

    notice.policyMaker = policyMaker;
    notice.powerFlags  = powerFlags;
    notice.stateNumber = stateNumber;
    notice.phase       = kPhasePowerStateDidChange;

    powerChangeHandler( (void *) this, (void *) &notice,
                        /* param1: inside gate */ (void *) false );
    return kIOPMAckImplied;
}

//------------------------------------------------------------------------------

IOReturn IONetworkInterface::message( UInt32 type, IOService * provider,
                                      void * argument )
{
    if (kMessageControllerWillShutdown == type)
    {
        // Handle system shutdown or restarts. Handle this by performing the
        // same work when driver is transitioning to an unusable power state.

        DLOG("%s: kMessageControllerWillShutdown\n", getName());
        haltOutputThread( kTxThreadStateDetach );
        powerStateWillChangeTo( 0, 0, NULL );
        return kIOReturnSuccess;
    }

    return super::message( type, provider, argument );
}

//------------------------------------------------------------------------------
// Termination

bool IONetworkInterface::requestTerminate(
    IOService *     provider,
    IOOptionBits    options )
{
    // Early indication that provider started termination
    if (provider == _driver)
    {
        // Stop transmit thread before a later disable by the
        // driver on work loop context, which can deadlock.

        DLOG("%s::requestTerminate(%s)\n",
            getName(), provider->getName());
        haltOutputThread( kTxThreadStateDetach );
    }

    return super::requestTerminate(provider, options);
}

bool IONetworkInterface::willTerminate( IOService *  provider,
                                        IOOptionBits options )
{
    DLOG("%s::%s(%s, 0x%x)\n", getName(), __FUNCTION__,
        provider->getName(), (uint32_t) options);

    setInterfaceState( kIONetworkInterfaceDisabledState );
    return super::willTerminate( provider, options );
}

//------------------------------------------------------------------------------
// Inlined functions pulled from header file to ensure
// binary compatibility with drivers built with gcc2.95.

IONetworkData * IONetworkInterface::getParameter(const char * aKey) const
{ return getNetworkData(aKey); }

bool IONetworkInterface::setExtendedFlags(UInt32 flags, UInt32 clear)
{ return true; }

//------------------------------------------------------------------------------

ifnet_t IONetworkInterface::getIfnet( void ) const
{
	return _backingIfnet;
}

OSMetaClassDefineReservedUsed(IONetworkInterface, 0);

IOReturn IONetworkInterface::attachToDataLinkLayer( IOOptionBits options,
                                                    void *       parameter )
{
    ifnet_init_params       params;
    ifnet_init_eparams      eparams;
	struct sockaddr_dl *    ll_addr = 0;
	char                    buffer[2 * sizeof(struct sockaddr_dl)];
    errno_t                 error;
    OSObject *              prop;
    IOReturn                result = kIOReturnInternalError;

    if (!_driver)
    {
        LOG("%s: BSD attach failed, no driver\n", getName());
        goto fail;
    }

	memset(&params, 0, sizeof(params));
    if (!initIfnetParams(&params))
    {
        LOG("%s: initIfnetParams failed\n", getName());
        goto fail;
    }

    memset(&eparams, 0, sizeof(eparams));

    IOLockLock(_privateLock);
    _configFlags |= kConfigFrozen;

    // Pass ifnet_init_params to subclass which is then converted to
    // the new ifnet_init_eparams. All this to avoid burning another
    // vtable pad slot, and subclasses don't need to change.

    eparams.ver             = IFNET_INIT_CURRENT_VERSION;
    eparams.len             = sizeof(eparams);

    eparams.uniqueid        = params.uniqueid;
    eparams.uniqueid_len    = params.uniqueid_len;
    eparams.name            = params.name;
    eparams.unit            = params.unit;
    eparams.family          = params.family;
    eparams.type            = params.type;

    eparams.demux           = params.demux;
    eparams.add_proto       = params.add_proto;
    eparams.del_proto       = params.del_proto;
    eparams.check_multi     = params.check_multi;
    eparams.framer          = params.framer;
    eparams.softc           = params.softc;
    eparams.ioctl           = params.ioctl;
    eparams.set_bpf_tap     = params.set_bpf_tap;
    eparams.detach          = params.detach;
    eparams.event           = params.event;
    eparams.broadcast_addr  = params.broadcast_addr;
    eparams.broadcast_len   = params.broadcast_len;

    if (!(_configFlags & kConfigRxPoll) &&
        !(_configFlags & kConfigTxPull))
    {
        eparams.flags       = IFNET_INIT_LEGACY;
    }

    if (_configFlags & kConfigRxPoll)
    {
        eparams.flags       = IFNET_INIT_INPUT_POLL;
        eparams.input_ctl   = if_input_ctl;
        eparams.input_poll  = (_rxPollOptions & kIONetworkWorkLoopSynchronous) ?
                              if_input_poll_gated : if_input_poll;
            
        // cache the driver's inputPacketPoll action
        _rxPollAction = (void *) OSMemberFunctionCast(
            IONetworkController::Action,
            _driver, &IONetworkController::pollInputPackets);

        _rxCtlAction =  (void *) OSMemberFunctionCast(
            IONetworkController::Action,
            this, &IONetworkInterface::actionInputCtl);

        DLOG("%s: supports input polling\n", getName());
    }

    if (_configFlags & kConfigTxPull)
    {
        eparams.sndq_maxlen = _txQueueSize;
        if (_txQueueSize)
            DLOG("%s: sndq_maxlen = %u\n", getName(), _txQueueSize);

        eparams.output_sched_model = _txSchedulingModel;

        if (_txPullOptions & kIONetworkWorkLoopSynchronous)
        {
            eparams.start   = if_start_gated;

            // retain work loop for transmitThreadStop()
            _txWorkLoop = _driver->getWorkLoop();
            if (!_txWorkLoop)
            {
                IOLockUnlock(_privateLock);
                goto fail;
            }
            _txWorkLoop->retain();

            _txStartAction = (void *) OSMemberFunctionCast(
                IONetworkController::Action,
                this, &IONetworkInterface::drainOutputQueue);
        }
        else
        {
            eparams.start   = if_start;
        }

        if (_configFlags & kConfigPreEnqueue)
        {
            eparams.pre_enqueue = if_output_pre_enqueue;
        }

        DLOG("%s: supports %stransmit pull, pre_enqueue %d\n",
            getName(), _txWorkLoop ? "gated " : "", (eparams.pre_enqueue != 0));
    }
    else
    {
        eparams.output      = params.output;
    }

    // ifnet_pre_enqueue_func pre_enqueue
    // ifnet_ctl_func output_ctl
    // u_int64_t output_bw
    // u_int64_t input_bw

    IOLockUnlock(_privateLock);

    error = ifnet_allocate_extended(&eparams, &_backingIfnet);
    if (error)
    {
        LOG("%s: ifnet_allocate_extended error %d\n", getName(), error);
        goto fail;
    }

	error = ifnet_set_offload(_backingIfnet,
            getIfnetHardwareAssistValue(_driver));		
    if (error)
        LOG("%s: ifnet_set_offload error %d\n", getName(), error);

    prop = _driver->copyProperty(kIOMACAddress);
    if (prop)
    {
		OSData *    macAddr = OSDynamicCast(OSData, prop);
        uint32_t    len;

        memset(buffer, 0, sizeof(buffer));
        len = sizeof(buffer) - offsetof(struct sockaddr_dl, sdl_data);

        if (macAddr && macAddr->getLength() && (macAddr->getLength() <= len))
        {
            len = macAddr->getLength();
            ll_addr = (struct sockaddr_dl *) buffer;
            bcopy(macAddr->getBytesNoCopy(), ll_addr->sdl_data, len);
            ll_addr->sdl_len = offsetof(struct sockaddr_dl, sdl_data) + len;
            ll_addr->sdl_family = AF_LINK;
            ll_addr->sdl_alen = len;
        }
        prop->release();
        
        if (!ll_addr)
        {
            LOG("%s: BSD attach failed, bad MAC address\n", getName());
            goto fail;
        }
    }

	ifnet_set_mtu(_backingIfnet, _mtu);
	ifnet_set_flags(_backingIfnet, _flags, 0xffff);
	ifnet_set_addrlen(_backingIfnet, _addrlen);
	ifnet_set_hdrlen(_backingIfnet, _hdrlen);
    if (_configFlags & kConfigRxPoll)
        ifnet_set_rcvq_maxlen(_backingIfnet, _rxRingSize);

    error = ifnet_attach(_backingIfnet, ll_addr);
    if (!error)
        result = kIOReturnSuccess;
    else
        LOG("%s: ifnet_attach error %d\n", getName(), error);

fail:    
    if ((result != kIOReturnSuccess) && _backingIfnet)
    {
        // attach failed, clean up
        ifnet_release(_backingIfnet);
        _backingIfnet = NULL;
    }

    return result;
}

//------------------------------------------------------------------------------

OSMetaClassDefineReservedUsed(IONetworkInterface, 1);

void IONetworkInterface::detachFromDataLinkLayer(
    IOOptionBits options, void * parameter )
{
    // Running on thread call context
    // Permanently halt the transmit thread before ifnet detach
    DLOG("%s: detachFromDataLinkLayer\n", getName());
    haltOutputThread( kTxThreadStateDetach );

    WAITING_FOR_DETACH(this) = true;

    // this will lead to another thread calling if_detach()
	ifnet_detach(_backingIfnet);

    // protect against if_detach() running before we block
	IOLockLock(_privateLock);
    while (WAITING_FOR_DETACH(this)) // if false, if_detach is done
    {
        IOLockSleep(_privateLock, this, THREAD_UNINT);
    }
	IOLockUnlock(_privateLock);
}

//------------------------------------------------------------------------------

void IONetworkInterface::debuggerRegistered( void )
{
    char buffer[REMOTE_NMI_PATTERN_LEN + 2];
    unsigned int i;
        
    if (_remote_NMI_len)
        return;

    memset(buffer, 0, sizeof(buffer));
    if (!PE_parse_boot_argn(kRemoteNMI, buffer, sizeof(buffer)))
        return;

    for (i = 0; i < (REMOTE_NMI_PATTERN_LEN >> 1); i++) {
        unsigned int val;
            
        if (sscanf(buffer + (i << 1), "%02X", &val) != 1)
            break;

        buffer[i] = val;
    }

    _remote_NMI_pattern = (char *) IOMalloc(sizeof(char) * i + 1);
    if (!_remote_NMI_pattern) 
        return;

    _remote_NMI_pattern[i] = '\0';
    memcpy(_remote_NMI_pattern, buffer, i);
    _remote_NMI_len = i;
}

//------------------------------------------------------------------------------

void IONetworkInterface::reportDataTransferRates(
    uint64_t    outputRateMax,
    uint64_t    inputRateMax,
    uint64_t    outputRateEffective,
    uint64_t    inputRateEffective )
{
    if_bandwidths_t bw_out, bw_in;

    if ((_configFlags & kConfigDataRates) == 0)
    {
        IOLockLock(_privateLock);
        _configFlags |= kConfigDataRates;
        IOLockUnlock(_privateLock);
    }

    if (!outputRateEffective)
        outputRateEffective = outputRateMax;
    if (!inputRateEffective)
        inputRateEffective = inputRateMax;

    bw_out.max_bw = outputRateMax;
    bw_out.eff_bw = outputRateEffective;
    bw_in.max_bw  = inputRateMax;
    bw_in.eff_bw  = inputRateEffective;

    if (_backingIfnet)
        ifnet_set_bandwidths(_backingIfnet, &bw_out, &bw_in);    
}

//------------------------------------------------------------------------------
// Driver-pull output model
//------------------------------------------------------------------------------

IOReturn IONetworkInterface::configureOutputPullModel(
    uint32_t       driverQueueSize,
    IOOptionBits   options,
    uint32_t       outputQueueSize,
    uint32_t       outputSchedulingModel )
{
    IOReturn ret = kIOReturnError;

    IOLockLock(_privateLock);
    if ((_configFlags & kConfigFrozen) == 0)
    {
        _txRingSize         = driverQueueSize;
        _txPullOptions      = options;
        _txQueueSize        = outputQueueSize;
        _txSchedulingModel  = outputSchedulingModel;
        _configFlags       |= kConfigTxPull;
        ret = kIOReturnSuccess;
    }
    IOLockUnlock(_privateLock);

    return ret;
}

IOReturn IONetworkInterface::installOutputPreEnqueueHandler(
    OutputPreEnqueueHandler handler,
    void *                  target,
    void *                  refCon )
{
    IOReturn ret = kIOReturnError;

    if (!handler)
        return kIOReturnBadArgument;

    IOLockLock(_privateLock);
    if ((_configFlags & kConfigFrozen) == 0)
    {
        _peqHandler   = handler;
        _peqTarget    = target;
        _peqRefcon    = refCon;
        _configFlags |= kConfigPreEnqueue;
        ret = kIOReturnSuccess;
    }
    IOLockUnlock(_privateLock);

    return ret;
}

errno_t IONetworkInterface::if_output_pre_enqueue( ifnet_t ifp, mbuf_t packet )
{
    IONetworkInterface *    me = IFNET_TO_THIS(ifp);
    errno_t                 ret;

    assert(ifp == me->_backingIfnet);
    assert(me->_peqAction);

    ret = me->_peqHandler(me->_peqTarget, me->_peqRefcon, packet);
    return ret;
}

void IONetworkInterface::if_start( ifnet_t ifp )
{
    IONetworkInterface *    me = IFNET_TO_THIS(ifp);
    IONetworkController *   driver;

    assert(ifp == me->_backingIfnet);

    // Thread will be halted before we drop our open/retain on the controller.
    if (me->_txThreadState & kTxThreadStateHalted)
    {
        if (me->_txThreadState & kTxThreadStatePurge)
            ifnet_purge(ifp);
        return;
    }

    driver = me->_driver;
    assert(driver);
    if (__builtin_expect(!driver, 0))
    {
        IOLockLock(me->_privateLock);
        me->_txThreadState |= kTxThreadStateHalted;
        IOLockUnlock(me->_privateLock);
        return;
    }

    me->drainOutputQueue(ifp, driver);
}

void IONetworkInterface::if_start_gated( ifnet_t ifp )
{
    IONetworkInterface *    me = IFNET_TO_THIS(ifp);
    IONetworkController *   driver;

    assert(ifp == me->_backingIfnet);

    // Queue will be halted before we drop our open/retain on the controller.
    if (me->_txThreadState & kTxThreadStateHalted)
    {
        if (me->_txThreadState & kTxThreadStatePurge)
            ifnet_purge(ifp);
        return;
    }

    driver = me->_driver;
    assert(driver);
    if (__builtin_expect(!driver, 0))
    {
        IOLockLock(me->_privateLock);
        me->_txThreadState |= kTxThreadStateHalted;
        IOLockUnlock(me->_privateLock);
        return;
    }

    driver->executeCommand(
            /* client */ me,
            /* action */ (IONetworkController::Action) me->_txStartAction,
            /* target */ me,
            /* param0 */ ifp,
            /* param1 */ driver,
            /* param2 */ 0,
            /* param3 */ 0 );
}

//------------------------------------------------------------------------------

errno_t IONetworkInterface::enqueueOutputPacket( mbuf_t packet, IOOptionBits options )
{
    return ifnet_enqueue(_backingIfnet, packet);
}

//------------------------------------------------------------------------------

void IONetworkInterface::drainOutputQueue(
    ifnet_t                 ifp,
    IONetworkController *   driver )
{
    uint32_t                count;
    IOReturn                status;

    while (true)
    {
        // must respond to queue state transitions before exiting the loop
        if (__builtin_expect((_txThreadState != 0), 0))
        {
            bool halted = false;

            IOLockLock(_privateLock);
            if (_txThreadState & kTxThreadStateInit)
            {
                // The initial if_start call
                assert(_txStartThread == 0);
                _txStartThread = current_thread();
                _txThreadState &= ~kTxThreadStateInit;
            }
            if (_txThreadState & (kTxThreadStateStop | kTxThreadStateDetach))
            {
                // Disable request or interface detached from DLIL
                _txThreadState &= ~kTxThreadStateStop;
                _txThreadState |=  kTxThreadStateHalted;
                thread_wakeup(&_txThreadState);
            }
            if (_txThreadState & kTxThreadStateHalted)
            {
                halted = true;
            }
            IOLockUnlock(_privateLock);

            if (halted)
                break;
        }

        // check for queue empty
        if (ifnet_get_sndq_len(ifp, &count) || !count)
            break;

        _txThreadFlags = 0;
        status = driver->outputStart(this, 0);

        if (kIOReturnSuccess != status)
        {
            if (kIOReturnNoResources == status)
            {
                // Try again on next packet enqueue, or when driver
                // calls outputThreadSignal().
                // Retry transmit if preempted by outputThreadSignal()

                if (OSCompareAndSwap(0, kTxThreadWakeupEnable, &_txThreadFlags))
                    break;
            }
            else
            {
                // Driver error, or dequeue failed
                break;
            }
        }
    }
}

//------------------------------------------------------------------------------

IOReturn IONetworkInterface::dequeueOutputPackets(
    uint32_t                maxCount,
    mbuf_t *                packetHead,
    mbuf_t *                packetTail,
    uint32_t *              packetCount,
    uint64_t *              packetBytes )
{
    uint32_t    txByteCount, temp, txPackets = 0, txErrors = 0;
    int         delta;
    errno_t     error;

    if (!maxCount || !packetHead)
        return kIOReturnBadArgument;

    if (_txThreadState & (kTxThreadStateInit | kTxThreadStateDetach))
    {
        goto no_frames;
    }

    assert(_backingIfnet);

    if (maxCount == 1)
    {
        error = ifnet_dequeue(_backingIfnet, packetHead);
        if (!error)
        {
            if (packetTail)
                *packetTail = *packetHead;
            if (packetCount)
                *packetCount = 1;
            txByteCount  = mbuf_pkthdr_len(*packetHead);
        }
    }
    else
    {
        error = ifnet_dequeue_multi(
                    _backingIfnet, maxCount,
                    packetHead, packetTail, packetCount, &txByteCount);
    }
    if (error)
        goto no_frames;

    // feed output tap
    if (_outputFilterFunc)
    {
        mbuf_t  m, n;

        m = *packetHead;
        assert(m);
        assert((mbuf_flags(m) & MBUF_PKTHDR));

        for (n = m; n != 0; n = mbuf_nextpkt(n))
            feedPacketOutputTap(n);
    }

    if (_txThreadSignal != _txThreadSignalLast)
    {
        // Update the stats that the driver maintains
        temp = _driverStats.outputErrors;
        delta = temp - _lastDriverStats.outputErrors;
        if (delta)
        {
            txErrors = ABS(delta);
            _lastDriverStats.outputErrors = temp;
        }

        temp = _driverStats.outputPackets;
        delta = temp - _lastDriverStats.outputPackets;
        txPackets = ABS(delta);
        _lastDriverStats.outputPackets = temp;
        _txThreadSignalLast = _txThreadSignal;
    }

    // update interface output byte count
    ifnet_stat_increment_out(_backingIfnet, txPackets, txByteCount, txErrors);
    if (packetBytes)
        *packetBytes = txByteCount;

    return kIOReturnSuccess;

no_frames:
    *packetHead  = 0;
    if (packetTail)
        packetTail = 0;
    if (packetCount)
        packetCount = 0;
    if (packetBytes)
        packetBytes = 0;
    return kIOReturnNoFrames;
}

IOReturn IONetworkInterface::dequeueOutputPacketsWithServiceClass(
    uint32_t                maxCount,
    IOMbufServiceClass      serviceClass,
    mbuf_t *                packetHead,
    mbuf_t *                packetTail,
    uint32_t *              packetCount,
    uint64_t *              packetBytes )
{
    uint32_t            txByteCount, temp, txPackets = 0, txErrors = 0;
    int                 delta;
    mbuf_svc_class_t    mbufSC;
    errno_t             error;

    if (!maxCount || !packetHead)
        return kIOReturnBadArgument;

    if (_txThreadState & (kTxThreadStateInit | kTxThreadStateDetach))
    {
        goto no_frames;
    }

    assert(_backingIfnet);

    // convert from I/O Kit SC to mbuf SC
    switch (serviceClass)
    {
        case kIOMbufServiceClassBE:    mbufSC = MBUF_SC_BE;  break;
        case kIOMbufServiceClassBKSYS: mbufSC = MBUF_SC_BK_SYS; break;
        case kIOMbufServiceClassBK:    mbufSC = MBUF_SC_BK;  break;
        case kIOMbufServiceClassRD:    mbufSC = MBUF_SC_RD;  break;
        case kIOMbufServiceClassOAM:   mbufSC = MBUF_SC_OAM; break;
        case kIOMbufServiceClassAV:    mbufSC = MBUF_SC_AV;  break;
        case kIOMbufServiceClassRV:    mbufSC = MBUF_SC_RV;  break;
        case kIOMbufServiceClassVI:    mbufSC = MBUF_SC_VI;  break;
        case kIOMbufServiceClassVO:    mbufSC = MBUF_SC_VO;  break;
        case kIOMbufServiceClassCTL:   mbufSC = MBUF_SC_CTL; break;
        default:
            return kIOReturnBadArgument;
    }

    if (maxCount == 1)
    {
        error = ifnet_dequeue_service_class(
                    _backingIfnet, mbufSC, packetHead);
        if (!error)
        {
            if (packetTail)
                *packetTail = *packetHead;
            if (packetCount)
                *packetCount = 1;
            txByteCount  = mbuf_pkthdr_len(*packetHead);
        }
    }
    else
    {
        error = ifnet_dequeue_service_class_multi(
                    _backingIfnet, mbufSC, maxCount,
                    packetHead, packetTail, packetCount, &txByteCount);
    }
    if (error)
        goto no_frames;

    // feed output tap
    if (_outputFilterFunc)
    {
        mbuf_t  m, n;

        m = *packetHead;
        assert(m);
        assert((mbuf_flags(m) & MBUF_PKTHDR));

        for (n = m; n != 0; n = mbuf_nextpkt(n))
            feedPacketOutputTap(n);
    }

    if (_txThreadSignal != _txThreadSignalLast)
    {
        // Update the stats that the driver maintains
        temp = _driverStats.outputErrors;
        delta = temp - _lastDriverStats.outputErrors;
        if (delta)
        {
            txErrors = ABS(delta);
            _lastDriverStats.outputErrors = temp;
        }

        temp = _driverStats.outputPackets;
        delta = temp - _lastDriverStats.outputPackets;
        txPackets = ABS(delta);
        _lastDriverStats.outputPackets = temp;
        _txThreadSignalLast = _txThreadSignal;
    }

    // update interface output byte count
    ifnet_stat_increment_out(_backingIfnet, txPackets, txByteCount, txErrors);
    if (packetBytes)
        *packetBytes = txByteCount;

    return kIOReturnSuccess;

no_frames:
    *packetHead  = 0;
    if (packetTail)
        packetTail = 0;
    if (packetCount)
        packetCount = 0;
    if (packetBytes)
        packetBytes = 0;
    return kIOReturnNoFrames;
}

//------------------------------------------------------------------------------

void IONetworkInterface::signalOutputThread( IOOptionBits options )
{
    // Unconditionally signal completion, to trigger drain loop retry
    UInt32 old = OSBitOrAtomic(kTxThreadWakeupSignal, &_txThreadFlags);

    // Interface detached from network stack
    if (_txThreadState & (kTxThreadStateInit | kTxThreadStateDetach))
    {
        return;
    }

    // Only wake if_start thread if drain loop left wakeup enabled
    if ((old & kTxThreadWakeupMask) == kTxThreadWakeupEnable)
    {
        assert(_backingIfnet);
        ifnet_start(_backingIfnet);
    }
    
    _txThreadSignal++;
}

//------------------------------------------------------------------------------

IOReturn IONetworkInterface::startOutputThread( IOOptionBits options )
{
    const uint32_t  mask  = (kTxThreadStateHalted | kTxThreadStateStop);
    IOReturn        error = kIOReturnUnsupported;
    bool            purge = false;

    DLOG("%s: %s(0x%x)\n",
        getName(), __FUNCTION__, _txThreadState);

    IOLockLock(_privateLock);
    if (_txThreadState & kTxThreadStateDetach)
    {
        error = kIOReturnNotAttached;
    }
    else
    {
        error = kIOReturnSuccess;
        purge = ((_txThreadState & kTxThreadStatePurge) != 0);        
        _txThreadState &= ~kTxThreadStatePurge;

        if (_txThreadState & mask)
        {
            _txThreadState &= ~mask;

            // No need to kick if_start thread if it hasn't called us yet.
            // This safety check covers the time before ifnet_attach.

            if ((_txThreadState & kTxThreadStateInit) == 0)
            {
                assert(_backingIfnet);
                if (purge)
                    ifnet_purge(_backingIfnet);
                ifnet_start(_backingIfnet);
            }
        }
    }
    IOLockUnlock(_privateLock);

    return error;
}

//------------------------------------------------------------------------------

IOReturn IONetworkInterface::haltOutputThread( uint32_t stateBit )
{
    AbsoluteTime    deadline;
    uint32_t        count = 0;
    const uint32_t  timeout = 100;
    IOReturn        error = kIOReturnSuccess;

    DLOG("%s: %s(0x%x, 0x%x)\n",
        getName(), __FUNCTION__, _txThreadState, stateBit);

    IOLockLock(_privateLock);

    do {
        // Prevent queue enable while we drop the lock
        _txThreadState |= (stateBit & kTxThreadStateDetach);
        _txThreadState &= ~kTxThreadStatePurge;

        // Already halted, thread may have terminated
        if (_txThreadState & kTxThreadStateHalted)
            break;

        if (!_txStartThread)
        {
            // Before the initial if_start call. Can update state directly
            // since this thread holds the lock that blocks if_start.
            _txThreadState |= kTxThreadStateHalted;
            break;
        }

        // if_start thread cannot call us without ifnet attach
        assert(_backingIfnet);

        if (current_thread() == _txStartThread)
        {
            // Driver called stop from if_start context
            _txThreadState |= kTxThreadStateHalted;
            break;
        }

        if (_txWorkLoop && _txWorkLoop->inGate())
        {
            // stopped from gated context
            _txThreadState |= kTxThreadStateHalted;
            break;
        }

        // Wait for halt confirmation from if_start thread
        while ((_txThreadState & kTxThreadStateHalted) == 0)
        {
            if (count)
            {
                LOG("%s: %s(0x%x, 0x%x) retry %u\n",
                    getName(), __FUNCTION__, _txThreadState, stateBit, count);
                if (count >= timeout)
                {
                    error = kIOReturnTimeout;
                    break;
                }
            }

            _txThreadState |= stateBit;
            clock_interval_to_deadline(100, kMillisecondScale, &deadline);
            ifnet_start(_backingIfnet);
            IOLockSleepDeadline(_privateLock, &_txThreadState,
                deadline, THREAD_UNINT);
            count++;
        }
    } while (false);

    IOLockUnlock(_privateLock);

    DLOG("%s: %s(0x%x, 0x%x) done after %u try\n",
        getName(), __FUNCTION__, _txThreadState, stateBit, count);

    return error;
}

IOReturn IONetworkInterface::stopOutputThread( IOOptionBits options )
{
    return haltOutputThread( kTxThreadStateStop );
}

//------------------------------------------------------------------------------

void IONetworkInterface::flushOutputQueue( IOOptionBits options )
{
    DLOG("%s: %s(0x%x)\n",
        getName(), __FUNCTION__, _txThreadState);

    IOLockLock(_privateLock);
    // synchronized with interface detach
    if (_backingIfnet && ((_txThreadState & kTxThreadStateDetach) == 0))
    {
        // Drop new packets if output thread stopped
        if (_txThreadState & kTxThreadStateHalted)
            _txThreadState |= kTxThreadStatePurge;

        ifnet_purge(_backingIfnet);
    }
    IOLockUnlock(_privateLock);
}

//------------------------------------------------------------------------------
// Stack-poll input model
//------------------------------------------------------------------------------

IOReturn IONetworkInterface::configureInputPacketPolling(
    uint32_t       driverQueueSize,
    IOOptionBits   options )
{
    IOReturn ret = kIOReturnError;

    IOLockLock(_privateLock);
    if ((_configFlags & kConfigFrozen) == 0)
    {
        _rxRingSize    = driverQueueSize;
        _rxPollOptions = options;
        _configFlags  |= kConfigRxPoll;
        ret = kIOReturnSuccess;
    }
    IOLockUnlock(_privateLock);
    return ret;
}

void IONetworkInterface::if_input_poll(
    ifnet_t     ifp,
    uint32_t    flags,
    uint32_t    max_count,
    mbuf_t *    first_packet,
    mbuf_t *    last_packet,
    uint32_t *  cnt,
    uint32_t *  len )
{
    IONetworkInterface *    me = IFNET_TO_THIS(ifp);
    IONetworkController *   driver;
    IOMbufQueue             queue;

    assert(ifp == me->_backingIfnet);
    assert(max_count != 0);

    driver = me->_driver;
    assert(driver);
    if (__builtin_expect(!driver, 0))
    {
        return;
    }

    me->_rxPollTotal++;

    IOMbufQueueInit(&queue);

    driver->pollInputPackets(me, max_count, &queue, 0);

    if (!IOMbufQueueIsEmpty(&queue))
    {
        *first_packet = queue.head;
        *last_packet  = queue.tail;
        *cnt          = queue.count;
        *len          = queue.bytes;
    }
    else
    {
        me->_rxPollEmpty++;
        *first_packet = 0;
        *last_packet  = 0;
        *cnt = 0;
        *len = 0;
    }
}

void IONetworkInterface::if_input_poll_gated(
    ifnet_t     ifp,
    uint32_t    flags,
    uint32_t    max_count,
    mbuf_t *    first_packet,
    mbuf_t *    last_packet,
    uint32_t *  cnt,
    uint32_t *  len )
{
    IONetworkInterface *    me = IFNET_TO_THIS(ifp);
    IONetworkController *   driver;
    IOMbufQueue             queue;

    assert(ifp == me->_backingIfnet);
    assert(max_count != 0);

    driver = me->_driver;
    assert(driver);
    if (__builtin_expect(!driver, 0))
    {
        return;
    }

    me->_rxPollTotal++;

    IOMbufQueueInit(&queue);

    driver->executeCommand(
            /* client */ me,
            /* action */ (IONetworkController::Action) me->_rxPollAction,
            /* target */ driver,
            /* param0 */ me,
            /* param1 */ (void *)(uintptr_t) max_count,
            /* param2 */ &queue,
            /* param3 */ 0 );

    if (!IOMbufQueueIsEmpty(&queue))
    {
        *first_packet = queue.head;
        *last_packet  = queue.tail;
        *cnt          = queue.count;
        *len          = queue.bytes;
    }
    else
    {
        me->_rxPollEmpty++;
        *first_packet = 0;
        *last_packet  = 0;
        *cnt = 0;
        *len = 0;
    }
}

//------------------------------------------------------------------------------

IOReturn IONetworkInterface::enqueueInputPacket(
    mbuf_t              packet,
    IOMbufQueue *       queue,
    IOOptionBits        options )
{
	const uint32_t      hdrlen = _hdrlen;
    void *              mdata;
    uint32_t            length;

    assert(packet);
    assert(_backingIfnet);

    if (!packet)
        return kIOReturnBadArgument;

    if (!_backingIfnet)
    {
        mbuf_freem(packet);
        return kIOReturnNotAttached;
    }

    if (!queue)
    {
        // use the push model queue
        queue = _inputPushQueue;
    }

    assert((mbuf_flags(packet) & MBUF_PKTHDR));
    assert((mbuf_nextpkt(packet) == 0));

	mbuf_pkthdr_setrcvif(packet, _backingIfnet);

    length = mbuf_pkthdr_len(packet);
    assert(length != 0);

    // check for special debugger packet 
    if (_remote_NMI_len) {
        const char *data = get_icmp_data(&packet, hdrlen, _remote_NMI_len);
        
        if (data && (memcmp(data, _remote_NMI_pattern, _remote_NMI_len) == 0)) {
            IOKernelDebugger::signalDebugger();
        } 
    }

    // input BPF tap
	if (_inputFilterFunc)
		feedPacketInputTap(packet);

    // frame header at start of mbuf data
    mdata = mbuf_data(packet);
	mbuf_pkthdr_setheader(packet, mdata);

    // packet header length does not include the frame header
    mbuf_pkthdr_setlen(packet, length - hdrlen);

    // adjust the mbuf data and length to exclude the frame header
    mbuf_setdata(packet, (char *)mdata + hdrlen, mbuf_len(packet) - hdrlen);

    IOMbufQueueTailAdd(queue, packet, length);
    return kIOReturnSuccess;
}

//------------------------------------------------------------------------------

errno_t IONetworkInterface::if_input_ctl( ifnet_t           ifp,
                                          ifnet_ctl_cmd_t   cmd,
                                          u_int32_t         arglen,
                                          void *            arg )
{
    IONetworkInterface *    me = IFNET_TO_THIS(ifp);
    IONetworkController *   driver;
    
    assert(ifp == me->_backingIfnet);

    driver = me->_driver;
    assert(driver);
    if (!driver)
    {
        return 0;
    }

    driver->executeCommand(
            /* client */ me,
            /* action */ (IONetworkController::Action) me->_rxCtlAction,
            /* target */ me,
            /* param0 */ driver,
            /* param1 */ (void *)(ifnet_ctl_cmd_t)(uintptr_t) cmd,
            /* param2 */ (void *)(uint32_t)(uintptr_t) arglen,
            /* param3 */ arg );

    return 0;
}

void IONetworkInterface::actionInputCtl( IONetworkController *  driver,
                                         ifnet_ctl_cmd_t        cmd,
                                         uint32_t               arglen,
                                         void *                 arg )
{
    ifnet_model_params * params = (ifnet_model_params *) arg;

    switch (cmd)
    {
        case IFNET_CTL_SET_INPUT_MODEL:
            if (arglen != sizeof(ifnet_model_params))
            {
                LOG("%s: SET_INPUT_MODEL bad params size %u != %u\n",
                    getName(), arglen, (uint32_t) sizeof(ifnet_model_params));
            }
            else if ((params->model != IFNET_MODEL_INPUT_POLL_OFF) &&
                     (params->model != IFNET_MODEL_INPUT_POLL_ON))
            {
                LOG("%s: SET_INPUT_MODEL unknown model 0x%x\n",
                    getName(), params->model);
            }
            else if (params->model != _rxPollModel)
            {
                // IFNET_MODEL_INPUT_POLL_OFF
                // IFNET_MODEL_INPUT_POLL_ON
                _rxPollModel = params->model;
                DLOG("%s: SET_INPUT_MODEL 0x%x\n", getName(), _rxPollModel);
                DLOG("%s: poll cnt %llu, empty %llu\n", getName(),
                    _rxPollTotal, _rxPollEmpty);
                _rxPollEmpty = 0;
                _rxPollTotal = 0;

                driver->setInputPacketPollingEnable(
                        this, _rxPollModel == IFNET_MODEL_INPUT_POLL_ON );
            }
            break;

        case IFNET_CTL_GET_INPUT_MODEL:
            if (arglen != sizeof(ifnet_model_params))
            {
                LOG("%s: GET_INPUT_MODEL bad params size %u != %u\n",
                    getName(), arglen, (uint32_t) sizeof(ifnet_model_params));
            }
            else
            {
                params->model = _rxPollModel;
            }
            break;

        default:
            DLOG("%s: if_input_ctl unknown cmd 0x%x\n", getName(), cmd);
    }
}
