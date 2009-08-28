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
 * IOFWController.h
 *
 */

#ifndef _IOFWCONTROLLER_H
#define _IOFWCONTROLLER_H

#include <IOKit/network/IONetworkController.h>

/*! @defined kIOFWControllerClass
    @abstract kIOFWControllerClass is the name of the
        IOFWController class. */

#define kIOFWControllerClass        "IOFWController"

/*! @defined kIOFWAddressSize
    @abstract The number of bytes in an FireWire hardware address. */

#define kIOFWAddressSize            8

/*! @defined kIOFWMaxPacketSize
    @abstract The maximum size of an FireWire packet, including
        the FCS bytes. */

#define kIOFWMaxPacketSize          4096

/*! @defined kIOFWMinPacketSize
    @abstract The minimum size of an FireWire packet, including
        the FCS bytes. */

#define kIOFWMinPacketSize     64

/*! @defined kIOFWCRCSize
    @abstract The size in bytes of the 32-bit CRC value appended
        to the end of each FireWire frame. */

#define kIOFWCRCSize                0

/*! @defined kIOFWWakeOnLANFilterGroup
    @abstract kIOFWWakeOnLANFilterGroup describes the name assigned
        to the FireWire wake-On-LAN filter group. */

#define kIOFWWakeOnLANFilterGroup   "IOFWWakeOnLANFilterGroup"

/*! @enum Enumeration of wake-On-LAN filters.
    @discussion An enumeration of all filters in the wake-on-LAN filter
        group. Each filter listed will respond to a network event that
        will trigger a system wake-up.
    @constant kIOFWWakeOnMagicPacket Reception of a Magic Packet.
    @constant kIOFWWakeOnPacketAddressMatch Reception of a packet
    which passes through any of the address filtering mechanisms based
    on its destination FireWire address. This may include unicast,
    broadcast, or multicast addresses depending on the current state
    and setting of the corresponding packet filters. */

enum {
    kIOFWWakeOnMagicPacket         = 0x00000001,
    kIOFWWakeOnPacketAddressMatch  = 0x00000002
};

/*
 * Kernel
 */
#if defined(KERNEL) && defined(__cplusplus)

struct IOFWAddress {
	UInt8 bytes[kIOFWAddressSize];
};

/*!	@defined gIOEthernetWakeOnLANFilterGroup
    @discussion gIOEthernetWakeOnLANFilterGroup is an OSSymbol object
        that contains the name of the FireWire wake-on-LAN filter group
        defined by kIOFWWakeOnLANFilterGroup. */

extern const OSSymbol * gIOEthernetWakeOnLANFilterGroup;

/*! @class IOFWController : public IONetworkController
    @abstract An abstract superclass for FireWire controllers. FireWire 
    controller drivers should subclass IOFWController, and implement
    or override the hardware specific methods to create an FireWire driver.
    An interface object (an IOFWInterface instance) must be
    instantiated by the driver, through attachInterface(), to connect
    the controller driver to the data link layer. */

class IOFWController : public IONetworkController
{
    OSDeclareDefaultStructors( IOFWController )

protected:
    struct ExpansionData { };
    /*! @var reserved
        Reserved for future use.  (Internal use only)  */
    ExpansionData *  _reserved;


public:

/*! @function initialize
    @abstract IOFWController class initializer.
    @discussion Create global OSSymbol objects that are used as keys. */

    static void initialize();

/*! @function init
    @abstract Initialize an IOFWController object.
    @param properties A dictionary object containing a property table
        associated with this instance.
    @result true on success, false otherwise. */ 

    virtual bool init(OSDictionary * properties);

/*! @function getPacketFilters
    @abstract Get the set of packet filters supported by the FireWire 
    controller in the given filter group.
    @discussion The default implementation of the abstract method inherited
    from IONetworkController. When the filter group specified is
    gIONetworkFilterGroup, then this method will return a value formed by
    a bitwise OR of kIOPacketFilterUnicast, kIOPacketFilterBroadcast,
    kIOPacketFilterMulticast, kIOPacketFilterPromiscuous. Otherwise, the 
    return value will be set to zero (0). Subclasses must override this
    method if their filtering capability differs from what is reported by
    this default implementation. This method is called from the workloop
    context, and the result is published to the I/O Kit registry.
    @param group The name of the filter group.
    @param filters Pointer to the mask of supported filters returned by
    	this method.
    @result kIOReturnSuccess. Drivers that override this
    method must return kIOReturnSuccess to indicate success, or an error 
    return code otherwise. */

