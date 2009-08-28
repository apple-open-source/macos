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
}

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkController.h>
#include "IONetworkUserClient.h"
#include "IONetworkStack.h"
#include "IONetworkControllerPrivate.h"

#include <TargetConditionals.h>

//---------------------------------------------------------------------------

#define super IOService

OSDefineMetaClassAndAbstractStructors( IONetworkInterface, IOService )
OSMetaClassDefineReservedUnused( IONetworkInterface,  5);
OSMetaClassDefineReservedUnused( IONetworkInterface,  6);
OSMetaClassDefineReservedUnused( IONetworkInterface,  7);
OSMetaClassDefineReservedUnused( IONetworkInterface,  8);
OSMetaClassDefineReservedUnused( IONetworkInterface,  9);
OSMetaClassDefineReservedUnused( IONetworkInterface, 10);
OSMetaClassDefineReservedUnused( IONetworkInterface, 11);
OSMetaClassDefineReservedUnused( IONetworkInterface, 12);
OSMetaClassDefineReservedUnused( IONetworkInterface, 13);
OSMetaClassDefineReservedUnused( IONetworkInterface, 14);
OSMetaClassDefineReservedUnused( IONetworkInterface, 15);

//---------------------------------------------------------------------------
// Macros

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#define WAITING_FOR_DETACH(n)          ((n)->_clientVar[1])

#define _unit					_reserved->unit
#define _type					_reserved->type
#define _mtu					_reserved->mtu
#define _flags					_reserved->flags
#define _eflags					_reserved->eflags
#define _addrlen				_reserved->addrlen
#define _hdrlen					_reserved->hdrlen
#define _inputDeltas            _reserved->inputDeltas
#define _driverStats			_reserved->driverStats
#define _lastDriverStats		_reserved->lastDriverStats
#define _detachLock				_reserved->detachLock

#define kIONetworkControllerProperties  "IONetworkControllerProperties"

void IONetworkInterface::_syncToBackingIfnet()
{
	if(_backingIfnet == NULL)
		return;
	ifnet_set_mtu(_backingIfnet, _mtu);
	ifnet_set_flags(_backingIfnet, _flags, 0xffff);
// 3741463, we won't make eflags accessible any more	ifnet_set_eflags(_backingIfnet, _eflags, 0xffff);
	ifnet_set_addrlen(_backingIfnet, _addrlen);
	ifnet_set_hdrlen(_backingIfnet, _hdrlen);
}

void IONetworkInterface::_syncFromBackingIfnet() const
{
	if(_backingIfnet == NULL)
		return;
	_mtu = ifnet_mtu(_backingIfnet);
	_flags = ifnet_flags(_backingIfnet);
	_eflags = ifnet_eflags(_backingIfnet);
	_addrlen = ifnet_addrlen(_backingIfnet);
	_hdrlen = ifnet_hdrlen(_backingIfnet);
}

//---------------------------------------------------------------------------
// Initialize an IONetworkInterface instance.
//
// Returns true if initialized successfully, false otherwise.

