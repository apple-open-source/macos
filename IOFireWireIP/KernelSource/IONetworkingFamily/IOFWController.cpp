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
 * IOFWController.cpp
 *
 * Abstract FireWire controller superclass.
 */

#include <IOKit/assert.h>
#include "IOFWController.h"
#include "IOFWInterface.h"

//---------------------------------------------------------------------------

#define super IONetworkController

OSDefineMetaClassAndStructors( IOFWController, IONetworkController)

OSMetaClassDefineReservedUnused( IOFWController,  0);
OSMetaClassDefineReservedUnused( IOFWController,  1);
OSMetaClassDefineReservedUnused( IOFWController,  2);
OSMetaClassDefineReservedUnused( IOFWController,  3);
OSMetaClassDefineReservedUnused( IOFWController,  4);
OSMetaClassDefineReservedUnused( IOFWController,  5);
OSMetaClassDefineReservedUnused( IOFWController,  6);
OSMetaClassDefineReservedUnused( IOFWController,  7);
OSMetaClassDefineReservedUnused( IOFWController,  8);
OSMetaClassDefineReservedUnused( IOFWController,  9);
OSMetaClassDefineReservedUnused( IOFWController, 10);
OSMetaClassDefineReservedUnused( IOFWController, 11);
OSMetaClassDefineReservedUnused( IOFWController, 12);
OSMetaClassDefineReservedUnused( IOFWController, 13);
OSMetaClassDefineReservedUnused( IOFWController, 14);
OSMetaClassDefineReservedUnused( IOFWController, 15);
OSMetaClassDefineReservedUnused( IOFWController, 16);
OSMetaClassDefineReservedUnused( IOFWController, 17);
OSMetaClassDefineReservedUnused( IOFWController, 18);
OSMetaClassDefineReservedUnused( IOFWController, 19);
OSMetaClassDefineReservedUnused( IOFWController, 20);
OSMetaClassDefineReservedUnused( IOFWController, 21);
OSMetaClassDefineReservedUnused( IOFWController, 22);
OSMetaClassDefineReservedUnused( IOFWController, 23);
OSMetaClassDefineReservedUnused( IOFWController, 24);
OSMetaClassDefineReservedUnused( IOFWController, 25);
OSMetaClassDefineReservedUnused( IOFWController, 26);
OSMetaClassDefineReservedUnused( IOFWController, 27);
OSMetaClassDefineReservedUnused( IOFWController, 28);
OSMetaClassDefineReservedUnused( IOFWController, 29);
OSMetaClassDefineReservedUnused( IOFWController, 30);
OSMetaClassDefineReservedUnused( IOFWController, 31);

//-------------------------------------------------------------------------
// Macros

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

//---------------------------------------------------------------------------
// IOFWController class initializer.

void IOFWController::initialize()
{
}

//---------------------------------------------------------------------------
// Initialize an IOFWController instance.