    virtual IOReturn getPacketFilters(const OSSymbol * group,
                                      UInt32 *         filters) const;

/*! @function enablePacketFilter
    @abstract Enable one of the supported packet filters from the
    given filter group.
    @discussion The default implementation of the abstract method inherited
    from IONetworkController. This method will call setMulticastMode() or
    setPromiscuousMode() when the multicast or the promiscuous filter is to be
    enabled. Requests to disable the Unicast or Broadcast filters are handled
    silently, without informing the subclass. Subclasses can override this
    method to change this default behavior, or to extend it to handle
    additional filter types or filter groups. This method call is synchronized
    by the workloop's gate.
    @param group The name of the filter group containing the filter to be
    enabled.
    @param aFilter The filter to enable.
    @param enabledFilters All filters currently enabled by the client.
    @param options Optional flags for the enable request.
    @result The return from setMulticastMode() or setPromiscuousMode() if
    either of those two methods are called. kIOReturnSuccess if the filter
    specified is kIOPacketFilterUnicast or kIOPacketFilterBroadcast.
    kIOReturnUnsupported if the filter group specified is not
    gIONetworkFilterGroup. */

    virtual IOReturn enablePacketFilter(const OSSymbol * group,
                                        UInt32           aFilter,
                                        UInt32           enabledFilters,
                                        IOOptionBits     options = 0);

/*! @function disablePacketFilter
    @abstract Disable a packet filter that is currently enabled from the
    given filter group.
    @discussion The default implementation of the abstract method inherited
    from IONetworkController. This method will call setMulticastMode() or
    setPromiscuousMode() when the multicast or the promiscuous filter is to be
    disabled. Requests to disable the Unicast or Broadcast filters are handled
    silently, without informing the subclass. Subclasses can override this
    method to change this default behavior, or to extend it to handle
    additional filter types or filter groups. This method call is synchronized
    by the workloop's gate.
    @param group The name of the filter group containing the filter to be
    disabled.
    @param aFilter The filter to disable.
    @param enabledFilters All filters currently enabled by the client.
    @param options Optional flags for the disable request.
    @result The return from setMulticastMode() or setPromiscuousMode() if
    either of those two methods are called. kIOReturnSuccess if the filter
    specified is kIOPacketFilterUnicast or kIOPacketFilterBroadcast.
    kIOReturnUnsupported if the filter group specified is not
    gIONetworkFilterGroup. */

    virtual IOReturn disablePacketFilter(const OSSymbol * group,
                                         UInt32           aFilter,
                                         UInt32           enabledFilters,
                                         IOOptionBits     options = 0);

/*! @function getHardwareAddress
    @abstract Get the FireWire controller's station address.
    @discussion The default implementation of the abstract method inherited
    from IONetworkController. This method will call the overloaded form
    IOFWController::getHardwareAddress() that subclasses are expected
    to override.
    @param addr The buffer where the controller's hardware address should
           be written.
    @param inOutAddrBytes The size of the address buffer provided by the
           client, and replaced by this method with the actual size of
           the hardware address in bytes.
    @result kIOReturnSuccess on success, or an error otherwise. */

    virtual IOReturn getHardwareAddress(void *   addr,
                                        UInt32 * inOutAddrBytes);

/*! @function setHardwareAddress
    @abstract Set or change the station address used by the FireWire
    controller.
    @discussion The default implementation of the abstract method inherited
    from IONetworkController. This method will call the overloaded form
    IOFWController::setHardwareAddress() that subclasses are expected
    to override.
    @param addr The buffer containing the hardware address provided by
    the client.
    @param addrBytes The size of the address buffer provided by the
    client in bytes.
    @result kIOReturnSuccess on success, or an error otherwise. */

    virtual IOReturn setHardwareAddress(const void * addr,
                                        UInt32       addrBytes);

/*! @function getMaxPacketSize
    @abstract Get the maximum packet size supported by the FireWire
        controller, including the frame header and FCS.
    @param maxSize Pointer to the return value.
    @result kIOReturnSuccess on success, or an error code otherwise. */

    virtual IOReturn getMaxPacketSize(UInt32 * maxSize) const;

/*! @function getMinPacketSize
    @abstract Get the minimum packet size supported by the FireWire
        controller, including the frame header and FCS.
    @param minSize Pointer to the return value.
    @result kIOReturnSuccess on success, or an error code otherwise. */