bool IONetworkInterface::init(IONetworkController * controller)
{
    // Propagate the init() call to our superclass.

    if ( super::init() == false )
        return false;

	// a non-null value at this point means the subclass is pre-kpi and allocated its ifnet/arpcom itself.
	// we can't work with such a sublcass but at least we can fail gracefully.
	if(getIfnet())
	{
		IOLog("\33[00m\33[31m%s: IONetworkingFamily interface KPIs do not support IONetworkInterface subclasses that use ifnets\n\33[00m", getName());
		return false;
	}
	
    // The controller object provided must be valid.

    if ( OSDynamicCast(IONetworkController, controller) == 0 )
        return false;

    _controller = controller;

    // Allocate memory for the ExpansionData structure.

    _reserved = IONew( ExpansionData, 1 );
    if ( _reserved == 0 )
        return false;

    // Initialize the fields in the ExpansionData structure.

    bzero( _reserved, sizeof(ExpansionData) );

	_detachLock = IOLockAlloc();
	if( _detachLock == 0)
		return false;

    // Create interface lock to serialize ifnet updates.

    _ifLock = IORecursiveLockAlloc();
    if ( _ifLock == 0 )
        return false;

    // Create an OSNumber to store interface state bits.

    _stateBits = OSNumber::withNumber((UInt64) 0, 32);
    if ( _stateBits == 0 )
        return false;
    setProperty( kIOInterfaceState, _stateBits );

    // Create an OSSet to store client objects. Initial capacity
    // (which can grow) is set at 2 clients.

    _clientSet = OSSet::withCapacity(2);
    if ( _clientSet == 0 )
        return false;


    // Create a data dictionary.

    if ( (_dataDict = OSDictionary::withCapacity(5)) == 0 )
        return false;

    IONetworkData * data = IONetworkData::withExternalBuffer(
                                    kIONetworkStatsKey,
                                    sizeof(IONetworkStats),
									&_driverStats);
    if ( data )
    {
        addNetworkData(data);
        data->release();
    }

    // Register default output handler.

    if ( registerOutputHandler( controller,
                                controller->getOutputHandler() ) == false )
    {
        return false;
    }

    // Set the kIOInterfaceNamePrefix and kIOPrimaryInterface properties.
    // These may be used by an user space agent as hints when assigning a
    // BSD name for the interface.

    setProperty( kIOInterfaceNamePrefix, getNamePrefix() );
#if TARGET_OS_EMBEDDED
	OSString *networkType = OSDynamicCast(OSString, controller->getProperty( "IONetworkRootType" ));
	if(networkType)
		setProperty( "IONetworkRootType", networkType );
#endif /* TARGET_OS_EMBEDDED */

    if (IOService *provider = controller->getProvider())
	{
		bool gotLocation = false;
		
		setProperty( kIOBuiltin, (bool)( provider->getProperty( "built-in" ) ) );
		
		if(OSData *locationAsData = OSDynamicCast(OSData, provider->getProperty("location")))
		{
			//the data may be null terminated...but to be on the safe side, let's assume it's not, and add a null...
			if(OSData *locationAsCstr = OSData::withData(locationAsData)) //create a copy that we can convert to C string
			{
				locationAsCstr->appendByte(0, 1); //make sure it's null terminated;
				if(OSString *locationAsString = OSString::withCString((const char *)locationAsCstr->getBytesNoCopy())) //now create the OSString
				{
					gotLocation = true;
					setProperty( kIOLocation, locationAsString);
					locationAsString->release(); //setProperty took a ref
				}
				locationAsCstr->release();
			}
		}
		if(gotLocation == false)
			setProperty( kIOLocation, "");
	}
		
    return true;
}

//---------------------------------------------------------------------------
// Destroy the interface. Release all allocated resources.

void IONetworkInterface::free()
{
    DLOG("IONetworkInterface::free()\n");

    if ( _clientSet )
    {
        // Should not have any clients.
        assert(_clientSet->getCount() == 0);
        _clientSet->release();
        _clientSet = 0;
    }

    if ( _dataDict  ) { _dataDict->release();  _dataDict  = 0; }
    if ( _stateBits ) { _stateBits->release(); _stateBits = 0; }

    if ( _ifLock )
    {
        IORecursiveLockFree(_ifLock);
        _ifLock = 0;
    }

    // Free resources referenced through fields in the ExpansionData
    // structure, and also the structure itself.

    if ( _reserved )
    {
		clearInputQueue();  //relies on fields in the _reserved structure
		
		if( _detachLock )
			IOLockFree( _detachLock);
        IODelete( _reserved, ExpansionData, 1 );
        _reserved = 0;
    }


    super::free();
}

//---------------------------------------------------------------------------
// Returns true if the receiver of this method is the system's primary
// network interface.

bool IONetworkInterface::isPrimaryInterface() const
{
    IOService * provider  = getController();
    bool        isPrimary = false;

    if ( provider ) provider = provider->getProvider();

    // Look for the built-in property in the ethernet entry.

    if ( provider && provider->getProperty( "built-in" ) && getUnitNumber()==0)
    {
        isPrimary = true;
    }

    return isPrimary;
}

//---------------------------------------------------------------------------
// Get the IONetworkCotroller object that is servicing this interface.

IONetworkController * IONetworkInterface::getController() const
{
    return _controller;
}

//---------------------------------------------------------------------------
// Get the value that should be set in the hwassist field in the ifnet
// structure. Currently, this field is solely used for advertising the
// hardware checksumming support.