bool IOFWController::init(OSDictionary * properties)
{
    if (!super::init(properties))
    {
        DLOG("IOFWController: super::init() failed\n");
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------
// Free the IOFWController instance.

void IOFWController::free()
{
    // Any allocated resources should be released here.

    super::free();
}

//---------------------------------------------------------------------------
// Publish FireWire controller capabilites and properties.

bool IOFWController::publishProperties()
{
    bool			ret = false;
    IOFWAddress		addr;
    OSDictionary	*dict;

    do {
        // Let the superclass publish properties first.

        if (super::publishProperties() == false)
            break;

        // Publish the controller's FireWire address.

        if ( (getHardwareAddress(&addr) != kIOReturnSuccess) ||
             (setProperty(kIOMACAddress,  (void *) &addr,
                          kIOFWAddressSize) == false) )
        {
            break;
        }

        // Publish FireWire defined packet filters.
        
        dict = OSDynamicCast(OSDictionary, getProperty(kIOPacketFilters));
        if ( dict )
        {
            UInt32			filters;
            OSNumber		*num;
            OSDictionary	*newdict;
			
            
            if ( getPacketFilters(gIOEthernetWakeOnLANFilterGroup,
                                  &filters) != kIOReturnSuccess )
            {
                break;
            }

            num = OSNumber::withNumber(filters, sizeof(filters) * 8);
            if (num == 0)
                break;

			//to avoid race condition with external threads we'll modify a copy of dictionary
			newdict = OSDictionary::withDictionary(dict); //copy the dictionary
			if(newdict)
			{
				ret = newdict->setObject(gIOEthernetWakeOnLANFilterGroup, num); //and add the WOL group to it
				setProperty(kIOPacketFilters, newdict); //then replace the property with the new dictionary
				newdict->release();
			}
            num->release();
        }
    }
    while (false);

    return ret;
}

//---------------------------------------------------------------------------
// Set or change the station address used by the FireWire controller.

IOReturn
IOFWController::setHardwareAddress(const IOFWAddress * addr)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Enable or disable multicast mode.

IOReturn IOFWController::setMulticastMode(bool active)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Enable or disable promiscuous mode.

IOReturn IOFWController::setPromiscuousMode(bool active)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Enable or disable the wake on Magic Packet support.

IOReturn IOFWController::setWakeOnMagicPacket(bool active)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Set the list of multicast addresses that the multicast filter should use
// to match against the destination address of an incoming frame. The frame 
// should be accepted when a match occurs.

IOReturn IOFWController::setMulticastList(IOFWAddress * /*addrs*/,
                                                UInt32              /*count*/)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Allocate and return a new IOFWInterface instance.

IONetworkInterface * IOFWController::createInterface()
{
    IOFWInterface * netif = new IOFWInterface;

    if ( netif && ( netif->init( this ) == false ) )
    {
        netif->release();
        netif = 0;
    }
    return netif;
}

//---------------------------------------------------------------------------
// Returns all the packet filters supported by the FireWire controller.
// This method will perform a bitwise OR of:
//
//    kIOPacketFilterUnicast
//    kIOPacketFilterBroadcast
//    kIOPacketFilterMulticast
//    kIOPacketFilterPromiscuous
//
// and write it to the argument provided if the group specified is
// gIONetworkFilterGroup, otherwise 0 is returned. Drivers that support
// a different set of filters should override this method.
//
// Returns kIOReturnSuccess. Drivers that override this method must return
// kIOReturnSuccess to indicate success, or an error code otherwise.

IOReturn
IOFWController::getPacketFilters(const OSSymbol * group,
                                       UInt32 *         filters) const
{
    *filters = 0;

    if ( group == gIONetworkFilterGroup )
    {
        return getPacketFilters(filters);
    }
    else
    {
        return kIOReturnSuccess;
    }
}

IOReturn IOFWController::getPacketFilters(UInt32 * filters) const
{
    *filters = ( kIOPacketFilterUnicast     |
                 kIOPacketFilterBroadcast   |
                 kIOPacketFilterMulticast   |
                 kIOPacketFilterPromiscuous );

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Enable a filter from the specified group.

#define UCAST_BCAST_MASK \
        ( kIOPacketFilterUnicast | kIOPacketFilterBroadcast )

IOReturn IOFWController::enablePacketFilter(
                                     const OSSymbol * group,
                                     UInt32           aFilter,
                                     UInt32           enabledFilters,
                                     IOOptionBits     options)
{
    IOReturn  ret = kIOReturnUnsupported;
    UInt32    newFilters = enabledFilters | aFilter;

    if ( group == gIONetworkFilterGroup )
    {
        // The default action is to call setMulticastMode() or
        // setPromiscuousMode() to handle multicast or promiscuous
        // filter changes.

        if ( aFilter == kIOPacketFilterMulticast )
        {
            ret = setMulticastMode(true);
        }
        else if ( aFilter == kIOPacketFilterPromiscuous )
        {
            ret = setPromiscuousMode(true);
        }
        else if ( (newFilters ^ enabledFilters) & UCAST_BCAST_MASK )
        {
            ret = kIOReturnSuccess;
        }
    }
    else if ( group == gIOEthernetWakeOnLANFilterGroup )
    {
        if ( aFilter == kIOFWWakeOnMagicPacket )
        {
            ret = setWakeOnMagicPacket(true);
        }
    }

    return ret;
}

//---------------------------------------------------------------------------
// Disable a filter from the specifed filter group.

IOReturn IOFWController::disablePacketFilter(
                                      const OSSymbol * group,
                                      UInt32           aFilter,
                                      UInt32           enabledFilters,
                                      IOOptionBits     options)
{
    IOReturn  ret = kIOReturnUnsupported;
    UInt32    newFilters = enabledFilters & ~aFilter;
        
    if ( group == gIONetworkFilterGroup )
    {
        // The default action is to call setMulticastMode() or
        // setPromiscuousMode() to handle multicast or promiscuous
        // filter changes.
    
        if ( aFilter == kIOPacketFilterMulticast )
        {
            ret = setMulticastMode(false);
        }
        else if ( aFilter == kIOPacketFilterPromiscuous )
        {
            ret = setPromiscuousMode(false);
        }
        else if ( (newFilters ^ enabledFilters) & UCAST_BCAST_MASK )
        {
            ret = kIOReturnSuccess;
        }
    }
    else if ( group == gIOEthernetWakeOnLANFilterGroup )
    {
        if ( aFilter == kIOFWWakeOnMagicPacket )
        {
            ret = setWakeOnMagicPacket(false);
        }
    }

    return ret;
}

IOReturn 
IOFWController::getHardwareAddress(IOFWAddress * addrP)
{
	return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Get the FireWire controller's station address.
// Call the FireWire specific (overloaded) form.

IOReturn
IOFWController::getHardwareAddress(void *   addr,
                                         UInt32 * inOutAddrBytes)
{
    UInt32 bufBytes;

    if (inOutAddrBytes == 0)
        return kIOReturnBadArgument;

    // Cache the size of the caller's buffer, and replace it with the
    // number of bytes required.

    bufBytes        = *inOutAddrBytes;
    *inOutAddrBytes = kIOFWAddressSize;

    // Make sure the buffer is large enough for a single FireWire
    // hardware address.

    if ((addr == 0) || (bufBytes < kIOFWAddressSize))
        return kIOReturnNoSpace;

    return getHardwareAddress((IOFWAddress *) addr);
}

//---------------------------------------------------------------------------
// Set or change the station address used by the FireWire controller.
// Call the FireWire specific (overloaded) version of this method.

IOReturn
IOFWController::setHardwareAddress(const void * addr,
                                         UInt32       addrBytes)
{
    if ((addr == 0) || (addrBytes != kIOFWAddressSize))
        return kIOReturnBadArgument;

    return setHardwareAddress((const IOFWAddress *) addr);
}

//---------------------------------------------------------------------------
// Report the max/min packet sizes, including the frame header and FCS bytes.

IOReturn IOFWController::getMaxPacketSize(UInt32 * maxSize) const
{
    *maxSize = kIOFWMaxPacketSize;
    return kIOReturnSuccess;
}

IOReturn IOFWController::getMinPacketSize(UInt32 * minSize) const
{
    *minSize = kIOFWMinPacketSize;
    return kIOReturnSuccess;
}