    virtual IOReturn getMinPacketSize(UInt32 * minSize) const;

/*! @function getPacketFilters
    @abstract Get the set of packet filters supported by the FireWire 
    controller in the network filter group.
    @param filters Pointer to the return value containing a mask of
    supported filters.
    @result kIOReturnSuccess. Drivers that override this
    method must return kIOReturnSuccess to indicate success, or an error 
    return code otherwise. */

    virtual IOReturn getPacketFilters(UInt32 * filters) const;

/*! @function getHardwareAddress
    @abstract Get the FireWire controller's permanent station address.
    @discussion. FireWire drivers must implement this method, by reading the
    address from hardware and writing it to the buffer provided. This method
    is called from the workloop context.
    @param addrP Pointer to an IOFWAddress where the hardware address
    should be returned.
    @result kIOReturnSuccess on success, or an error return code otherwise. */

    virtual IOReturn getHardwareAddress(IOFWAddress * addrP);

/*! @function setHardwareAddress
    @abstract Set or change the station address used by the FireWire
        controller.
    @discussion This method is called in response to a client command to
    change the station address used by the FireWire controller. Implementation
    of this method is optional. This method is called from the workloop context.
    @param addrP Pointer to an IOFWAddress containing the new station
    address.
    @result The default implementation will always return kIOReturnUnsupported.
    If overridden, drivers must return kIOReturnSuccess on success, or an error
    return code otherwise. */

    virtual IOReturn setHardwareAddress(const IOFWAddress * addrP);

/*! @function setMulticastMode
    @abstract Enable or disable multicast mode.
    @discussion Called by enablePacketFilter() or disablePacketFilter()
    when there is a change in the activation state of the multicast filter
    identified by kIOPacketFilterMulticast. This method is called from the
    workloop context.
    @param active True to enable multicast mode, false to disable it.
    @result kIOReturnUnsupported. If overridden, drivers must return
    kIOReturnSuccess on success, or an error return code otherwise. */

    virtual IOReturn setMulticastMode(bool active);

/*! @function setMulticastList
    @abstract Set the list of multicast addresses that the multicast filter
    should use to match against the destination address of an incoming frame.
    The frame should be accepted when a match occurs.
    @discussion Called when the multicast group membership of an interface
    object is changed. Drivers that support kIOPacketFilterMulticast should
    override this method and update the hardware multicast filter using the
    list of FireWire addresses provided. Perfect multicast filtering is
    preferred if supported by the hardware, to order to reduce the number of
    unwanted packets received. If the number of multicast addresses in the
    list exceeds what the hardware is capable of supporting, or if perfect
    filtering is not supported, then ideally the hardware should be programmed
    to perform imperfect filtering, thorugh some form of hash filtering
    mechanism. Only at the last resort should the driver enable reception of
    all multicast packets to satisfy this request. This method is called
    from the workloop context, and only if the driver reports
    kIOPacketFilterMulticast support in getPacketFilters().
    @param addrs An array of FireWire addresses. This argument must be
        ignored if the count argument is 0.
    @param count The number of FireWire addresses in the list. This value
        will be zero when the list becomes empty.
    @result kIOReturnUnsupported. Drivers must return kIOReturnSuccess to
    indicate success, or an error return code otherwise. */

    virtual IOReturn setMulticastList(IOFWAddress * addrs, 
                                      UInt32              count);

/*! @function setPromiscuousMode
    @abstract Enable or disable promiscuous mode.
    @discussion Called by enablePacketFilter() or disablePacketFilter()
    when there is a change in the activation state of the promiscuous
    filter identified by kIOPacketFilterPromiscuous. This method is 
    called from the workloop context.
    @param active True to enable promiscuous mode, false to disable it.
    @result kIOReturnUnsupported. If overridden, drivers must return
    kIOReturnSuccess on success, or an error return code otherwise. */

    virtual IOReturn setPromiscuousMode(bool active);

/*! @function setWakeOnMagicPacket
    @abstract Enable or disable the wake on Magic Packet support.
    @discussion Called by enablePacketFilter() or disablePacketFilter()
    when there is a change in the activation state of the wake-on-LAN
    filter identified by kIOFWWakeOnMagicPacket. This method is
    called from the workloop context.
    @param active True to enable support for system wake on reception
    of a Magic Packet, false to disable it.
    @result kIOReturnUnsupported. If overridden, drivers must return
    kIOReturnSuccess on success, or an error return code otherwise. */

