/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved. 
 *
 * IOFWInterface.cpp
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSData.h>
#include "IOFWInterface.h"
#include "IOFWController.h"
//#include <IOKit/network/IONetworkUserClient.h>
#include <IOKit/pwr_mgt/RootDomain.h>	// publishFeature()

extern "C" {
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <net/if.h>
#include <firewire.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/dlil.h>
#include <net/bpf.h>
#include <net/kpi_protocol.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_arp.h>
#include <netinet/if_ether.h>

#include <sys/sockio.h>
#include <sys/malloc.h>
}
#include "if_firewire.h"

#ifndef DLT_APPLE_IP_OVER_IEEE1394
#define DLT_APPLE_IP_OVER_IEEE1394 138
#endif

//---------------------------------------------------------------------------

#define super IONetworkInterface

OSDefineMetaClassAndStructors( IOFWInterface, IONetworkInterface )
OSMetaClassDefineReservedUnused( IOFWInterface,  0);
OSMetaClassDefineReservedUnused( IOFWInterface,  1);
OSMetaClassDefineReservedUnused( IOFWInterface,  2);
OSMetaClassDefineReservedUnused( IOFWInterface,  3);
OSMetaClassDefineReservedUnused( IOFWInterface,  4);
OSMetaClassDefineReservedUnused( IOFWInterface,  5);
OSMetaClassDefineReservedUnused( IOFWInterface,  6);
OSMetaClassDefineReservedUnused( IOFWInterface,  7);
OSMetaClassDefineReservedUnused( IOFWInterface,  8);
OSMetaClassDefineReservedUnused( IOFWInterface,  9);
OSMetaClassDefineReservedUnused( IOFWInterface, 10);
OSMetaClassDefineReservedUnused( IOFWInterface, 11);
OSMetaClassDefineReservedUnused( IOFWInterface, 12);
OSMetaClassDefineReservedUnused( IOFWInterface, 13);
OSMetaClassDefineReservedUnused( IOFWInterface, 14);
OSMetaClassDefineReservedUnused( IOFWInterface, 15);

// The name prefix for all BSD FireWire interfaces.
// 
#define kIOFWInterfaceNamePrefix      "fw"

// Options used for enableFilter(), disableFilter().
enum {
    kFilterOptionDeferIO          = 0x0001,
    kFilterOptionNotInsideGate    = 0x0002,
    kFilterOptionNoStateChange    = 0x0004,
    kFilterOptionDisableZeroBits  = 0x0008,
    kFilterOptionSyncPendingIO    = 0x0010
};
static u_long ivedonethis = 0;

//---------------------------------------------------------------------------
// Macros

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

UInt32 IOFWInterface::getFilters(const OSDictionary *dict,
								const OSSymbol		*group)
{
    OSNumber * num;
    UInt32     filters = 0;

    assert( dict && group );

    if (( num = (OSNumber *) dict->getObject(group) ))
    {
        filters = num->unsigned32BitValue();
    }
    return filters;
}

bool IOFWInterface::setFilters(OSDictionary		*dict,
								const OSSymbol	*group,
								UInt32           filters)
{
    OSNumber * num;
    bool       ret = false;

    assert( dict && group );

    num = (OSNumber *) dict->getObject(group);
    if ( num == 0 )
    {
        if (( num = OSNumber::withNumber(filters, 32) ))
        {
            ret = dict->setObject(group, num);
            num->release();
        }
    }
    else
    {
        num->setValue(filters);
        ret = true;
    }
    return ret;
}

#define GET_REQUIRED_FILTERS(g)     getFilters(_requiredFilters, (g))
#define GET_ACTIVE_FILTERS(g)       getFilters(_activeFilters, (g))
#define GET_SUPPORTED_FILTERS(g)    getFilters(_supportedFilters, (g))

#define SET_REQUIRED_FILTERS(g, v)  setFilters(_requiredFilters, (g), (v))
#define SET_ACTIVE_FILTERS(g, v)    setFilters(_activeFilters, (g), (v))