static UInt32 getIfnetHardwareAssistValue( IONetworkController * ctr )
{
    UInt32  input;
    UInt32  output;
    UInt32  hwassist = 0;

    do {
        if ( ctr->getChecksumSupport(
                     &input,
                     IONetworkController::kChecksumFamilyTCPIP,
                     false ) != kIOReturnSuccess ) break;
        
        if ( ctr->getChecksumSupport(
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
    }
    while ( false );

	if( ctr->getFeatures() & kIONetworkFeatureHardwareVlan)
		hwassist |= IFNET_VLAN_TAGGING;
	
	if( ctr->getFeatures() & kIONetworkFeatureSoftwareVlan)
		hwassist |= IFNET_VLAN_MTU;
	
	if( ctr->getFeatures() & kIONetworkFeatureMultiPages)
		hwassist |= IFNET_MULTIPAGES;

	if( ctr->getFeatures() & kIONetworkFeatureTSOIPv4)
		hwassist |= IFNET_TSO_IPV4;

	if( ctr->getFeatures() & kIONetworkFeatureTSOIPv6)
		hwassist |= IFNET_TSO_IPV6;

    return hwassist;
}

//---------------------------------------------------------------------------
// Initialize the ifnet structure.
    
bool IONetworkInterface::initIfnet(struct ifnet * ifp)
{
	return false;
}

OSMetaClassDefineReservedUsed(IONetworkInterface, 4);
bool IONetworkInterface::initIfnetParams(struct ifnet_init_params *params)
{
    // Register our 'shim' functions. These function pointers
    // points to static member functions inside this class.
	params->name		= (char *) getNamePrefix();
	params->type		= _type;
	params->unit		= _unit;
	params->output		= output_shim;
	params->ioctl		= ioctl_shim;
	params->set_bpf_tap = set_bpf_tap_shim;
	params->detach		= detach_shim;
	params->softc		= this;

    return true;
}

//---------------------------------------------------------------------------
// Implement family specific matching.

bool IONetworkInterface::matchPropertyTable(OSDictionary * table,
                                            SInt32       * score)
{ 
    return super::matchPropertyTable(table, score);
}

//---------------------------------------------------------------------------
// Take the interface lock.

void IONetworkInterface::lock()
{
    IORecursiveLockLock(_ifLock);
}

//---------------------------------------------------------------------------
// Release the interface lock.

void IONetworkInterface::unlock()
{
    IORecursiveLockUnlock(_ifLock);
}

//---------------------------------------------------------------------------
// Inspect the controller after it has been opened.

bool IONetworkInterface::controllerDidOpen(IONetworkController * controller)
{
    return true;   // by default, always accept the controller open.
}

//---------------------------------------------------------------------------
// Perform cleanup before the controller is closed.

void IONetworkInterface::controllerWillClose(IONetworkController * controller)
{
}

//---------------------------------------------------------------------------
// Handle a client open on the interface.

bool IONetworkInterface::handleOpen(IOService *  client,
                                    IOOptionBits options,
                                    void *       argument)
{
    bool  accept         = false;
    bool  controllerOpen = false;

    do {
        // Was this object already registered as our client?

        if ( _clientSet->containsObject(client) )
        {
            DLOG("%s: multiple opens from client %lx\n",
                 getName(), (UInt32) client);
            accept = true;
            break;
        }

        // If the interface has not received a client open, which also
        // implies that the interface has not yet opened the controller,
        // then open the controller upon receiving the first open from
        // a client. If the controller open fails, the client open will
        // be rejected.

        if ( ( getInterfaceState() & kIONetworkInterfaceOpenedState ) == 0 )
        {
            if ( ( (controllerOpen = _controller->open(this)) == false ) ||
                 ( controllerDidOpen(_controller) == false ) )
                break;
        }

        // Qualify the client.

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
            _controller->registerInterestedDriver( this );
        }
        else {
            controllerWillClose(_controller);
            _controller->close(this);
        }
    }

    return accept;
}

//---------------------------------------------------------------------------
// Handle a client close on the interface.

void IONetworkInterface::handleClose(IOService * client, IOOptionBits options)
{
    // Remove the object from the client OSSet.

    if ( _clientSet->containsObject(client) )
    {
        // Call handleClientClose() to handle the client close.

        handleClientClose( client, options );

        // If this is the last client, then close our provider.

        if ( _clientSet->getCount() == 1 )
        {
            _controller->deRegisterInterestedDriver( this );
            controllerWillClose( _controller );
            _controller->close( this );
            setInterfaceState( 0, kIONetworkInterfaceOpenedState );
        }

        // Remove the client from our OSSet.

        _clientSet->removeObject(client);
    }
}

//---------------------------------------------------------------------------
// Query whether a client has an open on the interface.

bool IONetworkInterface::handleIsOpen(const IOService * client) const
{
    if (client)
        return _clientSet->containsObject(client);
    else
        return (_clientSet->getCount() > 0);
}

//---------------------------------------------------------------------------
// Handle a client open on the interface.

bool IONetworkInterface::handleClientOpen(IOService *  client,
                                          IOOptionBits options,
                                          void *       argument)
{
    return true;
}

//---------------------------------------------------------------------------
// Handle a client close on the interface.

void IONetworkInterface::handleClientClose(IOService *  client,
                                           IOOptionBits options)
{
}

//---------------------------------------------------------------------------
// Register the output packet handler.

bool IONetworkInterface::registerOutputHandler(OSObject *      target,
                                               IOOutputAction  action)
{
    lock();

    // Sanity check on the arguments.

    if ( (getInterfaceState() & kIONetworkInterfaceOpenedState) ||
         !target || !action )
    {
        unlock();
        return false;
    }

    _outTarget = target;
    _outAction = action;

    unlock();

    return true;
}

//---------------------------------------------------------------------------
// Feed packets to the input/output BPF packet filter taps.
OSMetaClassDefineReservedUsed(IONetworkInterface, 2);

void IONetworkInterface::feedPacketInputTap(mbuf_t  m)
{
	bpf_packet_func inFilter = _inputFilterFunc; //PR3662433 we're not protected from this getting changed out from under us
	if(inFilter)						// so double check it hasn't be set to null and use a local ptr to avoid the race
		inFilter(_backingIfnet, m); 
}

OSMetaClassDefineReservedUsed(IONetworkInterface, 3);

void IONetworkInterface::feedPacketOutputTap(mbuf_t m)
{
	bpf_packet_func outFilter = _outputFilterFunc; //see comment in feedPacketInputTap
	if(outFilter)
		outFilter(_backingIfnet, m);
}

//---------------------------------------------------------------------------
// Called by a network controller to submit a single packet received from
// the network to the data link layer.
#define ABS(a)  ((a) < 0 ? -(a) : (a))

#define IN_Q_ENQUEUE(m)                   \
{                                         \
    if (_inputQHead == 0) {               \
        _inputQHead = _inputQTail = (m);  \
    }                                     \
    else {                                \
       mbuf_setnextpkt(_inputQTail, (m)) ;    \
        _inputQTail = (m);                \
    }                                     \
    _inputQCount++;                       \
}

__inline__  void IONetworkInterface::DLIL_INPUT(mbuf_t m_head)
{
    if ( m_head ) {
		UInt32 noraceTemp;
		int delta;
		
		//_inputDeltas.bytes_in already contains the count accumlated between calls to DLIL_INPUT
		
		// now copy over the stats that the driver maintains.
		noraceTemp = _driverStats.inputPackets;
		delta = noraceTemp - _lastDriverStats.inputPackets ;
		_inputDeltas.packets_in = ABS(delta);
		_lastDriverStats.inputPackets = noraceTemp;

		noraceTemp = _driverStats.inputErrors;
		delta =  noraceTemp - _lastDriverStats.inputErrors;
		_inputDeltas.errors_in = ABS(delta);
		_lastDriverStats.inputErrors = noraceTemp;

		noraceTemp = _driverStats.collisions;
		delta = noraceTemp - _lastDriverStats.collisions;
		_inputDeltas.collisions = ABS(delta);
		_lastDriverStats.collisions = noraceTemp;
		
        ifnet_input(_backingIfnet, m_head, &_inputDeltas);
		_inputDeltas.bytes_in = 0;	//reset to 0
    }
}

UInt32 IONetworkInterface::flushInputQueue()
{
    UInt32 count = _inputQCount;

    DLIL_INPUT(_inputQHead);
    _inputQHead  = _inputQTail = 0;
    _inputQCount = 0;

    return count;
}

UInt32 IONetworkInterface::clearInputQueue()
{
    UInt32 count = _inputQCount;

    mbuf_freem_list( _inputQHead );
	_inputDeltas.bytes_in = 0;
    _inputQHead = _inputQTail = 0;
    _inputQCount = 0;

    return count;
}


UInt32 IONetworkInterface::inputPacket(mbuf_t pkt,
                                       UInt32        length,
                                       IOOptionBits  options,
                                       void *        param)
{
    UInt32 count;
	UInt32 hdrlen = getMediaHeaderLength();
    assert(pkt);


    // Set the source interface and length of the received frame.

    if ( length )
    {
        if ( mbuf_next(pkt) == 0 )
        {
            mbuf_pkthdr_setlen(pkt, length);
			mbuf_setlen(pkt, length);
        }
        else
        {
            mbuf_t m   = pkt;
            mbuf_pkthdr_setlen(pkt, length);
            do {
                if (length < (UInt32) mbuf_len(m))
                    mbuf_setlen(m, length);
                length -= mbuf_len(m);
            } while (( m = mbuf_next(m) ));
            assert(length == 0);
        }
    }

	mbuf_pkthdr_setrcvif(pkt, _backingIfnet);
    
    // Increment input byte count. (accumulate until DLIL_INPUT is called)
	_inputDeltas.bytes_in += mbuf_pkthdr_len(pkt);
	

    // Feed BPF tap.
	if(_inputFilterFunc)
		feedPacketInputTap(pkt);
	
	mbuf_pkthdr_setheader(pkt, mbuf_data(pkt));
    mbuf_pkthdr_setlen(pkt, mbuf_pkthdr_len(pkt) - hdrlen);
    mbuf_setdata(pkt, (char *)mbuf_data(pkt) + hdrlen, mbuf_len(pkt) - hdrlen);

    if ( options & kInputOptionQueuePacket )
    {
        IN_Q_ENQUEUE(pkt);
        count = 0;
    }
    else
    {
        if ( _inputQHead )  // queue is not empty
        {
            IN_Q_ENQUEUE(pkt);

            count = _inputQCount;
            DLIL_INPUT(_inputQHead);
            _inputQHead  = _inputQTail = 0;
            _inputQCount = 0;
        }
        else
        {
            DLIL_INPUT(pkt);
            count = 1;
        }
    }
    return count;
}

//---------------------------------------------------------------------------
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
            // Send an event only if DLIL has a reference to this
            // interface.
            if ( _backingIfnet )
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

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
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
            break;  // unable to allocate memory, or no medium dictionary.
        }

        if ((mediumCount = mediumDict->getCount()) == 0)
            break;  // no medium in the medium dictionary

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

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
// if_ioctl() handler - Calls performCommand() when we receive an ioctl
// from DLIL.

