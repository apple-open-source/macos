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
 * IOEthernetInterface.cpp
 *
 * HISTORY
 * 8-Jan-1999       Joe Liu (jliu) created.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSData.h>
#include <IOEthernetInterface.h>
#include <IOEthernetController.h>
#include "IONetworkUserClient.h"
#include <IOKit/pwr_mgt/RootDomain.h>	// publishFeature()

extern "C" {
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_ether.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/dlil.h>
#include <net/bpf.h>
#include <netinet/if_ether.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
}

//---------------------------------------------------------------------------

#define super IONetworkInterface

OSDefineMetaClassAndStructors( IOEthernetInterface, IONetworkInterface )
OSMetaClassDefineReservedUnused( IOEthernetInterface,  0);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  1);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  2);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  3);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  4);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  5);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  6);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  7);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  8);
OSMetaClassDefineReservedUnused( IOEthernetInterface,  9);
OSMetaClassDefineReservedUnused( IOEthernetInterface, 10);
OSMetaClassDefineReservedUnused( IOEthernetInterface, 11);
OSMetaClassDefineReservedUnused( IOEthernetInterface, 12);
OSMetaClassDefineReservedUnused( IOEthernetInterface, 13);
OSMetaClassDefineReservedUnused( IOEthernetInterface, 14);
OSMetaClassDefineReservedUnused( IOEthernetInterface, 15);

// The name prefix for all BSD Ethernet interfaces.
// 
#define kIOEthernetInterfaceNamePrefix      "en"

// Options used for enableFilter(), disableFilter().
enum {
    kFilterOptionDeferIO          = 0x0001,
    kFilterOptionNotInsideGate    = 0x0002,
    kFilterOptionNoStateChange    = 0x0004,
    kFilterOptionDisableZeroBits  = 0x0008,
    kFilterOptionSyncPendingIO    = 0x0010
};

//---------------------------------------------------------------------------
// Macros

#define _altMTU                 _reserved->altMTU
#define _publishedFeatureID     _reserved->publishedFeatureID
#define _supportedWakeFilters   _reserved->supportedWakeFilters
#define _disabledWakeFilters    _reserved->disabledWakeFilters

#define kWOMPFeatureKey         "WakeOnMagicPacket"

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

UInt32
IOEthernetInterface::getFilters(
    const OSDictionary * dict,
    const OSSymbol *     group )
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

bool
IOEthernetInterface::setFilters(
    OSDictionary *   dict,
    const OSSymbol * group,
    UInt32           filters )
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
// Initialize an IOEthernetInterface instance. Instance variables are
// initialized

bool IOEthernetInterface::init(IONetworkController * controller)
{
    OSObject *  obj;

    _reserved = IONew( ExpansionData, 1 );
	if( _reserved == 0 )
		return false;
	memset(_reserved, 0, sizeof(ExpansionData));
	
    if ( super::init(controller) == false )
    	return false;

	// initialize enet specific fields.
	setInterfaceType(IFT_ETHER);
	setMaxTransferUnit( ETHERMTU );
	setMediaAddressLength( ETHER_ADDR_LEN );
	setMediaHeaderLength( ETHER_HDR_LEN );
	setFlags( IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS,
			  IFF_RUNNING   | IFF_MULTICAST );
	
    // Add an IONetworkData with room to hold an IOEthernetStats structure.
    // This class does not reference the data object created, and no harm
    // is done if the data object is released or replaced.

    IONetworkData * data = IONetworkData::withInternalBuffer(
                                              kIOEthernetStatsKey,
                                              sizeof(IOEthernetStats));
    if (data)
    {
        addNetworkData(data);
        data->release();
    }

    _inputEventThreadCall = thread_call_allocate(
                            handleEthernetInputEvent, this );
    if (!_inputEventThreadCall)
        return false;

    // Create and initialize the filter dictionaries.

    _requiredFilters = OSDictionary::withCapacity(2);
    _activeFilters   = OSDictionary::withCapacity(2);

    if ( (_requiredFilters == 0) || (_activeFilters == 0) )
        return false;

    obj = controller->copyProperty(kIOPacketFilters);
    if (obj && ((_supportedFilters = OSDynamicCast(OSDictionary, obj)) == 0))
        obj->release();
    if (!_supportedFilters)
        return false;

    // Cache the bit mask of wake filters supported by the driver.
    // This value will not change.

    _supportedWakeFilters = GET_SUPPORTED_FILTERS(
                            gIOEthernetWakeOnLANFilterGroup );

    // Retain the Disabled WOL filters OSNumber.
    // Its value will be updated live for link and WOL changed events.

    obj = _supportedFilters->getObject(
            gIOEthernetDisabledWakeOnLANFilterGroup );
    _disabledWakeFilters = OSDynamicCast(OSNumber, obj);
    if (_disabledWakeFilters)
        _disabledWakeFilters->retain();

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

    _publishedFeatureID = 0;

    // Publish filter dictionaries to property table.

    setProperty( kIORequiredPacketFilters, _requiredFilters );
    setProperty( kIOActivePacketFilters,   _activeFilters );

    return true;
}