//---------------------------------------------------------------------------
// Initialize an IOFWInterface instance. Instance variables are
// initialized
bool IOFWInterface::init(IONetworkController *controller)
{
    // Pass the init() call to our superclass.
    if ( super::init(controller) == false )
        return false;


	// initialize firewire specific fields.
    setInterfaceType( IFT_IEEE1394 ); 
	setMaxTransferUnit( FIREWIREMTU );
    setMediaAddressLength( kIOFWAddressSize );
    setMediaHeaderLength( FIREWIRE_HDR_LEN );
    setFlags( IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS,
              IFF_RUNNING   | IFF_MULTICAST );

			  
    // Add an IONetworkData with room to hold an IOFWStats structure.
    // This class does not reference the data object created, and no harm
    // is done if the data object is released or replaced.

    IONetworkData * data = IONetworkData::withInternalBuffer(
                                              kIOFWStatsKey,
                                              sizeof(IOFWStats));
    if (data)
    {
        addNetworkData(data);
        data->release();
    }

    // Create and initialize the filter dictionaries.

    _requiredFilters = OSDictionary::withCapacity(4);
    _activeFilters   = OSDictionary::withCapacity(4);

    if ( (_requiredFilters == 0) || (_activeFilters == 0) )
        return false;

    _supportedFilters = OSDynamicCast(OSDictionary,
                        controller->getProperty(kIOPacketFilters));
    if ( _supportedFilters == 0 ) return false;
    _supportedFilters->retain();

    // Controller's Unicast (directed) and Broadcast filters should always
    // be enabled. Those bits should never be cleared.

    if ( !SET_REQUIRED_FILTERS( gIONetworkFilterGroup,
                                kIOPacketFilterUnicast |
                                kIOPacketFilterBroadcast )
      || !SET_REQUIRED_FILTERS( gIOEthernetWakeOnLANFilterGroup, 0 )
      || !SET_ACTIVE_FILTERS(   gIONetworkFilterGroup, 0 )
      || !SET_ACTIVE_FILTERS(   gIOEthernetWakeOnLANFilterGroup, 0 ) )
    {
         return false;
    }

    // Publish filter dictionaries to property table.

    setProperty( kIORequiredPacketFilters, _requiredFilters );
    setProperty( kIOActivePacketFilters,   _activeFilters );

	_controller = controller;

    return true;
}

//---------------------------------------------------------------------------
// Initialize the given ifnet structure. The argument specified is a pointer
// to an ifnet structure obtained through getIfnet(). IOFWInterface
// will initialize this structure in a manner that is appropriate for most
// FireWire interfaces, then call super::initIfnet() to allow the superclass
// to perform generic interface initialization.
//
// ifp: Pointer to the ifnet structure to be initialized.
//
// Returns true on success, false otherwise.
int firewire_frameout(ifnet_t   ifp, mbuf_t *m,
					const struct sockaddr *ndest, const char *edst, const char *fw_type);
int firewire_demux(ifnet_t ifp, mbuf_t m,char *frame_header,protocol_family_t *protocol_family);
int firewire_add_proto(ifnet_t   ifp, protocol_family_t protocol, const struct ifnet_demux_desc *demux_list, u_int32_t demux_count);
int firewire_del_proto(ifnet_t   ifp, protocol_family_t protocol_family);
int firewire_add_if(ifnet_t ifp);

bool IOFWInterface::initIfnetParams(struct ifnet_init_params *params)
{
	//get the default values
	super::initIfnetParams( params );
	
    _uniqueID = OSDynamicCast(OSData, getProvider()->getProperty(kIOMACAddress));
    if ( (_uniqueID == 0) || (_uniqueID->getLength() != kIOFWAddressSize) )
    {
        DLOG("%s: kIOMACAddress property access error (len %d)\n",
             getName(), _uniqueID ? _uniqueID->getLength() : 0);
        return false;
    }

	// fill in firewire specific values
	params->uniqueid = _uniqueID->getBytesNoCopy();
	params->uniqueid_len = _uniqueID->getLength();
	params->family = APPLE_IF_FAM_FIREWIRE;
	params->demux = firewire_demux;
	params->add_proto = firewire_add_proto;
	params->del_proto = firewire_del_proto;
	params->framer = firewire_frameout;
	params->broadcast_addr	= fwbroadcastaddr;
	params->broadcast_len	= sizeof(fwbroadcastaddr);
	 
    return true;
}

//---------------------------------------------------------------------------
// Free the IOFWInterface instance. 
int  firewire_del_if(IOFWInterface	*fwIf);

void IOFWInterface::free()
{
	struct ifnet *interface = (struct ifnet*)getIfnet();
    if ( interface )
    {
        DLOG("%s%d: release ifnet %p\n", getNamePrefix(), getUnitNumber(), interface);
		firewire_del_if( this );
    }

    if ( _requiredFilters )
    {
        _requiredFilters->release();
        _requiredFilters = 0;
    }

    if ( _activeFilters )
    {
        _activeFilters->release();
        _activeFilters = 0;
    }
    
    if ( _supportedFilters )
    {
        _supportedFilters->release();
        _supportedFilters = 0; 
    }

    super::free();
}

//---------------------------------------------------------------------------
// The name of the interface advertised to the network layer
// is generated by concatenating the string returned by this method,
// and an unit number.
//
// Returns a pointer to a constant string "fw". Thus FireWire interfaces
// will be registered as fw0, fw1, etc.

const char * IOFWInterface::getNamePrefix() const
{
    return kIOFWInterfaceNamePrefix;
}

//---------------------------------------------------------------------------
// Prepare the 'FireWire' controller after it has been opened. This is called
// by IONetworkInterface after a controller has accepted an open from this 
// interface. IOFWInterface uses this method to inspect the controller,
// and to cache certain controller properties, such as its hardware address.
// This method is called with the arbitration lock held.
//
// controller: The controller object that was opened.
//
// Returns true on success, false otherwise
// (which will cause the controller to be closed).