errno_t
IONetworkInterface::ioctl_shim(ifnet_t ifn, unsigned long cmd, void * data)
{
//    assert(ifp && ifp->if_private);

    IONetworkInterface * self = (IONetworkInterface *) ifnet_softc(ifn);

//    assert(ifp == self->_ifp);

    return self->performCommand( self->_controller,
                                 cmd,
                                 (void *) ifn,
                                 data );
}

//---------------------------------------------------------------------------
// if_output() handler.
//
// Handle a call from the network stack to transmit the given mbuf.
// For now, we can assume that the mbuf is singular, and never chained.

int IONetworkInterface::output_shim(ifnet_t ifn, mbuf_t m)
{
	UInt32 noraceTemp;
	int delta;
	u_int32_t outPackets, outErrors;
	
//    assert(ifp && ifp->if_private);

    IONetworkInterface * self = (IONetworkInterface *) ifnet_softc(ifn);
    
//    assert(ifp == self->_ifp);

    if ( m == 0 )
    {
        DLOG("IONetworkInterface: NULL output mbuf\n");
        return EINVAL;
    }

    if ( (mbuf_flags(m) & M_PKTHDR) == 0 )
    {
        DLOG("IONetworkInterface: M_PKTHDR bit not set\n");
        mbuf_freem(m);
        return EINVAL;
    }

    // Increment output related statistics.
	// There is a race condition if this function in entered by more than one packet
	// at the same time: the fields in _lastDriverStats could get an incorrect update from the other thread.
	// As of Tiger, IP transmit is single threaded though.
	
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

//---------------------------------------------------------------------------
// if_set_bpf_tap() handler. Handles request from the DLIL to enable or
// disable the input/output filter taps.
//
// FIXME - locking may be needed.

errno_t IONetworkInterface::set_bpf_tap_shim(ifnet_t ifn,
                                         bpf_tap_mode            mode,
                                         bpf_packet_func       func)
{
//    assert(ifp && ifp->if_private);

    IONetworkInterface * self = (IONetworkInterface *) ifnet_softc(ifn);

//    assert(ifp == self->_ifp);

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
            DLOG("IONetworkInterface: Unknown BPF tap mode %d\n", mode);
            break;
    }

    return 0;
}