static const u_char ether_broadcast_addr[ETHER_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

bool IOEthernetInterface::initIfnetParams(struct ifnet_init_params *params)
{
	OSData *uniqueID;
	//get the default values
    super::initIfnetParams( params );
		
    uniqueID = OSDynamicCast(OSData, getProvider()->getProperty(kIOMACAddress));
    if ( (uniqueID == 0) || (uniqueID->getLength() != ETHER_ADDR_LEN) )
    {
        DLOG("%s: kIOMACAddress property access error (len %d)\n",
             getName(), uniqueID ? uniqueID->getLength() : 0);
        return false;
    }
	
	// fill in ethernet specific values
	params->uniqueid = uniqueID->getBytesNoCopy();
	params->uniqueid_len = uniqueID->getLength();
	params->family = APPLE_IF_FAM_ETHERNET;
	params->demux = ether_demux;
	params->add_proto = ether_add_proto;
	params->del_proto = ether_del_proto;
	params->framer = ether_frameout;
	params->check_multi = ether_check_multi;
	params->broadcast_addr = ether_broadcast_addr;
	params->broadcast_len = sizeof(ether_broadcast_addr);
    return true;
}

//---------------------------------------------------------------------------
// Free the IOEthernetInterface instance. 

void IOEthernetInterface::free()
{
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

    if ( _inputEventThreadCall )
    {
        thread_call_free( _inputEventThreadCall );
        _inputEventThreadCall = 0;
    }

	if ( _reserved )
	{
        if (_disabledWakeFilters)
        {
            _disabledWakeFilters->release();
            _disabledWakeFilters = 0;
        }
		IODelete( _reserved, ExpansionData, 1 );
        _reserved = 0;
    }
	
    super::free();
}

//---------------------------------------------------------------------------
// The name of the interface advertised to the network layer
// is generated by concatenating the string returned by this method,
// and an unit number.
//
// Returns a pointer to a constant string "en". Thus Ethernet interfaces
// will be registered as en0, en1, etc.

const char * IOEthernetInterface::getNamePrefix() const
{
    return kIOEthernetInterfaceNamePrefix;
}

//---------------------------------------------------------------------------
// Prepare the 'Ethernet' controller after it has been opened. This is called
// by IONetworkInterface after a controller has accepted an open from this 
// interface. IOEthernetInterface uses this method to inspect the controller,
// and to cache certain controller properties, such as its hardware address.
// This method is called with the arbitration lock held.
//
// controller: The controller object that was opened.
//
// Returns true on success, false otherwise
// (which will cause the controller to be closed).

bool IOEthernetInterface::controllerDidOpen(IONetworkController * ctr)
{
    bool                 ret = false;
    OSData *             addrData;
    IOEthernetAddress *  addr;

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
        
        if ( _supportedWakeFilters & kIOEthernetWakeOnMagicPacket )
        {
            IOPMrootDomain * root = getPMRootDomain();
            if ( root ) root->publishFeature( kWOMPFeatureKey,
                             kIOPMSupportedOnAC | kIOPMSupportedOnUPS, 
                             (uint32_t *)&_publishedFeatureID);
        }

        // Get the controller's MAC/Ethernet address.

        addrData = OSDynamicCast(OSData, ctr->getProperty(kIOMACAddress));
        if ( (addrData == 0) || (addrData->getLength() != ETHER_ADDR_LEN) )
        {
            DLOG("%s: kIOMACAddress property access error (len %d)\n",
                 getName(), addrData ? addrData->getLength() : 0);
            break;
        }

        addr = (IOEthernetAddress *) addrData->getBytesNoCopy();

#if 1   // Print the address
        IOLog("%s: Ethernet address %02x:%02x:%02x:%02x:%02x:%02x\n",
              ctr->getName(),
              addr->bytes[0],
              addr->bytes[1],
              addr->bytes[2],
              addr->bytes[3],
              addr->bytes[4],
              addr->bytes[5]);
#endif
        
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
// IOEthernetInterface will ensure that the controller is disabled.
//
// controller: The currently opened controller object.

void IOEthernetInterface::controllerWillClose(IONetworkController * ctr)
{
    if ( _supportedWakeFilters & kIOEthernetWakeOnMagicPacket )
    {
        IOPMrootDomain * root = getPMRootDomain();
        if ( root ) root->removePublishedFeature( _publishedFeatureID );
        _publishedFeatureID = 0;
    }
    
    super::controllerWillClose(ctr);
}

//---------------------------------------------------------------------------
// Handle ioctl commands originated from the network layer.
// Commands not handled by this method are passed to our superclass.
//
// Argument convention is:
//
//    arg0 - (struct ifnet *)
//    arg1 - (void *)
//
// The commands handled by IOEthernetInterface are:
//
//    SIOCSIFADDR
//    SIOCSIFFLAGS
//    SIOCADDMULTI
//    SIOCDELMULTI
//
// Returns an error code defined in errno.h (BSD).

SInt32 IOEthernetInterface::performCommand( IONetworkController * ctr,
                                            unsigned long         cmd,
                                            void *                arg0,
                                            void *                arg1 )
{
    SInt32  ret;

    assert( arg0 == _arpcom );

    if ( ctr == 0 ) return EINVAL;

    switch ( cmd )
    {
        case SIOCSIFFLAGS:
        case SIOCADDMULTI:
        case SIOCDELMULTI:
        case SIOCSIFADDR:
        case SIOCSIFMTU:
		case SIOCSIFDEVMTU:
		case SIOCGIFDEVMTU:
        case SIOCSIFLLADDR:
            ret = (int) ctr->executeCommand(
                             this,            /* client */
                             (IONetworkController::Action)
                                &IOEthernetInterface::performGatedCommand,
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

int IOEthernetInterface::performGatedCommand(void * target,
                                             void * arg1_ctr,
                                             void * arg2_cmd,
                                             void * arg3_0,
                                             void * arg4_1)
{
    IOEthernetInterface * self = (IOEthernetInterface *) target;
    IONetworkController * ctr  = (IONetworkController *) arg1_ctr;
    struct ifreq *        ifr  = (struct ifreq *) arg4_1;
    SInt                  ret  = EOPNOTSUPP;

    // Refuse to perform controller I/O if the controller is in a
    // low-power state that makes it "unusable".

    if ( self->_controllerLostPower ||
        ( self->getInterfaceState() & kIONetworkInterfaceDisabledState ) )
         return EPWROFF;

    switch ( (uintptr_t) arg2_cmd )
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
            ret = self->syncSIOCSIFMTU( ctr, ifr, 0 );
            break;
		
		case SIOCSIFDEVMTU:
			ret = self->syncSIOCSIFMTU(ctr, ifr, 1);
			break;
		
		case SIOCGIFDEVMTU:
			ret = self->syncSIOCGIFDEVMTU(ctr, ifr);
			break;
			
        case SIOCSIFLLADDR:
            ret = self->syncSIOCSIFLLADDR( ctr, ifr->ifr_addr.sa_data,
                                           ifr->ifr_addr.sa_len );
            break;
    }

    return ret;
}

//---------------------------------------------------------------------------
// enableController() is reponsible for calling the controller's enable()
// method and restoring the state of the controller. We assume that
// controllers can completely reset its state upon receiving a disable()
// method call. And when it is brought back up, the interface should
// assist in restoring the previous state of the Ethernet controller.

IOReturn IOEthernetInterface::enableController(IONetworkController * ctr)
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

        disableFilter(ctr, gIOEthernetWakeOnLANFilterGroup, ~0,
                      kFilterOptionNoStateChange);

        // Restore current filter selection.

        SET_ACTIVE_FILTERS(gIONetworkFilterGroup, 0);
        ret = enableFilter(ctr, gIONetworkFilterGroup, 0,
                           kFilterOptionSyncPendingIO);
        if ( ret != kIOReturnSuccess ) break;

        // Restore multicast filter settings.

        syncSIOCADDMULTI(ctr);

        // Re-apply the user supplied link-level address.

        OSData * lladdr = OSDynamicCast(OSData, getProperty(kIOMACAddress));
        if ( lladdr && lladdr->getLength() == ETHER_ADDR_LEN )
        {
            ctr->setHardwareAddress( lladdr->getBytesNoCopy(),
                                     lladdr->getLength() );
        }

        _ctrEnabled = true;
        
        // Publish WOL support flags after interface is marked enabled.

        reportInterfaceWakeFlags();

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
// Handles SIOCSIFFLAGS ioctl command for Ethernet interfaces. The network
// stack has changed the if_flags field in ifnet. Our job is to go
// through if_flags and see what has changed, and act accordingly.
//
// The fact that if_flags contains both generic and Ethernet specific bits
// means that we cannot move some of the default flag processing to the
// superclass.

int IOEthernetInterface::syncSIOCSIFFLAGS(IONetworkController * ctr)
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

SInt IOEthernetInterface::syncSIOCSIFADDR(IONetworkController * ctr)
{
    IOReturn ret = kIOReturnSuccess;

    // Interface is implicitly brought up by an SIOCSIFADDR ioctl.

    setFlags(IFF_UP);

    if ( (getFlags() & IFF_RUNNING) == 0 )
    {
        if ( (ret = enableController(ctr)) == kIOReturnSuccess )
            setFlags(IFF_RUNNING);
    }
               
    return errnoFromReturn(ret);
}

//---------------------------------------------------------------------------
// Handle SIOCADDMULTI ioctl command.

SInt IOEthernetInterface::syncSIOCADDMULTI(IONetworkController * ctr)
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

SInt IOEthernetInterface::syncSIOCDELMULTI(IONetworkController * ctr)
{
    return syncSIOCADDMULTI(ctr);
}

#define MTU_TO_FRAMESIZE(x) \
((x) + kIOEthernetCRCSize + sizeof(struct ether_header))

#define FRAMESIZE_TO_MTU(x) \
((x) - kIOEthernetCRCSize - sizeof(struct ether_header))

int IOEthernetInterface::syncSIOCGIFDEVMTU( IONetworkController * ctr,
											struct ifreq *        ifr )
{
	IOReturn ret;
    UInt32  size;

	ifr->ifr_devmtu.ifdm_current = max(_altMTU, getMaxTransferUnit());
		
	ret =  ctr->getMaxPacketSize( &size );
	if(ret == kIOReturnSuccess )
	{
		ifr->ifr_devmtu.ifdm_max = FRAMESIZE_TO_MTU(size);
		ret = ctr->getMinPacketSize( &size );
		if(ret == kIOReturnSuccess)
			ifr->ifr_devmtu.ifdm_min = FRAMESIZE_TO_MTU( size );
	}
	return errnoFromReturn( ret );
}

//---------------------------------------------------------------------------
// Handle SIOCSIFMTU ioctl.
	
int IOEthernetInterface::syncSIOCSIFMTU( IONetworkController * ctr,
                                         struct ifreq *        ifr,
										 bool setAltMTU)
{

	IOReturn ret = kIOReturnSuccess;
	UInt32	newControllerMTU, oldControllerMTU;
	UInt32	softMTU = getMaxTransferUnit();
	UInt32	requestedMTU = ifr->ifr_mtu;
	
    UInt32  maxFrameSize = kIOEthernetMaxPacketSize;  // 1518
	UInt32	minFrameSize = kIOEthernetMinPacketSize;
	UInt32	tempSize;
	
	// Determine controller's max allowable mtu...
	if ( ctr->getMaxPacketSize( &tempSize ) == kIOReturnSuccess )
		maxFrameSize = max( tempSize, kIOEthernetMaxPacketSize );
	
	// ...and reject requests that are too big.
	if ( MTU_TO_FRAMESIZE( requestedMTU ) > maxFrameSize )
		return EINVAL;	// MTU is too large for the controller.

	// Determine controller's min allowable mtu...
	ctr->getMinPacketSize( &minFrameSize );

	// ...and reject requests that are too small.
	if ( MTU_TO_FRAMESIZE( requestedMTU ) < minFrameSize && !(setAltMTU && requestedMTU==0))  //allow setting dev MTU to 0 to turn it off
		return EINVAL;	// MTU is too small for the controller.
	
	//the controller gets the max of the mtu we're changing and the one we're not.
	newControllerMTU = max(requestedMTU, setAltMTU ? softMTU : _altMTU);
	
	// determine what's currently set on the controller (the max of current soft and alt settings)
	oldControllerMTU = max(softMTU, _altMTU);
		
	// we only have to change the controller if the new value is different from the old,
	// and either the new value is bigger than the standard max ethernet or the old value was.
	if(newControllerMTU != oldControllerMTU && 
	   ( (MTU_TO_FRAMESIZE(newControllerMTU) > kIOEthernetMaxPacketSize) ||
		 (MTU_TO_FRAMESIZE(oldControllerMTU) > kIOEthernetMaxPacketSize) )
	   )
	{
		ret = ctr->setMaxPacketSize( max(MTU_TO_FRAMESIZE(newControllerMTU), kIOEthernetMaxPacketSize)); //don't set smaller than standard max ethernet
	}
	if(ret == kIOReturnSuccess) //if we successfully set value in controller (or didn't need to) then store the new value
	{
		
		if(setAltMTU)
			_altMTU = requestedMTU;
		else
			setMaxTransferUnit( requestedMTU );
	}
	return errnoFromReturn( ret );
}

//---------------------------------------------------------------------------

int IOEthernetInterface::syncSIOCSIFLLADDR( IONetworkController * ctr,
                                            const char * lladdr, int len )
{
	unsigned char tempaddr[kIOEthernetAddressSize];
	OSData *hardAddr;
	
	if(len != kIOEthernetAddressSize)
		return EINVAL;
	
   if (_ctrEnabled != true)    /* reject if interface is down */
        return (ENETDOWN);
	// keep a backup in case stack refuses our change
	hardAddr = OSDynamicCast(OSData, getProperty(kIOMACAddress));
	if(hardAddr && hardAddr->getLength() == kIOEthernetAddressSize)
		bcopy(hardAddr->getBytesNoCopy(), tempaddr, kIOEthernetAddressSize);
	
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

        DLOG("%s: SIOCSIFLLADDR %02x:%02x:%02x:%02x:%02x:%02x\n",
              ctr->getName(),
              lladdr[0], lladdr[1], lladdr[2],
              lladdr[3], lladdr[4], lladdr[5]);
   }

    return 0;
}

//---------------------------------------------------------------------------
// Enable a packet filter.

#define getOneFilter(x)   ((x) & (~((x) - 1)))

// this stub's sole purpose is to eliminate warning generated by gcc in enableFilter
// when we try to cast a ptr to member function to a ptr to a 'C' function.

IOReturn 
IOEthernetInterface::enableFilter_Wrapper(
    IOEthernetInterface *   self,
    IONetworkController *   ctr,
    const OSSymbol *        group,
    UInt32                  filters,
    IOOptionBits            options)
{
	return self->enableFilter(ctr, group, filters, options);
}

IOReturn
IOEthernetInterface::enableFilter(IONetworkController * ctr,
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
                           (IONetworkController::Action)
                               &IOEthernetInterface::enableFilter_Wrapper,
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
IOEthernetInterface::disableFilter(IONetworkController * ctr,
                                   const OSSymbol *      group,
                                   UInt32                filters,
                                   IOOptionBits          options)
{
    IOReturn ret = kIOReturnSuccess;

#if NOT_NEEDED  // disableFilter is always called from gated context
    if ( options & kFilterOptionNotInsideGate )
    {
        options &= ~kFilterOptionNotInsideGate;
    
    	return ctr->executeCommand(
                           this,               /* client */
                           (IONetworkController::Action)
                               &IOEthernetInterface::disableFilter,
                           this,               /* target */
                           (void *) ctr,       /* param0 */
                           (void *) group,     /* param1 */
                           (void *) filters,   /* param2 */
                           (void *) options ); /* param3 */
    }
#endif

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
IOEthernetInterface::setupMulticastFilter(IONetworkController * ctr)
{
    void *               multiAddrs = 0;
    UInt                 mcount;
    OSData *             mcData = 0;
    ifnet_t				interface;
    struct sockaddr  dlAddress;
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
            
        mcData = OSData::withCapacity(mcount * ETHER_ADDR_LEN);
        if (!mcData)
        {
            DLOG("%s: no memory for multicast address list\n", getName());
			ifnet_free_multicast_list(addressList);
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

            ok = mcData->appendBytes((const void *) addrp, ETHER_ADDR_LEN);
            assert(ok);
        }

        multiAddrs = (void *) mcData->getBytesNoCopy();
        assert(multiAddrs);
    }

    // Issue a controller command to setup the multicast filter.

    ret = ((IOEthernetController *)ctr)->setMulticastList(
                                            (IOEthernetAddress *) multiAddrs,
                                            mcount);
    if (mcData)
    {
        if (ret == kIOReturnSuccess)
            setProperty(kIOMulticastAddressList, mcData);

        mcData->release();
    }
    else {
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
IOEthernetInterface::controllerWillChangePowerState(
                               IONetworkController * ctr,
                               IOPMPowerFlags        flags,
                               UInt32                stateNumber,
                               IOService *           policyMaker )
{
	if ( ( (flags & IOPMDeviceUsable ) == 0) && 
         ( _controllerLostPower == false ) )
    {
        _controllerLostPower = true;

        if (_ctrEnabled)
        {
            if (policyMaker)
            {
                unsigned long filters;

                // Called from PM instead of shutdown/restart context.
                // Get the "aggressiveness" factor from the policy maker.

                ctr->getAggressiveness( kPMEthernetWakeOnLANSettings, &filters );

                filters &= _supportedWakeFilters;

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
                    filters &= ~( kIOEthernetWakeOnMagicPacket |
                                  kIOEthernetWakeOnPacketAddressMatch );
                }

                // Before controller is disabled, program its wake up filters.
                // The WOL setting is formed by a bitwise OR between the WOL filter
                // settings, and the aggresiveness factor from the policy maker.

                enableFilter( ctr,
                              gIOEthernetWakeOnLANFilterGroup,
                              filters,
                              kFilterOptionNoStateChange |
                              kFilterOptionSyncPendingIO );
            }

            // Call SIOCSIFFLAGS handler to disable the controller,
            // and mark the interface as Not Running.

            syncSIOCSIFFLAGS(ctr);
        }
    }
    
    return super::controllerWillChangePowerState( ctr, flags,
                                                  stateNumber,
                                                  policyMaker );
}

IOReturn
IOEthernetInterface::controllerDidChangePowerState(
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
        // perhaps enable the controller, restore all Ethernet controller
        // state, then mark the interface as Running.

        syncSIOCSIFFLAGS(ctr);
    }

    return ret;
}

//---------------------------------------------------------------------------
// Handle a request to set properties from kernel or non-kernel clients.
// For non-kernel clients, the preferred mechanism is through an user
// client.

IOReturn IOEthernetInterface::setProperties( OSObject * properties )
{
#ifndef __LP64__
#define kIONetworkInterfaceProperties   "IONetworkInterfaceProperties"

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
                        dict->getObject(kIOEthernetWakeOnLANFilterGroup) );

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
#else /* __LP64__ */
    return super::setProperties(properties);
#endif
}

//---------------------------------------------------------------------------
// willTerminate

bool IOEthernetInterface::willTerminate( IOService *  provider,
                                         IOOptionBits options )
{
    bool ret = super::willTerminate( provider, options );

    // We assume that willTerminate() is always called from the
    // provider's work loop context.

    // Once the gated ioctl path has been blocked, disable the controller.
    // The hardware may have already been removed from the system.

    if ( _ctrEnabled && getController() )
    {
        DLOG("IOEthernetInterface::willTerminate disabling controller\n");
        getController()->doDisable( this );
        _ctrEnabled = false;
    }

    return ret;
}

//---------------------------------------------------------------------------

IOReturn IOEthernetInterface::attachToDataLinkLayer( IOOptionBits options,
                                                     void *       parameter )
{
    IOReturn ret = super::attachToDataLinkLayer( options, parameter );
    if ( ret == kIOReturnSuccess )
    {
		// after successful attach, the interface is backed by an actual BSD ifnet_t.
		// now we can set a few important ethernet specific values on it.
		// Set defaults suitable for Ethernet interfaces.
		ifnet_set_baudrate( getIfnet(), 10000000); //FIXME maybe IONetworkInterface should provide accessor
		bpfattach( getIfnet(), DLT_EN10MB, sizeof(struct ether_header) );
   }
    return ret;
}

#define VLAN_HEADER_LEN 4
// Temporarily stuff a vlan tag back into a packet so that tag shows up to bpf.
// We do it by creating a temp header mbuf with the enet/vlan header in it and
// then point its next field to the proper place (after the dest+src addresses) in the original
// mbuf.  
void IOEthernetInterface::_fixupVlanPacket(mbuf_t mt, u_int16_t vlan_tag, int inputPacket)
{
	mbuf_t newmb;
	mbuf_t chain;
	size_t remainingBytes;
	size_t copyBytes = 0;  //initialize to prevent annoying, incorrect warning that it's used uninitialized
	char * destptr;
	
	if( mbuf_gethdr(M_DONTWAIT, MT_DATA, &newmb) )
		return;
		
	//init enough of the mbuf to keep bpf happy
	mbuf_setlen(newmb, ETHER_ADDR_LEN*2 + VLAN_HEADER_LEN);
	mbuf_pkthdr_setlen(newmb, mbuf_pkthdr_len( mt ) + VLAN_HEADER_LEN);	
	mbuf_pkthdr_setrcvif(newmb, mbuf_pkthdr_rcvif( mt ) );
	
	//now walk the incoming mbuf to copy out its dst & src address and
	//locate the type/len field in the packet.
	chain = mt;
	remainingBytes = ETHER_ADDR_LEN*2;
	destptr = (char *)mbuf_data( newmb );
	
	while(chain && remainingBytes)
	{
		copyBytes = remainingBytes > mbuf_len( chain ) ? mbuf_len( chain ): remainingBytes;
		
		remainingBytes -= copyBytes;
		bcopy( mbuf_data( chain ), destptr, copyBytes);
		destptr += copyBytes;
		if (mbuf_len( chain ) == copyBytes) //we've completely drained this mbuf
		{
			chain = mbuf_next( chain );  //advance to next
			copyBytes = 0; //if we break out of loop now, make sure the offset is correct
		}
	}
	
	// chain points to the mbuf that contains the packet data with type/len field
	// and copyBytes indicates the offset it's at.
	if(chain==0 || remainingBytes)
	{
		mbuf_freem( newmb );
		return; //if we can't munge the packet, just return
	}
	
	//patch mbuf so its data points after the dst+src address
	mbuf_setdata(chain, (char *)mbuf_data( chain ) + copyBytes, mbuf_len( chain ) - copyBytes );
	
	//finish setting up our head mbuf
	*(short *)(destptr) = htons(ETHERTYPE_VLAN); //vlan magic number
	*(short *)(destptr + 2) = htons( vlan_tag ); // and the tag's value
	mbuf_setnext( newmb, chain ); //stick it infront of the rest of the packet
	
	// feed the tap
	if(inputPacket)
		super::feedPacketInputTap( newmb );
	else
		super::feedPacketOutputTap( newmb );
	
	//release the fake header
	mbuf_setnext( newmb, NULL );
	mbuf_freem( newmb );

	//and repair our old mbuf
	mbuf_setdata( chain, (char *)mbuf_data( chain ) - copyBytes, mbuf_len( chain ) + copyBytes );
}

//we need to feed the taps the packets as they appear
//on the wire...but devices with hardware vlan stuffing/stripping
//put that info oob. so we must munge the packet with the
//vlan, feed the tap, then unmunge it.

void IOEthernetInterface::feedPacketInputTap(mbuf_t mt)
{
	u_int16_t vlan;
	
	if( mbuf_get_vlan_tag(mt, &vlan) == 0) //does this packet have oob vlan tag?
		_fixupVlanPacket(mt, vlan, 1);
	else
		super::feedPacketInputTap(mt);
}

void IOEthernetInterface::feedPacketOutputTap(mbuf_t mt)
{
	u_int16_t vlan;
	if( mbuf_get_vlan_tag(mt, &vlan) == 0) //does this packet have oob vlan tag?
		_fixupVlanPacket(mt, vlan, 0);
	else
		super::feedPacketOutputTap(mt);
}

//---------------------------------------------------------------------------

bool IOEthernetInterface::inputEvent( UInt32 type, void * data )
{
    if ((type == kIONetworkEventTypeLinkUp)   ||
        (type == kIONetworkEventTypeLinkDown) ||
        (type == kIONetworkEventWakeOnLANSupportChanged))
    {
        // reportInterfaceWakeFlags() callout
        retain();
        if (thread_call_enter( _inputEventThreadCall ) == TRUE)
            release();
    }

    return super::inputEvent(type, data);
}

void IOEthernetInterface::handleEthernetInputEvent(
    thread_call_param_t param0,
    thread_call_param_t /*param1 not used*/ )
{
    IOEthernetInterface * me = (IOEthernetInterface *) param0;
    IONetworkController * ctr;
    
    if (me)
    {
        ctr = me->getController();
        if (ctr) ctr->executeCommand(
            me,     /* client */
                    /* action */
            OSMemberFunctionCast(
                IONetworkController::Action, me,
                &IOEthernetInterface::reportInterfaceWakeFlags),
            me );   /* target */

        me->release();
    }
}

//---------------------------------------------------------------------------

void IOEthernetInterface::reportInterfaceWakeFlags( void )
{
    ifnet_t                 ifnet;
    IONetworkController *   ctr;
    OSNumber *              number;
    unsigned long           wakeSetting = 0;
    uint32_t                disabled    = 0;
    uint32_t                filters     = 0;
    uint32_t                linkStatus  = 0;

    ctr   = getController();
    ifnet = getIfnet();

    if (!ifnet || !ctr)
        return;

    // Across system sleep/wake, link down is expected, this should
    // not trigger a wake flags changed.

    if (_controllerLostPower)
    {
        DLOG("en%u: controllerLostPower\n", getUnitNumber());
        return;
    }

    do {
        // Report negative if controller is disabled or does not support WOL.

        if (!_ctrEnabled ||
            ((_supportedWakeFilters & kIOEthernetWakeOnMagicPacket) == 0))
        {
            DLOG("en%u: ctrEnabled = %x, WakeFilters = %x\n",
                getUnitNumber(), _ctrEnabled, _supportedWakeFilters);
            break;
        }

        // Poll for disabled WOL filters, which is allowed to change
        // after every link and WOL changed event.

        if (_disabledWakeFilters)
        {
            if ( ctr->getPacketFilters(
                    gIOEthernetDisabledWakeOnLANFilterGroup,
                    (UInt32 *) &disabled ) != kIOReturnSuccess )
            {
                disabled = 0;
            }
            _disabledWakeFilters->setValue( disabled );
        }

        // Check if network wake option is enabled,
        // that also implies system is on AC power.

        getAggressiveness(kPMEthernetWakeOnLANSettings, &wakeSetting);
        filters = wakeSetting & _supportedWakeFilters & ~disabled;
        DLOG("en%u: WakeSetting = %lx, WakeFilters = %x, disabled = %x\n",
            getUnitNumber(), wakeSetting, _supportedWakeFilters, disabled);
        
        if ((kIOEthernetWakeOnMagicPacket & filters) == 0)
            break;

        // Check driver is reporting valid link.

        number = OSDynamicCast(OSNumber, ctr->getProperty(kIOLinkStatus));
        if (!number)
        {
            filters = 0;
            break;
        }

        linkStatus = number->unsigned32BitValue();
        if ((linkStatus & (kIONetworkLinkValid | kIONetworkLinkActive)) ==
            kIONetworkLinkValid)
        {
            filters = 0;
        }
    }
    while (false);

    filters &= IFNET_WAKE_ON_MAGIC_PACKET;
    if (filters != (ifnet_get_wake_flags(ifnet) & IFNET_WAKE_ON_MAGIC_PACKET))
    {
        ifnet_set_wake_flags(ifnet, filters, IFNET_WAKE_ON_MAGIC_PACKET);
        DLOG("en%u: ifnet_set_wake_flags = %x\n", getUnitNumber(), filters);
    }
}