bool IOFWInterface::controllerDidOpen(IONetworkController * ctr)
{
    bool                 ret = false;
    OSData *             addrData;

    do {
        // Call the controllerDidOpen() in superclass first.

        if ( (ctr == 0) || (super::controllerDidOpen(ctr) == false) )
             break;

        // If the controller supports some form of multicast filtering,
        // then set the ifnet IFF_MULTICAST flag.

        if ( GET_SUPPORTED_FILTERS(gIONetworkFilterGroup) &
             (kIOPacketFilterMulticast | kIOPacketFilterMulticastAll) )
        {
            setFlags(IFF_MULTICAST);
        }

        // Advertise Wake on Magic Packet feature if supported.
        
        if ( GET_SUPPORTED_FILTERS( gIOEthernetWakeOnLANFilterGroup ) &
             kIOFWWakeOnMagicPacket )
        {
            IOPMrootDomain * root = getPMRootDomain();
            if ( root ) root->publishFeature( "WakeOnMagicPacket" );
        }

        // Get the controller's MAC/FireWire address.

        addrData = OSDynamicCast(OSData, ctr->getProperty(kIOMACAddress));
        if ((addrData == 0) || (addrData->getLength() != kIOFWAddressSize))
        {
            DLOG("%s: kIOMACAddress property access error (len %d)\n",
                 getName(), addrData ? addrData->getLength() : 0);
            break;
        }

        ret = true;
    }
    while (0);

    return ret;
}

//---------------------------------------------------------------------------
// When a close from our last client is received, the interface will
// close the controller. But before the controller is closed, this method
// will be called by our superclass to perform any final cleanup. This
// method is called with the arbitration lock held.
//
// IOFWInterface will ensure that the controller is disabled.
//
// controller: The currently opened controller object.

void IOFWInterface::controllerWillClose(IONetworkController * ctr)
{
    super::controllerWillClose(ctr);
}

//---------------------------------------------------------------------------
// Handle ioctl commands originated from the network layer.
// Commands not handled by this method are passed to our superclass.
//
// Argument convention is:
//
//    arg0 - (ifnet_t  )
//    arg1 - (void *)
//
// The commands handled by IOFWInterface are:
//
//    SIOCSIFADDR
//    SIOCSIFFLAGS
//    SIOCADDMULTI
//    SIOCDELMULTI
//
// Returns an error code defined in errno.h (BSD).
SInt32 IOFWInterface::performCommand( IONetworkController *		  ctr,
                                            unsigned long         cmd,
                                            void *                arg0,
                                            void *                arg1 )
{
    SInt32  ret;

    if ( ctr == 0 ) return EINVAL;
	
	switch ( cmd )
	{
		case SIOCSIFFLAGS:
		case SIOCADDMULTI:
		case SIOCDELMULTI:
		case SIOCSIFADDR:
		case SIOCSIFMTU:
		case SIOCSIFLLADDR:
			ret = (int) ctr->executeCommand(
							this,            /* client */
							(IONetworkController::Action)
								&IOFWInterface::performGatedCommand,
							this,            /* target */
							ctr,             /* param0 */
							(void *) cmd,    /* param1 */
							arg0,            /* param2 */
							arg1 );          /* param3 */
			break;

		default:
			// Unknown command, let our superclass deal with it.
			ret = super::performCommand(ctr, cmd, arg0, arg1);
			break;
	}
	
    return ret;
}

//---------------------------------------------------------------------------
// Handle an ioctl command on the controller's workloop context.

int IOFWInterface::performGatedCommand(void * target,
                                             void * arg1_ctr,
                                             void * arg2_cmd,
                                             void * arg3_0,
                                             void * arg4_1)
{
    IOFWInterface		*self = (IOFWInterface *) target;
    IONetworkController *ctr  = (IONetworkController *) arg1_ctr;
    struct ifreq 		*ifr  = (struct ifreq*) arg4_1;
    SInt                 ret  = EOPNOTSUPP;
	
    // Refuse to perform controller I/O if the controller is in a
    // low-power state that makes it "unusable".

    if ( self->_controllerLostPower ||
        ( self->getInterfaceState() & kIONetworkInterfaceDisabledState ) )
         return EPWROFF;

    switch ( (UInt64) arg2_cmd )
    {
        case SIOCSIFADDR:
            ret = self->syncSIOCSIFADDR(ctr);
            break;

        case SIOCSIFFLAGS:
            ret = self->syncSIOCSIFFLAGS(ctr);
            break;

        case SIOCADDMULTI:
            ret = self->syncSIOCADDMULTI(ctr);
            break;

        case SIOCDELMULTI:
            ret = self->syncSIOCDELMULTI(ctr);
            break;

        case SIOCSIFMTU:
            ret = self->syncSIOCSIFMTU( ctr, ifr );
            break;

        case SIOCSIFLLADDR:
            ret = self->syncSIOCSIFLLADDR( ctr, ifr->ifr_addr.sa_data,
                                           ifr->ifr_addr.sa_len );
            break;
		
		case SIOCGIFADDR:	/* get ifnet address */
			struct sockaddr *sa = (struct sockaddr *) & ifr->ifr_data;
			ret = self->syncSIOCGIFADDR( ctr, sa->sa_data, FIREWIRE_ADDR_LEN );
			break;
    }

    return ret;
}