void IONetworkInterface::detach_shim(ifnet_t ifn)
{
	IONetworkInterface * self = (IONetworkInterface *) ifnet_softc(ifn);
	
	IOLockLock( self->_detachLock);
	WAITING_FOR_DETACH(self) = 0;
	thread_wakeup( (void *) self->getIfnet() );
	IOLockUnlock( self->_detachLock);
}

//---------------------------------------------------------------------------
// ifnet field (and property table) getter/setter.

bool IONetworkInterface::_setInterfaceProperty(UInt32  value,
                                               UInt32  mask,
                                               UInt32  bytes,
                                               void *  addr,
                                               char *  key)
{
    bool    updateOk = true;
    UInt32  newValue;

    // Update the property in ifnet.

    switch (bytes)
    {
        case 1:
            newValue = (*((UInt8 *) addr) & mask) | value;
            *((UInt8 *) addr) = (UInt8) newValue;
            break;
        case 2:
            newValue = (*((UInt16 *) addr) & mask) | value;
            *((UInt16 *) addr) = (UInt16) newValue;
            break;
        case 4:
            newValue = (*((UInt32 *) addr) & mask) | value;
            *((UInt32 *) addr) = (UInt32) newValue;
            break;
        default:
            updateOk = false;
            break;
    }

    return updateOk;
}