    virtual IOReturn setWakeOnMagicPacket(bool active);

protected:

/*! @function createInterface
    @abstract Create an IOFWInterface object.
    @discussion Allocate and return a new IOFWInterface instance.
    A subclass of IONetworkController must implement this method and return
    a matching interface object. The implementation in IOFWController
    will return an IOFWInterface object. Subclasses of
    IOFWController, such as FireWire controller drivers, will have
    little reason to override this implementation.
    @result A newly allocated and initialized IOFWInterface object. */

    virtual IONetworkInterface * createInterface();

/*! @function free
    @abstract Free the IOFWController instance. Release resources,
    then followed by a call to super::free(). */

    virtual void free();

/*! @function publishProperties
    @abstract Publish FireWire controller properties and capabilities.
    @discussion Publish FireWire controller properties to the property
    table. For instance, getHardwareAddress() is called to fetch the
    hardware address, and the address is then published to the property
    table. This method call is synchronized by the workloop's gate,
    and must never be called directly by subclasses.
    @result true if all properties and capabilities were discovered,
    and published successfully, false otherwise. Returning false will
    prevent client objects from attaching to the FireWire controller
    since a property that a client relies upon may be missing. */

    virtual bool publishProperties();

    // Virtual function padding
    OSMetaClassDeclareReservedUnused( IOFWController,  0);
    OSMetaClassDeclareReservedUnused( IOFWController,  1);
    OSMetaClassDeclareReservedUnused( IOFWController,  2);
    OSMetaClassDeclareReservedUnused( IOFWController,  3);
    OSMetaClassDeclareReservedUnused( IOFWController,  4);
    OSMetaClassDeclareReservedUnused( IOFWController,  5);
    OSMetaClassDeclareReservedUnused( IOFWController,  6);
    OSMetaClassDeclareReservedUnused( IOFWController,  7);
    OSMetaClassDeclareReservedUnused( IOFWController,  8);
    OSMetaClassDeclareReservedUnused( IOFWController,  9);
    OSMetaClassDeclareReservedUnused( IOFWController, 10);
    OSMetaClassDeclareReservedUnused( IOFWController, 11);
    OSMetaClassDeclareReservedUnused( IOFWController, 12);
    OSMetaClassDeclareReservedUnused( IOFWController, 13);
    OSMetaClassDeclareReservedUnused( IOFWController, 14);
    OSMetaClassDeclareReservedUnused( IOFWController, 15);
    OSMetaClassDeclareReservedUnused( IOFWController, 16);
    OSMetaClassDeclareReservedUnused( IOFWController, 17);
    OSMetaClassDeclareReservedUnused( IOFWController, 18);
    OSMetaClassDeclareReservedUnused( IOFWController, 19);
    OSMetaClassDeclareReservedUnused( IOFWController, 20);
    OSMetaClassDeclareReservedUnused( IOFWController, 21);
    OSMetaClassDeclareReservedUnused( IOFWController, 22);
    OSMetaClassDeclareReservedUnused( IOFWController, 23);
    OSMetaClassDeclareReservedUnused( IOFWController, 24);
    OSMetaClassDeclareReservedUnused( IOFWController, 25);
    OSMetaClassDeclareReservedUnused( IOFWController, 26);
    OSMetaClassDeclareReservedUnused( IOFWController, 27);
    OSMetaClassDeclareReservedUnused( IOFWController, 28);
    OSMetaClassDeclareReservedUnused( IOFWController, 29);
    OSMetaClassDeclareReservedUnused( IOFWController, 30);
    OSMetaClassDeclareReservedUnused( IOFWController, 31);
};

/*
 * FIXME: remove this.
 */
enum {
    kIOEnetPromiscuousModeOff   = false,
    kIOEnetPromiscuousModeOn    = true,
    kIOEnetPromiscuousModeAll   = true,
    kIOEnetMulticastModeOff     = false,
    kIOEnetMulticastModeFilter  = true
};
typedef bool IOEnetPromiscuousMode;
typedef bool IOEnetMulticastMode;

#endif /* defined(KERNEL) && defined(__cplusplus) */

#endif /* !_IOFWCONTROLLER_H */