//---------------------------------------------------------------------------
// enableController() is reponsible for calling the controller's enable()
// method and restoring the state of the controller. We assume that
// controllers can completely reset its state upon receiving a disable()
// method call. And when it is brought back up, the interface should
// assist in restoring the previous state of the FireWire controller.

IOReturn IOFWInterface::enableController(IONetworkController * ctr)
{
    IOReturn   ret     = kIOReturnSuccess;
    bool       enabled = false;

    assert(ctr);

    do {
        // Is controller already enabled? If so, exit and return success.

        if ( _ctrEnabled )
            break;

        // Send the controller an enable command.
   
        if ( (ret = ctr->doEnable(this)) != kIOReturnSuccess )
            break;     // unable to enable the controller.

        enabled = true;

        // Disable all Wake-On-LAN filters.

        disableFilter(ctr, gIOEthernetWakeOnLANFilterGroup, (UInt32)~0, kFilterOptionNoStateChange);

        // Restore current filter selection.

        SET_ACTIVE_FILTERS(gIONetworkFilterGroup, 0);
        ret = enableFilter(ctr, gIONetworkFilterGroup, 0,
                           kFilterOptionSyncPendingIO);
        if ( ret != kIOReturnSuccess ) break;

        // Restore multicast filter settings.

        syncSIOCADDMULTI(ctr);

        // Re-apply the user supplied link-level address.

        OSData * lladdr = OSDynamicCast(OSData, getProperty(kIOMACAddress));
        if ( lladdr && lladdr->getLength() == kIOFWAddressSize )
        {
            ctr->setHardwareAddress( lladdr->getBytesNoCopy(),
                                     lladdr->getLength() );
        }

        _ctrEnabled = true;

    } while (false);

    // Disable the controller if a serious error has occurred after the
    // controller has been enabled.

    if ( enabled && (ret != kIOReturnSuccess) )
    {
        ctr->doDisable(this);
    }

    return ret;
}

//---------------------------------------------------------------------------
// Handles SIOCSIFFLAGS ioctl command for FireWire interfaces. The network
// stack has changed the if_flags field in ifnet. Our job is to go
// through if_flags and see what has changed, and act accordingly.
//
// The fact that if_flags contains both generic and FireWire specific bits
// means that we cannot move some of the default flag processing to the
// superclass.

int IOFWInterface::syncSIOCSIFFLAGS(IONetworkController * ctr)
{
    UInt16    flags = getFlags();
    IOReturn  ret   = kIOReturnSuccess;

    if ( ( ((flags & IFF_UP) == 0) || _controllerLostPower ) &&
         ( flags & IFF_RUNNING ) )
    {
        // If interface is marked down and it is currently running,
        // then stop it.

        ctr->doDisable(this);
        flags &= ~IFF_RUNNING;
        _ctrEnabled = false;
    }
    else if ( ( flags & IFF_UP )                &&
              ( _controllerLostPower == false ) &&
              ((flags & IFF_RUNNING) == 0) )
    {
        // If interface is marked up and it is currently stopped,
        // then start it.

        if ( (ret = enableController(ctr)) == kIOReturnSuccess )
            flags |= IFF_RUNNING;
    }

    if ( flags & IFF_RUNNING )
    {
        IOReturn rc;

        // We don't expect multiple flags to be changed for a given
        // SIOCSIFFLAGS call.

        // Promiscuous mode

        rc = (flags & IFF_PROMISC) ?
             enableFilter(ctr, gIONetworkFilterGroup,
                          kIOPacketFilterPromiscuous) :
             disableFilter(ctr, gIONetworkFilterGroup,
                           kIOPacketFilterPromiscuous);

        if (ret == kIOReturnSuccess) ret = rc;

        // Multicast-All mode

        rc = (flags & IFF_ALLMULTI) ?
             enableFilter(ctr, gIONetworkFilterGroup,
                          kIOPacketFilterMulticastAll) :
             disableFilter(ctr, gIONetworkFilterGroup,
                           kIOPacketFilterMulticastAll);

        if (ret == kIOReturnSuccess) ret = rc;
    }

    // Update the flags field to pick up any modifications. Also update the
    // property table to reflect any flag changes.

    setFlags(flags, ~flags);

    return errnoFromReturn(ret);
}

//---------------------------------------------------------------------------
// Handles SIOCSIFADDR ioctl.