#define IO_IFNET_GET(func, type, field)                        \
type IONetworkInterface::func() const                          \
{																\
	_syncFromBackingIfnet();	\
	return _##field;										\
}

#define IO_IFNET_SET(func, type, field, propName)              \
bool IONetworkInterface::func(type value)                      \
{                                                              \
    _##field = value;											\
	_syncToBackingIfnet();										\
	return true;											\
}

#define IO_IFNET_RMW(func, type, field, propName)              \
bool IONetworkInterface::func(type set, type clear)          \
{															\
	_##field = (_##field & ~clear) | set; \
	_syncToBackingIfnet();  \
	return true;											\
}

//---------------------------------------------------------------------------
// Interface type accessors (ifp->if_type). The list of interface types is
// defined in <bsd/net/if_types.h>.

//IO_IFNET_SET(setInterfaceType, UInt8, type, kIOInterfaceType)
bool IONetworkInterface::setInterfaceType(UInt8 type)
{
	// once attached to dlil, we can't change the interface type
	if(_backingIfnet)
		return false;
	else
		_type = type;
	return true;
}
IO_IFNET_GET(getInterfaceType, UInt8, type)

//---------------------------------------------------------------------------
// Mtu (MaxTransferUnit) accessors (ifp->if_mtu).

IO_IFNET_SET(setMaxTransferUnit, UInt32, mtu, kIOMaxTransferUnit)
IO_IFNET_GET(getMaxTransferUnit, UInt32, mtu)

//---------------------------------------------------------------------------
// Flags accessors (ifp->if_flags). This is a read-modify-write operation.

IO_IFNET_RMW(setFlags, UInt16, flags, kIOInterfaceFlags)
IO_IFNET_GET(getFlags, UInt16, flags)

//---------------------------------------------------------------------------
// EFlags accessors (ifp->if_eflags). This is a read-modify-write operation.

IO_IFNET_RMW(setExtraFlags, UInt32, eflags, kIOInterfaceExtraFlags)
IO_IFNET_GET(getExtraFlags, UInt32, eflags)

//---------------------------------------------------------------------------
// MediaAddressLength accessors (ifp->if_addrlen)

IO_IFNET_SET(setMediaAddressLength, UInt8, addrlen, kIOMediaAddressLength)
IO_IFNET_GET(getMediaAddressLength, UInt8, addrlen)

//---------------------------------------------------------------------------
// MediaHeaderLength accessors (ifp->if_hdrlen)

IO_IFNET_SET(setMediaHeaderLength, UInt8, hdrlen, kIOMediaHeaderLength)
IO_IFNET_GET(getMediaHeaderLength, UInt8, hdrlen)

//---------------------------------------------------------------------------
// Interface unit number. The unit number for the interface is assigned
// by our client.