SInt IOFWInterface::syncSIOCSIFADDR(IONetworkController * ctr)
{
	ifnet_t		ifp		= getIfnet();
    IOReturn	ret		= kIOReturnSuccess;
	char		lladdr[kIOFWAddressSize];
	
	if(ifp == NULL)
		return (EINVAL);

	IOFWAddress *addr = (IOFWAddress *) _uniqueID->getBytesNoCopy();

	bcopy(addr->bytes, lladdr, kIOFWAddressSize);

	if(ifnet_set_lladdr(ifp, lladdr, kIOFWAddressSize) != 0)
		DLOG("ifnet_set_lladdr failure in %s:%d", __FILE__, __LINE__);

    // Interface is implicitly brought up by an SIOCSIFADDR ioctl.
    setFlags(IFF_UP);

    if ( (getFlags() & IFF_RUNNING) == 0 )
    {
        if ( (ret = enableController(ctr)) == kIOReturnSuccess )
		{
            setFlags(IFF_RUNNING);
			
			ifaddr_t *addresses;
			
		   /*
			* Also send gratuitous ARPs to notify other nodes about
			* the address change.
			*/
			if (ifnet_get_address_list_family(ifp, &addresses, AF_INET) == 0) 
			{
				int i;
				
				for (i = 0; addresses[i] != NULL; i++) 
					inet_arp_init_ifaddr(ifp, addresses[i]);
				
				ifnet_free_address_list(addresses);
			}			
		}
    }
	
    return errnoFromReturn(ret);
}

//---------------------------------------------------------------------------
// Handle SIOCADDMULTI ioctl command.

SInt IOFWInterface::syncSIOCADDMULTI(IONetworkController * ctr)
{
    IOReturn ret;

    // Make sure multicast filter is active.

    ret = enableFilter(ctr, gIONetworkFilterGroup, kIOPacketFilterMulticast);

    if ( ret == kIOReturnSuccess )
    {
        // Load multicast addresses only if the filter was activated.

        ret = setupMulticastFilter(ctr);

        // If the list is now empty, then deactivate the multicast filter.

        if ( _mcAddrCount == 0 )
        {
            IOReturn dret = disableFilter(ctr, gIONetworkFilterGroup,
                                          kIOPacketFilterMulticast);

            if (ret == kIOReturnSuccess) ret = dret;
        }
    }

    return errnoFromReturn(ret);
}

//---------------------------------------------------------------------------
// Handle SIOCDELMULTI ioctl command.

SInt IOFWInterface::syncSIOCDELMULTI(IONetworkController * ctr)
{
    return syncSIOCADDMULTI(ctr);
}

//---------------------------------------------------------------------------
// Handle SIOCSIFMTU ioctl.

int IOFWInterface::syncSIOCSIFMTU( IONetworkController * ctr,
                                         struct ifreq *        ifr )
{
#define MTU_TO_FRAMESIZE(x) \
        ((x) + kIOFWCRCSize + sizeof(struct firewire_header))

    SInt32  error = 0;
    UInt32  size;
    UInt32  maxSize = kIOFWMaxPacketSize;  // 1518
    UInt32  ifrSize = MTU_TO_FRAMESIZE( ifr->ifr_mtu );
    UInt32  ctrSize = MTU_TO_FRAMESIZE( getMaxTransferUnit() );

    // If change is not necessary, return success without getting the
    // controller involved.

    if ( ctrSize == ifrSize )
        return 0;  // no change required

    if ( ctr->getMaxPacketSize( &size ) == kIOReturnSuccess )
        maxSize = max( size, kIOFWMaxPacketSize );

    if ( ifrSize > maxSize )
        return EINVAL;	// MTU is too large for the controller.

    // Message the controller if the new MTU requires a non standard
    // frame size, or if the controller is currently programmed to
    // support an extended frame size which is no longer required.

    if ( max( ifrSize, ctrSize ) > kIOFWMaxPacketSize )
    {
        IOReturn ret;
        ret = ctr->setMaxPacketSize( max(ifrSize, kIOFWMaxPacketSize) );
        error = errnoFromReturn( ret );
    }

	if ( error == 0 )
    {
        // Success, update the MTU in ifnet.
        setMaxTransferUnit( ifr->ifr_mtu );
    }

    return error;
}

//---------------------------------------------------------------------------

int IOFWInterface::syncSIOCGIFADDR( IONetworkController * ctr, char * lladdr, int len )
{
	if( len != kIOFWAddressSize || lladdr == NULL )
		return EINVAL;
	
    if ( _ctrEnabled != true )    /* reject if interface is down */
        return (ENETDOWN);
	
	ifnet_lladdr_copy_bytes(getIfnet(), lladdr, len);
	
	return 0;
}

int IOFWInterface::syncSIOCSIFLLADDR( IONetworkController * ctr,
                                            const char * lladdr, int len )
{
	unsigned char tempaddr[kIOFWAddressSize];
	OSData *hardAddr;
	
	if(len != kIOFWAddressSize)
		return EINVAL;
	
    if (_ctrEnabled != true)    /* reject if interface is down */
        return (ENETDOWN);
		
	// keep a backup in case stack refuses our change
	hardAddr = OSDynamicCast(OSData, getProperty(kIOMACAddress));
	if(hardAddr && hardAddr->getLength() == kIOFWAddressSize)
		bcopy(hardAddr->getBytesNoCopy(), tempaddr, kIOFWAddressSize);
	
	// change the hardware- we do it before the stack, in case the stack
	// needs to generate traffic as a result.
	
    if ( ctr->setHardwareAddress( lladdr, len ) == kIOReturnSuccess )
    {
		if( ifnet_set_lladdr(getIfnet(), lladdr, len) ) //uh-oh, stack didn't like this
		{
			// restore previous address
			if(hardAddr)
			   ctr->setHardwareAddress(tempaddr, sizeof(tempaddr));
			return EINVAL;
		}

        setProperty(kIOMACAddress, (void *)lladdr, len);

        DLOG("%s: SIOCSIFLLADDR %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
              ctr->getName(),
              lladdr[0], lladdr[1], lladdr[2], lladdr[3], 
			  lladdr[4], lladdr[5], lladdr[6], lladdr[7]);
    }

    return 0;
}

//---------------------------------------------------------------------------
// Enable a packet filter.

#define getOneFilter(x)   ((x) & (~((x) - 1)))

IOReturn
IOFWInterface::enableFilter(IONetworkController * ctr,
                                  const OSSymbol *      group,
                                  UInt32                filters,
                                  IOOptionBits          options)
{
    IOReturn ret = kIOReturnSuccess;

    if ( options & kFilterOptionNotInsideGate )
    {
        options &= ~kFilterOptionNotInsideGate;

    	return ctr->executeCommand(
                           this,               /* client */
                           OSMemberFunctionCast(IONetworkController::Action, this, &IOFWInterface::enableFilter),
                           this,               /* target */
                           (void *) ctr,       /* param0 */
                           (void *) group,     /* param1 */
                           (void *) filters,   /* param2 */
                           (void *) options ); /* param3 */
    }

	if ( options & kFilterOptionDisableZeroBits )
    {
        ret = disableFilter(ctr, group, ~filters, options);
        if ( ret != kIOReturnSuccess) return ret;
    }

    // If the controller does not support the packet filter,
    // there's no need to proceed.

    if (( GET_SUPPORTED_FILTERS(group) & filters ) != filters)
        return kIOReturnUnsupported;

    do {
        // Add new filter to the set of required filters.

        UInt32 reqFilters = GET_REQUIRED_FILTERS(group) | filters;
        UInt32 actFilters = GET_ACTIVE_FILTERS(group);
        UInt32 resFilters = ( actFilters ^ reqFilters );
        
        if ( (options & kFilterOptionSyncPendingIO) == 0 )
        {
            // Restrict filter update by using 'filters' as a mask.
            resFilters &= filters;
        }

        while ( resFilters && ((options & kFilterOptionDeferIO) == 0) )
        {
            UInt32 oneFilter = getOneFilter(resFilters);

            // Send a command to the controller driver.

            ret = ctr->enablePacketFilter(group, oneFilter,
                                          actFilters, options);
            if ( ret != kIOReturnSuccess ) break;

            resFilters &= ~oneFilter;
            actFilters |= oneFilter;
        }

        if ( (options & kFilterOptionNoStateChange) == 0 )
            SET_REQUIRED_FILTERS(group, reqFilters);
        SET_ACTIVE_FILTERS(group, actFilters);
    }
    while (false);

    return ret;
}

//---------------------------------------------------------------------------
// Disable a packet filter.

IOReturn
IOFWInterface::disableFilter(IONetworkController * ctr,
                                   const OSSymbol *      group,
                                   UInt32                filters,
                                   IOOptionBits          options)
{
    IOReturn ret = kIOReturnSuccess;

    do {
        // Remove specified filter from the set of required filters.

        UInt32 reqFilters = GET_REQUIRED_FILTERS(group) & ~filters;
        UInt32 actFilters = GET_ACTIVE_FILTERS(group);
        UInt32 resFilters = ( actFilters ^ reqFilters ) & filters;

        while ( resFilters && ((options & kFilterOptionDeferIO) == 0) )
        {
            UInt32 oneFilter = getOneFilter(resFilters);

            // Send a command to the controller driver.

            ret = ctr->disablePacketFilter(group, oneFilter,
                                           actFilters, options);
            if ( ret != kIOReturnSuccess ) break;

            resFilters &= ~oneFilter;
            actFilters &= ~oneFilter;
        }

        if ( (options & kFilterOptionNoStateChange) == 0 )
            SET_REQUIRED_FILTERS(group, reqFilters);
        SET_ACTIVE_FILTERS(group, actFilters);
    }
    while (false);

    return ret;
}

//---------------------------------------------------------------------------
// Cache the list of multicast addresses and send a command to the
// controller to update the multicast list.