bool IONetworkInterface::setUnitNumber( UInt16 value )
{
	if(_backingIfnet)   //once we've attached to dlil, unit can't be changed
		return false;
	
    if ( setProperty( kIOInterfaceUnit, value, 16 ) )
    {
        _unit = value;
		setProperty( kIOPrimaryInterface, isPrimaryInterface() );
        return true;
    }
    else
        return false;
}

UInt16 IONetworkInterface::getUnitNumber(void) const
{
	if(_backingIfnet)
		return ifnet_unit(_backingIfnet);
	else
		return _unit;
}

//---------------------------------------------------------------------------
// Return true if the interface has been registered with the network layer,
// false otherwise.

bool IONetworkInterface::isRegistered() const
{
    return (bool)(getInterfaceState() & kIONetworkInterfaceRegisteredState);
}

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
// Return the interface state flags.

UInt32 IONetworkInterface::getInterfaceState() const
{
    return _stateBits->unsigned32BitValue();
}

//---------------------------------------------------------------------------
// Set (or clear) the interface state flags.

UInt32 IONetworkInterface::setInterfaceState( UInt32 set,
                                              UInt32 clear )
{
    UInt32  val;

    assert( _stateBits );

    lock();

    val = ( _stateBits->unsigned32BitValue() | set ) & ~clear;
    _stateBits->setValue( val );

    unlock();

    return val;
}

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
// Remove an entry from the IONetworkData dictionary managed by the interface.
// The removed object is released.

bool IONetworkInterface::removeNetworkData(const OSSymbol * aKey)
{
    bool ret = false;

    lock();

    do {
        if ( getInterfaceState() & kIONetworkInterfaceOpenedState )
            break;

        _dataDict->removeObject(aKey);
        ret = _syncNetworkDataDict();
    }
    while (0);

    unlock();

    return ret;
}

bool IONetworkInterface::removeNetworkData(const char * aKey)
{
    bool ret = false;

    lock();

    do {
        if ( getInterfaceState() & kIONetworkInterfaceOpenedState )
            break;

        _dataDict->removeObject(aKey);
        ret = _syncNetworkDataDict();
    }
    while (0);

    unlock();

    return ret;
}

//---------------------------------------------------------------------------
// Add an IONetworkData object to a dictionary managed by the interface.

bool IONetworkInterface::addNetworkData(IONetworkData * aData)
{
    bool ret = false;

    if (OSDynamicCast(IONetworkData, aData) == 0)
        return false;

    lock();

    if (( getInterfaceState() & kIONetworkInterfaceOpenedState ) == 0)
    {
        if ((ret = _dataDict->setObject(aData->getKey(), aData)))
            ret = _syncNetworkDataDict();
    }

    unlock();

    return ret;
}

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
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

    if ( param1 == 0 )
    {
        // Issue a call to this same function synchronized with the
        // work loop thread.

        self->getController()->executeCommand(
              /* client */ self,
              /* action */ (IONetworkController::Action) &powerChangeHandler,
              /* target */ self,
              /* param0 */ param0,
              /* param1 */ (void *) true );

        return;
    }

    notice = (IONetworkPowerChangeNotice *) param0;
    assert( notice );

    DLOG("%s: power change flags:%08x, state:%d, from:%p, phase:%d\n",
         self->getName(), (uint32_t) notice->powerFlags,
         notice->stateNumber, notice->policyMaker, notice->phase );

    if ( notice->phase == kPhasePowerStateWillChange )
    {
        self->controllerWillChangePowerState(
              self->getController(),
              notice->powerFlags,
              notice->stateNumber,
              notice->policyMaker );
    }
    else if ( notice->phase == kPhasePowerStateDidChange )
    {
        self->controllerDidChangePowerState(
              self->getController(),
              notice->powerFlags,
              notice->stateNumber,
              notice->policyMaker );
    }
}

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------

IOReturn IONetworkInterface::message( UInt32 type, IOService * provider,
                                      void * argument )
{
    if (kMessageControllerWillShutdown == type)
    {
        // Handle shutdown or restarts the same as when the controller
        // is transitioning to an "unusable" power state.

        powerStateWillChangeTo( 0, 0, NULL );
        return kIOReturnSuccess;
    }
    
    return super::message( type, provider, argument );
}

//---------------------------------------------------------------------------
// Handle a request to set properties from kernel or non-kernel clients.
// For non-kernel clients, the preferred mechanism is through an user
// client.