IOReturn
IOFWInterface::setupMulticastFilter(IONetworkController * ctr)
{
    void *               multiAddrs = 0;
    UInt                 mcount;
    OSData *             mcData = 0;
    ifnet_t  			 interface;
    struct sockaddr		 dlAddress;
	IOReturn             ret = kIOReturnSuccess;
    bool                 ok;

	ifmultiaddr_t		*addressList;
	
    interface = getIfnet();
	
	assert(interface);

    // get the list and count how many mcast link addresses there are
	if(ifnet_get_multicast_list(interface, &addressList))
		return kIOReturnNoMemory;
		
	mcount = 0;
	for(int i=0; addressList[i]; i++)
	{
		ifmaddr_address(addressList[i], &dlAddress, sizeof(dlAddress));

		if (dlAddress.sa_family == AF_UNSPEC || dlAddress.sa_family == AF_LINK)
			mcount++;
	}
	
	_mcAddrCount = mcount;

	// now rewalk the list and copy the addresses to a format suitable to give to the controller
    if ( mcount )
    {
        char * addrp;
            
        mcData = OSData::withCapacity(mcount * kIOFWAddressSize);
        if (!mcData)
        {
            DLOG("%s: no memory for multicast address list\n", getName());
            return kIOReturnNoMemory;
        }

        // Loop through the list and copy the link multicast
        // address to the OSData.

		for(int i = 0; addressList[i]; i++)
		{
			//retrieve the datalink mcast address
			ifmaddr_address(addressList[i], &dlAddress, sizeof(dlAddress));
			
			if (dlAddress.sa_family == AF_UNSPEC)
                addrp = &dlAddress.sa_data[0];
            else if (dlAddress.sa_family == AF_LINK)
                addrp = LLADDR((struct sockaddr_dl *)&dlAddress);
            else
                continue;

            ok = mcData->appendBytes((const void *) addrp, kIOFWAddressSize);
            assert(ok);
        }

        multiAddrs = (void *) mcData->getBytesNoCopy();
        assert(multiAddrs);
    }

    // Issue a controller command to setup the multicast filter.
	ret = ((IOFWController *)ctr)->setMulticastList((IOFWAddress *) multiAddrs,
													mcount);
    if (mcData)
    {
        if (ret == kIOReturnSuccess)
            setProperty(kIOMulticastAddressList, mcData);

        mcData->release();
    }
    else 
	{
        removeProperty(kIOMulticastAddressList);
    }
	
	ifnet_free_multicast_list(addressList);

    return ret;
}

//---------------------------------------------------------------------------
// Power management support.
//
// Handlers called, with the controller's gate closed, in response to a
// controller power state change.

IOReturn
IOFWInterface::controllerWillChangePowerState(
                               IONetworkController * ctr,
                               IOPMPowerFlags        flags,
                               UInt32                stateNumber,
                               IOService *           policyMaker )
{
	if ( ( (flags & IOPMDeviceUsable ) == 0) && 
         ( _controllerLostPower == false )   &&
         _ctrEnabled )
    {
        unsigned long filters;

        _controllerLostPower = true;

        // Get the "aggressiveness" factor from the policy maker.

        ctr->getAggressiveness( kPMEthernetWakeOnLANSettings, &filters );

        filters &= GET_SUPPORTED_FILTERS( gIOEthernetWakeOnLANFilterGroup );

        // Is the link up? If it is, leave the WOL filters intact,
        // otherwise mask out the WOL filters that would not function
        // without a proper link. This will reduce power consumption
        // for cases when a machine is put to sleep and there is no
        // network connection.

        OSNumber * linkStatusNumber = (OSNumber *) 
                                      ctr->getProperty( kIOLinkStatus );

        if ( ( linkStatusNumber == 0 ) ||
             ( ( linkStatusNumber->unsigned32BitValue() &
                 (kIONetworkLinkValid | kIONetworkLinkActive) ) ==
                  kIONetworkLinkValid ) )
        {
            filters &= ~( kIOFWWakeOnMagicPacket |
                          kIOFWWakeOnPacketAddressMatch );
        }

        // Before controller is disabled, program its wake up filters.
        // The WOL setting is formed by a bitwise OR between the WOL filter
        // settings, and the aggresiveness factor from the policy maker.

        enableFilter( ctr,
                      gIOEthernetWakeOnLANFilterGroup,
                      filters,
                      kFilterOptionNoStateChange |
                      kFilterOptionSyncPendingIO );

        // Call SIOCSIFFLAGS handler to disable the controller,
        // and mark the interface as Not Running.

        syncSIOCSIFFLAGS(ctr);
    }
    
    return super::controllerWillChangePowerState( ctr, flags,
                                                  stateNumber,
                                                  policyMaker );
}

IOReturn
IOFWInterface::controllerDidChangePowerState(
                               IONetworkController * ctr,
                               IOPMPowerFlags        flags,
                               UInt32                stateNumber,
                               IOService *           policyMaker )
{
    IOReturn ret = super::controllerDidChangePowerState( ctr, flags,
                                                         stateNumber,
                                                         policyMaker );

    if ( ( flags & IOPMDeviceUsable ) && ( _controllerLostPower == true ) )
    {
        _controllerLostPower = false;

        // Clear _controllerLostPower, then call the SIOCSIFFLAGS handler to
        // perhaps enable the controller, restore all FireWire controller
        // state, then mark the interface as Running.

        syncSIOCSIFFLAGS(ctr);
    }

    return ret;
}

#define kIONetworkInterfaceProperties   "IONetworkInterfaceProperties"

//---------------------------------------------------------------------------
// Handle a request to set properties from kernel or non-kernel clients.
// For non-kernel clients, the preferred mechanism is through an user
// client.

IOReturn IOFWInterface::setProperties( OSObject * properties )
{
    IOReturn       ret;
    OSDictionary * dict = (OSDictionary *) properties;
    OSNumber *     num;

    // Call IONetworkInterface::setProperties() first.

    ret = super::setProperties(properties);

    if ( (ret == kIOReturnSuccess) || (ret == kIOReturnUnsupported) )
    {
        dict = OSDynamicCast( OSDictionary,
                 dict->getObject(kIONetworkInterfaceProperties) );
        if ( dict )
        {
            dict = OSDynamicCast( OSDictionary,
                     dict->getObject(kIORequiredPacketFilters) );
            if ( dict )
            {
                num = OSDynamicCast( OSNumber,
                        dict->getObject(kIOFWWakeOnLANFilterGroup) );

                if ( num )
                {
                    ret = enableFilter( getController(),
                                        gIOEthernetWakeOnLANFilterGroup,
                                        num->unsigned32BitValue(),
                                        kFilterOptionDeferIO       |
                                        kFilterOptionNotInsideGate |
                                        kFilterOptionDisableZeroBits );
                }
            }
        }
    }

    return ret;
}

//---------------------------------------------------------------------------
// willTerminate

bool IOFWInterface::willTerminate( IOService *  provider,
                                         IOOptionBits options )
{
    bool ret = super::willTerminate( provider, options );

    // We assume that willTerminate() is always called from the
    // provider's work loop context.

    // Once the gated ioctl path has been blocked, disable the controller.
    // The hardware may have already been removed from the system.

    if ( _ctrEnabled && getController() )
    {
        DLOG("IOFWInterface::willTerminate disabling controller\n");
        getController()->doDisable( this );
        _ctrEnabled = false;
    }

    return ret;
}

//---------------------------------------------------------------------------
int firewire_init_if(ifnet_t ifp);

IOReturn IOFWInterface::attachToDataLinkLayer(	IOOptionBits options,
												void *       parameter )
{
	IOReturn	ret = 0;
	unsigned char lladdr[kIOFWAddressSize];

	ret = super::attachToDataLinkLayer( options, parameter );
    if (ret == kIOReturnSuccess)
    {
		ifnet_set_baudrate(getIfnet(), 10000000); 
		
        bpfattach(getIfnet(), DLT_APPLE_IP_OVER_IEEE1394, sizeof(struct firewire_header) );

		IOFWAddress *addr = (IOFWAddress *) _uniqueID->getBytesNoCopy();
		bcopy(addr->bytes, lladdr, kIOFWAddressSize);

		if(ifnet_set_lladdr(getIfnet(), lladdr, kIOFWAddressSize) != 0)
			DLOG("ifnet_set_lladdr failure in %s:%d", __FILE__, __LINE__);

		if (ivedonethis)
		{	
			ivedonethis++;
			return kIOReturnSuccess;
		}

		ivedonethis++;
					
		// IPv4 proto register
		ret = proto_register_plumber(PF_INET, APPLE_IF_FAM_FIREWIRE,
									firewire_attach_inet, NULL);
		if(ret == EEXIST || ret == 0)
			ret = kIOReturnSuccess;
		else
			DLOG("ERROR: dlil_reg_proto_module for IPv4 over FIREWIRE %x", ret);

		// IPv6 proto register
		ret = proto_register_plumber(PF_INET6, APPLE_IF_FAM_FIREWIRE,
									firewire_attach_inet6, NULL);
		if(ret == EEXIST || ret == 0)
			ret = kIOReturnSuccess;
		else
			DLOG("ERROR: dlil_reg_proto_module for IPv6 over FIREWIRE %x", ret);
	}

    return ret;
}

void IOFWInterface::detachFromDataLinkLayer( IOOptionBits options,
                                                  void *       parameter )
{
	if (ivedonethis == 1)
	{		
		proto_unregister_plumber(PF_INET, APPLE_IF_FAM_FIREWIRE);
		proto_unregister_plumber(PF_INET6, APPLE_IF_FAM_FIREWIRE);
	}

	ivedonethis--;

	super::detachFromDataLinkLayer(options, parameter);
}

void IOFWInterface::setIfnetMTU(UInt32 mtu)
{
    setMaxTransferUnit(mtu-FIREWIRE_HDR_LEN);
}

void IOFWInterface::setFamilyCookie(void *data)
{
	_familyCookie = data;
}