IOReturn IONetworkInterface::setProperties( OSObject * properties )
{
    IOReturn       ret;
    OSDictionary * dict;
    OSObject *     obj;

    // Controller properties are routed to our provider.

    if (( dict = OSDynamicCast( OSDictionary, properties ) ) &&
        ( obj  = dict->getObject( kIONetworkControllerProperties ) ))
    {
        ret = _controller->setProperties( obj );
    }
    else
    {
        ret = super::setProperties(properties);
    }

    return ret;
}

//---------------------------------------------------------------------------
// willTerminate

bool IONetworkInterface::willTerminate( IOService *  provider,
                                        IOOptionBits options )
{
    DLOG("%s::%s\n", getName(), __FUNCTION__);

    // Mark the interface as disabled.

    setInterfaceState( kIONetworkInterfaceDisabledState );

    return super::willTerminate( provider, options );
}

//---------------------------------------------------------------------------
// Inlined functions pulled from header file to ensure
// binary compatibility with drivers built with gcc2.95.

IONetworkData * IONetworkInterface::getParameter(const char * aKey) const
{ return getNetworkData(aKey); }

bool IONetworkInterface::setExtendedFlags(UInt32 flags, UInt32 clear)
{ return true; }

//---------------------------------------------------------------------------

OSMetaClassDefineReservedUsed(IONetworkInterface, 0);

IOReturn IONetworkInterface::attachToDataLinkLayer( IOOptionBits options,
                                                    void *       parameter )
{
    ifnet_init_params iparams;
	struct sockaddr_dl *ll_addr;
	char buffer[2*sizeof(struct sockaddr_dl)];
	UInt32 asize;
	IOReturn ret = kIOReturnInternalError;

	memset(&iparams, 0, sizeof(iparams));
	initIfnetParams(&iparams);

	if(ifnet_allocate( &iparams, &_backingIfnet))
		return kIOReturnNoMemory;


	ifnet_set_offload (_backingIfnet, getIfnetHardwareAssistValue( getController() ));		

	// prepare the link-layer address structure	
	memset(buffer, 0, sizeof(buffer));
	ll_addr = (struct sockaddr_dl *)buffer;
	asize = sizeof(buffer) - offsetof(struct sockaddr_dl, sdl_data);

	do{
		OSData *hardAddr;
		hardAddr = OSDynamicCast(OSData, getController()->getProperty(kIOMACAddress));
		if(hardAddr == NULL || hardAddr->getLength() > asize)
			continue;
		
		asize = hardAddr->getLength();
		bcopy(hardAddr->getBytesNoCopy(), ll_addr->sdl_data, asize);
		ll_addr->sdl_len = offsetof(struct sockaddr_dl, sdl_data) + asize;
		ll_addr->sdl_family = AF_LINK;
		ll_addr->sdl_alen = asize;
		_syncToBackingIfnet();

		if(ifnet_attach(_backingIfnet, ll_addr) == 0)
			return kIOReturnSuccess;
	}while(false);


	//error condition, clean up.
	ifnet_release(_backingIfnet);
	_backingIfnet = NULL;
	return ret;
}

//---------------------------------------------------------------------------

OSMetaClassDefineReservedUsed(IONetworkInterface, 1);

void IONetworkInterface::detachFromDataLinkLayer( IOOptionBits options,
                                                  void *       parameter )
{
    WAITING_FOR_DETACH(this) = 1;
	ifnet_detach( _backingIfnet ); //this will lead to detach_shim getting called on another thread.

	IOLockLock(_detachLock); // protect against the detach_shim running before we block
    if ( WAITING_FOR_DETACH( this ) ) //if this is false, it means detach_shim has already done its thing
    {
		// otherwise, prepare to release the lock...
        assert_wait( (event_t) _backingIfnet, THREAD_UNINT );
		IOLockUnlock( _detachLock );
		// and block waiting for detach_shim.
        thread_block( THREAD_CONTINUE_NULL );
		//reacquire lock to ensure we don't continue until detach_shim is totally done.
		IOLockLock(_detachLock);
    }
	// at this point detach_shim has done its thing, so we can safely continue with tear-down
	IOLockUnlock(_detachLock);
	ifnet_release(_backingIfnet);
	_backingIfnet = NULL;
}

ifnet_t IONetworkInterface::getIfnet() const
{
	return _backingIfnet;
}


