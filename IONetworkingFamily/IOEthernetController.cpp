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
 * IOEthernetController.cpp
 *
 * Abstract Ethernet controller superclass.
 *
 * HISTORY
 *
 * Dec 3, 1998  jliu - C++ conversion.
 */

#include <IOKit/assert.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/IOMapper.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "IONetworkDebug.h"

extern "C" {
#include <sys/param.h>  // mbuf limits defined here.
#include <sys/mbuf.h>
}

#include <AssertMacros.h>


class IOECStateNotifier : public OSObject
{
	OSDeclareDefaultStructors(IOECStateNotifier)
	
public:
	virtual bool init(IOEthernetController::avb_state_callback_t callback, void *context);
	
	bool isCallbackAndContext(IOEthernetController::avb_state_callback_t callback, void *context);
	
	void notify(IOEthernetControllerAVBState oldState, IOEthernetControllerAVBState newState);
	
private:
	IOEthernetController::avb_state_callback_t fCallback;
	void *fContext;
};

class IOECTimeSyncHandler : public OSObject
{
	OSDeclareDefaultStructors(IOECTimeSyncHandler)
	
public:
	virtual bool init(IOEthernetController::avb_packet_callback_t callback, void *context, uint32_t callbackID);
	
	bool isCallbackAndContext(IOEthernetController::avb_packet_callback_t callback, void *context);
	
	void call(IOEthernetController::IOEthernetAVBPacket *packet);
	
	uint32_t callbackID(void);
	
private:
	IOEthernetController::avb_packet_callback_t fCallback;
	void *fContext;
	uint32_t fCallbackID;
};

struct IOEthernetController::IOECTSCallbackEntry
{
	struct IOEthernetController::IOECTSCallbackEntry *next;
	IOEthernetController::IOEthernetAVBPacket *packet;
};



//---------------------------------------------------------------------------

#define super IONetworkController

OSDefineMetaClassAndAbstractStructors( IOEthernetController, IONetworkController)
OSMetaClassDefineReservedUnused( IOEthernetController, 10);
OSMetaClassDefineReservedUnused( IOEthernetController, 11);
OSMetaClassDefineReservedUnused( IOEthernetController, 12);
OSMetaClassDefineReservedUnused( IOEthernetController, 13);
OSMetaClassDefineReservedUnused( IOEthernetController, 14);
OSMetaClassDefineReservedUnused( IOEthernetController, 15);
OSMetaClassDefineReservedUnused( IOEthernetController, 16);
OSMetaClassDefineReservedUnused( IOEthernetController, 17);
OSMetaClassDefineReservedUnused( IOEthernetController, 18);
OSMetaClassDefineReservedUnused( IOEthernetController, 19);
OSMetaClassDefineReservedUnused( IOEthernetController, 20);
OSMetaClassDefineReservedUnused( IOEthernetController, 21);
OSMetaClassDefineReservedUnused( IOEthernetController, 22);
OSMetaClassDefineReservedUnused( IOEthernetController, 23);
OSMetaClassDefineReservedUnused( IOEthernetController, 24);
OSMetaClassDefineReservedUnused( IOEthernetController, 25);
OSMetaClassDefineReservedUnused( IOEthernetController, 26);
OSMetaClassDefineReservedUnused( IOEthernetController, 27);
OSMetaClassDefineReservedUnused( IOEthernetController, 28);
OSMetaClassDefineReservedUnused( IOEthernetController, 29);
OSMetaClassDefineReservedUnused( IOEthernetController, 30);
OSMetaClassDefineReservedUnused( IOEthernetController, 31);

//---------------------------------------------------------------------------
// IOEthernetController class initializer.

void IOEthernetController::initialize()
{
}

//---------------------------------------------------------------------------
// Initialize an IOEthernetController instance.

bool IOEthernetController::init(OSDictionary * properties)
{
    if (!super::init(properties))
    {
        DLOG("IOEthernetController: super::init() failed\n");
        return false;
    }
	
	_reserved = (ExpansionData *)IOMalloc(sizeof(ExpansionData));
	if(!_reserved)
	{
		DLOG("IOEthernetController: failed to allocate expansion struct\n");
		return false;
	}
	
	memset(_reserved, 0, sizeof(ExpansionData));
	
	_reserved->fStateLock = IOLockAlloc();
	if(!_reserved->fStateLock)
	{
		DLOG("IOEthernetController: failed to allocate state lock\n");
		return false;
	}
	_reserved->fStateChangeNotifiers = OSArray::withCapacity(0);
	if(!_reserved->fStateChangeNotifiers)
	{
		DLOG("IOEthernetController: failed to allocate state change notifiers array\n");
		return false;
	}
	_reserved->fStateChangeNotifiersLock = IOLockAlloc();
	if(!_reserved->fStateChangeNotifiersLock)
	{
		DLOG("IOEthernetController: failed to allocate state change lock\n");
		return false;
	}
	_reserved->fTimeSyncReceiveHandlers = OSArray::withCapacity(0);
	if(!_reserved->fTimeSyncReceiveHandlers)
	{
		DLOG("IOEthernetController: failed to allocate time sync receive handlers array\n");
		return false;
	}
	_reserved->fTimeSyncReceiveHandlersLock = IOLockAlloc();
	if(!_reserved->fTimeSyncReceiveHandlersLock)
	{
		DLOG("IOEthernetController: failed to allocate timesync receive handlers lock\n");
		return false;
	}
	_reserved->fTimeSyncTransmitHandlers = OSArray::withCapacity(0);
	if(!_reserved->fTimeSyncTransmitHandlers)
	{
		DLOG("IOEthernetController: failed to allocate time sync transmit handlers array\n");
		return false;
	}
	_reserved->fTimeSyncTransmitHandlersLock = IOLockAlloc();
	if(!_reserved->fTimeSyncTransmitHandlersLock)
	{
		DLOG("IOEthernetController: failed to allocate timesync transmit handlers lock\n");
		return false;
	}
	_reserved->fHasTimeSyncTransmitCallbackIDAvailable = true;
	
	_reserved->fAVBControllerState = kIOEthernetControllerAVBStateActivated;
	
	if(KERN_SUCCESS != semaphore_create(kernel_task, &_reserved->fTimeSyncCallbackStartSemaphore, SYNC_POLICY_FIFO, 0))
	{
		DLOG("IOEthernetController: failed to allocate timesync queue start semaphore\n");
		return false;
	}
	
	if(KERN_SUCCESS != semaphore_create(kernel_task, &_reserved->fTimeSyncCallbackStopSemaphore, SYNC_POLICY_FIFO, 0))
	{
		DLOG("IOEthernetController: failed to allocate timesync queue stop semaphore\n");
		return false;
	}
	
	if(KERN_SUCCESS != semaphore_create(kernel_task, &_reserved->fTimeSyncCallbackQueueSemaphore, SYNC_POLICY_FIFO, 0))
	{
		DLOG("IOEthernetController: failed to allocate timesync queue semaphore\n");
		return false;
	}
	
	nanoseconds_to_absolutetime(50 * NSEC_PER_MSEC, &_reserved->fTimeSyncCallbackTimeoutTime);
	
	_reserved->fTimeSyncTransmitCallbackQueueLock = IOLockAlloc();
	if(!_reserved->fTimeSyncTransmitCallbackQueueLock)
	{
		DLOG("IOEthernetController: failed to allocate timesync transmit queue lock\n");
		return false;
	}
	
	_reserved->fTimeSyncReceiveCallbackQueueLock = IOLockAlloc();
	if(!_reserved->fTimeSyncReceiveCallbackQueueLock)
	{
		DLOG("IOEthernetController: failed to allocate timesync transmit queue lock\n");
		return false;
	}
	
    return true;
}

//---------------------------------------------------------------------------
// Free the IOEthernetController instance.

void IOEthernetController::free()
{
    // Any allocated resources should be released here.
	
	if(_reserved)
	{
		if(_reserved->fTransmitQueuePacketLatency)
		{
			IOFree(_reserved->fTransmitQueuePacketLatency, sizeof(uint64_t) * _reserved->fNumberOfRealtimeTransmitQueues);
		}
		if(_reserved->fTransmitQueuePrefetchDelay)
		{
			IOFree(_reserved->fTransmitQueuePrefetchDelay, sizeof(uint64_t) * _reserved->fNumberOfRealtimeTransmitQueues);
		}
		
		if(_reserved->fStateLock)
		{
			IOLockFree(_reserved->fStateLock);
		}
		if(_reserved->fStateChangeNotifiers)
		{
			_reserved->fStateChangeNotifiers->release();
		}
		if(_reserved->fStateChangeNotifiersLock)
		{
			IOLockFree(_reserved->fStateChangeNotifiersLock);
		}
		if(_reserved->fTimeSyncReceiveHandlers)
		{
			_reserved->fTimeSyncReceiveHandlers->release();
		}
		if(_reserved->fTimeSyncReceiveHandlersLock)
		{
			IOLockFree(_reserved->fTimeSyncReceiveHandlersLock);
		}
		if(_reserved->fTimeSyncTransmitHandlers)
		{
			_reserved->fTimeSyncTransmitHandlers->release();
		}
		if(_reserved->fTimeSyncTransmitHandlersLock)
		{
			IOLockFree(_reserved->fTimeSyncTransmitHandlersLock);
		}
		
		if(_reserved->fTimeSyncCallbackStartSemaphore)
		{
			semaphore_destroy(kernel_task, _reserved->fTimeSyncCallbackStartSemaphore);
		}
		
		if(_reserved->fTimeSyncCallbackStopSemaphore)
		{
			semaphore_destroy(kernel_task, _reserved->fTimeSyncCallbackStopSemaphore);
		}
		
		if(_reserved->fTimeSyncCallbackQueueSemaphore)
		{
			semaphore_destroy(kernel_task, _reserved->fTimeSyncCallbackQueueSemaphore);
		}
		
		if(_reserved->fTimeSyncTransmitCallbackQueueLock)
		{
			IOLockFree(_reserved->fTimeSyncTransmitCallbackQueueLock);
		}
		
		if(_reserved->fTimeSyncReceiveCallbackQueueLock)
		{
			IOLockFree(_reserved->fTimeSyncReceiveCallbackQueueLock);
		}
		
		if(_reserved->fTimeSyncTransmitCallbackQueue)
		{
			IOECTSCallbackEntry *thisEntry = NULL;
			IOECTSCallbackEntry *nextEntry = _reserved->fTimeSyncTransmitCallbackQueue;
			
			while(nextEntry)
			{
				thisEntry = nextEntry;
				nextEntry = thisEntry->next;
				
				completeAVBPacket(thisEntry->packet);
				
				IOFree(thisEntry, sizeof(struct IOECTSCallbackEntry));
			}
		}
		
		if(_reserved->fTimeSyncReceiveCallbackQueue)
		{
			IOECTSCallbackEntry *thisEntry = NULL;
			IOECTSCallbackEntry *nextEntry = _reserved->fTimeSyncReceiveCallbackQueue;
			
			while(nextEntry)
			{
				thisEntry = nextEntry;
				nextEntry = thisEntry->next;
				
				completeAVBPacket(thisEntry->packet);
				
				IOFree(thisEntry, sizeof(struct IOECTSCallbackEntry));
			}
		}
		
		if(_reserved->fAVBPacketMapper)
		{
			_reserved->fAVBPacketMapper->release();
		}

		IOFree(_reserved, sizeof(ExpansionData));
	}

    super::free();
}

//---------------------------------------------------------------------------
// Publish Ethernet controller capabilites and properties.

bool IOEthernetController::publishProperties()
{
    bool              ret = false;
    IOEthernetAddress addr;
    OSDictionary *    dict;

    do {
        // Let the superclass publish properties first.

        if (super::publishProperties() == false)
            break;

        // Publish the controller's Ethernet address.

        if ( (getHardwareAddress(&addr) != kIOReturnSuccess) ||
             (setProperty(kIOMACAddress,  (void *) &addr,
                          kIOEthernetAddressSize) == false) )
        {
            break;
        }

        // Publish Ethernet defined packet filters.

        dict = OSDynamicCast(OSDictionary, getProperty(kIOPacketFilters));
        if ( dict )
        {
            OSNumber *      num;
            OSDictionary *  newdict;
            UInt32          supported = 0;
            UInt32          disabled  = 0;

            // Supported WOL filters
            if ( getPacketFilters(
                    gIOEthernetWakeOnLANFilterGroup,
                    &supported) != kIOReturnSuccess )
            {
                supported = 0;
            }

            // Disabled WOL filters
            if ( getPacketFilters(
                    gIOEthernetDisabledWakeOnLANFilterGroup,
                    &disabled) != kIOReturnSuccess )
            {
                disabled = 0;
            }

			newdict = OSDictionary::withDictionary(
                        dict, dict->getCount() + (supported ? 2 : 1));
			if (newdict)
			{
                // Supported WOL filters
                num = OSNumber::withNumber(supported, sizeof(supported) * 8);
                if (num)
                {
                    ret = newdict->setObject(gIOEthernetWakeOnLANFilterGroup, num);
                    num->release();
                }

                // Disabled WOL filters
                if (supported)
                {
                    num = OSNumber::withNumber(disabled, sizeof(disabled) * 8);
                    if (num)
                    {
                        ret = newdict->setObject(
                            gIOEthernetDisabledWakeOnLANFilterGroup, num);
                        num->release();
                    }
                }

                if (ret)
                    setProperty(kIOPacketFilters, newdict);

                newdict->release();
            }
        }
    }
    while (false);

    return ret;
}

//---------------------------------------------------------------------------
// Set or change the station address used by the Ethernet controller.

IOReturn
IOEthernetController::setHardwareAddress(const IOEthernetAddress * addr)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Enable or disable multicast mode.

IOReturn IOEthernetController::setMulticastMode(bool active)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Enable or disable promiscuous mode.

IOReturn IOEthernetController::setPromiscuousMode(bool active)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Enable or disable the wake on Magic Packet support.

IOReturn IOEthernetController::setWakeOnMagicPacket(bool active)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Set the list of multicast addresses that the multicast filter should use
// to match against the destination address of an incoming frame. The frame 
// should be accepted when a match occurs.

IOReturn IOEthernetController::setMulticastList(IOEthernetAddress * /*addrs*/,
                                                UInt32              /*count*/)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Allocate and return a new IOEthernetInterface instance.

IONetworkInterface * IOEthernetController::createInterface()
{
    IOEthernetInterface * netif = new IOEthernetInterface;

    if ( netif && ( netif->init( this ) == false ) )
    {
        netif->release();
        netif = 0;
    }
    return netif;
}

//---------------------------------------------------------------------------
// Returns all the packet filters supported by the Ethernet controller.
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
IOEthernetController::getPacketFilters(const OSSymbol * group,
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

IOReturn IOEthernetController::getPacketFilters(UInt32 * filters) const
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

IOReturn IOEthernetController::enablePacketFilter(
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
        if ( aFilter == kIOEthernetWakeOnMagicPacket )
        {
            ret = setWakeOnMagicPacket(true);
        }
    }

    return ret;
}

//---------------------------------------------------------------------------
// Disable a filter from the specifed filter group.

IOReturn IOEthernetController::disablePacketFilter(
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
        if ( aFilter == kIOEthernetWakeOnMagicPacket )
        {
            ret = setWakeOnMagicPacket(false);
        }
    }

    return ret;
}

//---------------------------------------------------------------------------
// Get the Ethernet controller's station address.
// Call the Ethernet specific (overloaded) form.

IOReturn
IOEthernetController::getHardwareAddress(void *   addr,
                                         UInt32 * inOutAddrBytes)
{
    UInt32 bufBytes;

    if (inOutAddrBytes == 0)
        return kIOReturnBadArgument;

    // Cache the size of the caller's buffer, and replace it with the
    // number of bytes required.

    bufBytes        = *inOutAddrBytes;
    *inOutAddrBytes = kIOEthernetAddressSize;

    // Make sure the buffer is large enough for a single Ethernet
    // hardware address.

    if ((addr == 0) || (bufBytes < kIOEthernetAddressSize))
        return kIOReturnNoSpace;

    return getHardwareAddress((IOEthernetAddress *) addr);
}

//---------------------------------------------------------------------------
// Set or change the station address used by the Ethernet controller.
// Call the Ethernet specific (overloaded) version of this method.

IOReturn
IOEthernetController::setHardwareAddress(const void * addr,
                                         UInt32       addrBytes)
{
    if ((addr == 0) || (addrBytes != kIOEthernetAddressSize))
        return kIOReturnBadArgument;

    return setHardwareAddress((const IOEthernetAddress *) addr);
}

//---------------------------------------------------------------------------
// Report the max/min packet sizes, including the frame header and FCS bytes.

IOReturn IOEthernetController::getMaxPacketSize(UInt32 * maxSize) const
{
    *maxSize = kIOEthernetMaxPacketSize;
    return kIOReturnSuccess;
}

IOReturn IOEthernetController::getMinPacketSize(UInt32 * minSize) const
{
    *minSize = kIOEthernetMinPacketSize;
    return kIOReturnSuccess;
}

OSMetaClassDefineReservedUsed( IOEthernetController,  0);
bool IOEthernetController::getVlanTagDemand(mbuf_t mt, UInt32 *vlantag)
{
	u_int16_t tag;
	int rval = mbuf_get_vlan_tag(mt, &tag);
	if(rval == 0)
	{
		*vlantag = tag;
		return true;
	}
	return false;
}

OSMetaClassDefineReservedUsed( IOEthernetController,  1);
void IOEthernetController::setVlanTag(mbuf_t mt, UInt32 vlantag)
{
	mbuf_set_vlan_tag(mt, vlantag);
}




/*
 Indicates that AVB streaming is supported and what capabilities it has.

 True if this controller has at least 1 real time transmit queues or at least 1 realtime receive queues
 */
bool IOEthernetController::getAVBSupport(IOEthernetControllerAVBSupport *avbSupport) const
{
	bool result = false;
	
	if(_reserved)
	{
		if(_reserved->fNumberOfRealtimeTransmitQueues || _reserved->fNumberOfRealtimeReceiveQueues)
		{
			result = true;
		}
		
		if(avbSupport)
		{
			avbSupport->timeSyncSupport = _reserved->fTimeSyncSupport;
			avbSupport->numberOfRealtimeTransmitQueues = _reserved->fNumberOfRealtimeTransmitQueues;
			avbSupport->numberOfRealtimeReceiveQueues = _reserved->fNumberOfRealtimeReceiveQueues;
			avbSupport->realtimeMulticastIsAllowed = _reserved->fRealtimeMulticastAllowed;
			avbSupport->packetMapper = _reserved->fAVBPacketMapper;
		}
	}
	
	return result;
}

/*
 Sets up the realtime multicast allowed in the AVB support information.
 */
void IOEthernetController::setRealtimeMulticastIsAllowed(bool realtimeMulticastAllowed)
{
	if(_reserved)
	{
		_reserved->fRealtimeMulticastAllowed = realtimeMulticastAllowed;
	}
}

/*
 Sets up the packet mapper in the AVB support information.
 */
void IOEthernetController::setAVBPacketMapper(IOMapper *packetMaper)
{
	if(_reserved)
	{
		if(_reserved->fAVBPacketMapper)
		{
			_reserved->fAVBPacketMapper->release();
		}
		_reserved->fAVBPacketMapper = packetMaper;
		if(_reserved->fAVBPacketMapper)
		{
			_reserved->fAVBPacketMapper->retain();
		}
	}
}

#pragma mark Interface State
/*
 Get the current AVB state of the controller.
 */
IOEthernetControllerAVBState IOEthernetController::getControllerAVBState(void) const
{
	IOEthernetControllerAVBState result = kIOEthernetControllerAVBStateDisabled;
	
	if(_reserved)
	{
		result = _reserved->fAVBControllerState;
	}
	
	return result;
}

/*
 Change the AVB state of the AVB state of the controller.
 */
IOReturn IOEthernetController::changeAVBControllerState(IOEthernetControllerAVBStateEvent event)
{
	IOReturn result = kIOReturnInternalError;
	
	if(_reserved)
	{
		IOLockLock(_reserved->fStateLock);
		
		switch (event)
		{
			case kIOEthernetControllerAVBStateEventDisable:
				switch (_reserved->fAVBControllerState)
			{
				case kIOEthernetControllerAVBStateDisabled:
					result = kIOReturnSuccess;
					break;
				case kIOEthernetControllerAVBStateActivated:
				case kIOEthernetControllerAVBStateTimeSyncEnabled:
				case kIOEthernetControllerAVBStateAVBEnabled:
					result = setAVBControllerState(kIOEthernetControllerAVBStateDisabled);
					break;
			}
				break;
			case kIOEthernetControllerAVBStateEventEnable:
				switch (_reserved->fAVBControllerState)
			{
				case kIOEthernetControllerAVBStateDisabled:
					if(_reserved->fAVBEnabled)
					{
						result = setAVBControllerState(kIOEthernetControllerAVBStateAVBEnabled);
					}
					else if(_reserved->fTimeSyncEnabled)
					{
						result = setAVBControllerState(kIOEthernetControllerAVBStateTimeSyncEnabled);
					}
					else
					{
						result = setAVBControllerState(kIOEthernetControllerAVBStateActivated);
					}
					break;
				case kIOEthernetControllerAVBStateActivated:
				case kIOEthernetControllerAVBStateTimeSyncEnabled:
				case kIOEthernetControllerAVBStateAVBEnabled:
					result = kIOReturnInvalid;
					break;
			}
				break;
			case kIOEthernetControllerAVBStateEventStartTimeSync:
				switch (_reserved->fAVBControllerState)
			{
				case kIOEthernetControllerAVBStateDisabled:
					result = kIOReturnNotPermitted;
					break;
				case kIOEthernetControllerAVBStateActivated:
					result = setAVBControllerState(kIOEthernetControllerAVBStateTimeSyncEnabled);
					if(kIOReturnSuccess == result)
					{
						_reserved->fTimeSyncEnabled = 1;
					}
					break;
				case kIOEthernetControllerAVBStateTimeSyncEnabled:
				case kIOEthernetControllerAVBStateAVBEnabled:
					if(_reserved->fTimeSyncEnabled < INT32_MAX)
					{
						_reserved->fTimeSyncEnabled++;
						result = kIOReturnSuccess;
					}
					else
					{
						result = kIOReturnInternalError;
					}
					break;
			}
				break;
			case kIOEthernetControllerAVBStateEventStopTimeSync:
				switch (_reserved->fAVBControllerState)
			{
				case kIOEthernetControllerAVBStateDisabled:
					if(_reserved->fTimeSyncEnabled > 0)
					{
						_reserved->fTimeSyncEnabled--;
						result = kIOReturnSuccess;
					}
					else
					{
						result = kIOReturnNotPermitted;
					}
					break;
				case kIOEthernetControllerAVBStateActivated:
					result = kIOReturnInvalid;
					break;
				case kIOEthernetControllerAVBStateTimeSyncEnabled:
					if(_reserved->fTimeSyncEnabled > 0)
					{
						_reserved->fTimeSyncEnabled--;
						if(!_reserved->fTimeSyncEnabled)
						{
							result = setAVBControllerState(kIOEthernetControllerAVBStateActivated);
						}
						else
						{
							result = kIOReturnSuccess;
						}
					}
					else
					{
						//This is an invalid cofiguration don't know how we got here so lets fix it
						setAVBControllerState(kIOEthernetControllerAVBStateActivated);
						_reserved->fTimeSyncEnabled = 0;
						result = kIOReturnInvalid;
					}
					break;
				case kIOEthernetControllerAVBStateAVBEnabled:
					result = kIOReturnSuccess;
					_reserved->fTimeSyncEnabled = false;
					break;
			}
				break;
			case kIOEthernetControllerAVBStateEventStartStreaming:
				switch (_reserved->fAVBControllerState)
			{
				case kIOEthernetControllerAVBStateDisabled:
					result = kIOReturnNotPermitted;
					break;
				case kIOEthernetControllerAVBStateActivated:
				case kIOEthernetControllerAVBStateTimeSyncEnabled:
					result = setAVBControllerState(kIOEthernetControllerAVBStateAVBEnabled);
					if(kIOReturnSuccess == result)
					{
						_reserved->fAVBEnabled = 1;
					}
					break;
				case kIOEthernetControllerAVBStateAVBEnabled:
					if(_reserved->fAVBEnabled < INT32_MAX)
					{
						_reserved->fAVBEnabled++;
						result = kIOReturnSuccess;
					}
					else
					{
						result = kIOReturnInternalError;
					}
					break;
			}
				break;
			case kIOEthernetControllerAVBStateEventStopStreaming:
				switch (_reserved->fAVBControllerState)
			{
				case kIOEthernetControllerAVBStateDisabled:
					if(_reserved->fAVBEnabled > 0)
					{
						_reserved->fAVBEnabled--;
						result = kIOReturnSuccess;
					}
					else
					{
						result = kIOReturnNotPermitted;
					}
					break;
				case kIOEthernetControllerAVBStateActivated:
				case kIOEthernetControllerAVBStateTimeSyncEnabled:
					result = kIOReturnInvalid;
					break;
				case kIOEthernetControllerAVBStateAVBEnabled:
					if(_reserved->fAVBEnabled > 0)
					{
						_reserved->fAVBEnabled--;
						if(!_reserved->fAVBEnabled)
						{
							if(_reserved->fTimeSyncEnabled)
							{
								result = setAVBControllerState(kIOEthernetControllerAVBStateTimeSyncEnabled);
							}
							else
							{
								result = setAVBControllerState(kIOEthernetControllerAVBStateActivated);
							}
						}
						else
						{
							result = kIOReturnSuccess;
						}
					}
					else
					{
						result = kIOReturnInvalid;
					}
					break;
			}
				break;
		}
		
		IOLockUnlock(_reserved->fStateLock);
	}
	
	return result;
}

/*
 Function to register to receive callbacks whenever the AVB state changes.
 */
IOReturn IOEthernetController::registerForAVBStateChangeNotifications(avb_state_callback_t callback, void *context)
{
	IOReturn result = kIOReturnInternalError;
	
	if(_reserved)
	{
		if(callback)
		{
			result = kIOReturnError;
			
			IOLockLock(_reserved->fStateChangeNotifiersLock);
			
			bool found = false;
			
			unsigned int count = _reserved->fStateChangeNotifiers->getCount();
			for(unsigned int index = 0; index < count; index++)
			{
				IOECStateNotifier *stateNotifier = OSDynamicCast(IOECStateNotifier, _reserved->fStateChangeNotifiers->getObject(index));
				
				if(stateNotifier)
				{
					if(stateNotifier->isCallbackAndContext(callback, context))
					{
						found = true;
						break;
					}
				}
			}
			
			if(!found)
			{
				IOECStateNotifier *stateNotifier = OSTypeAlloc(IOECStateNotifier);
				
				if(stateNotifier)
				{
					if(stateNotifier->init(callback, context))
					{
						_reserved->fStateChangeNotifiers->setObject(stateNotifier);
						result = kIOReturnSuccess;
					}
					stateNotifier->release();
				}
				else
				{
					result = kIOReturnNoMemory;
				}
			}
			else
			{
				result = kIOReturnExclusiveAccess;
			}
			
			IOLockUnlock(_reserved->fStateChangeNotifiersLock);
		}
		else
		{
			result = kIOReturnBadArgument;
		}
	}
	
	return result;
}

/*
 Function to deregister from receiving callbacks whenever the AVB state changes.
 */
IOReturn IOEthernetController::deregisterForAVBStateChangeNotifications(avb_state_callback_t callback, void *context)
{
	IOReturn result = kIOReturnInternalError;
	
	if(_reserved)
	{
		if(callback)
		{
			result = kIOReturnNotFound;
			
			IOLockLock(_reserved->fStateChangeNotifiersLock);
			
			unsigned int count = _reserved->fStateChangeNotifiers->getCount();
			for(unsigned int index = 0; index < count; index++)
			{
				IOECStateNotifier *stateNotifier = OSDynamicCast(IOECStateNotifier, _reserved->fStateChangeNotifiers->getObject(index));
				
				if(stateNotifier)
				{
					if(stateNotifier->isCallbackAndContext(callback, context))
					{
						_reserved->fStateChangeNotifiers->removeObject(index);
						result = kIOReturnSuccess;
						break;
					}
				}
			}
			
			IOLockUnlock(_reserved->fStateChangeNotifiersLock);
		}
		else
		{
			result = kIOReturnBadArgument;
		}
	}
	
	return result;
}

/*
 Set the controller to the new AVB state.
 */
OSMetaClassDefineReservedUsed( IOEthernetController,  2);
IOReturn IOEthernetController::setAVBControllerState(IOEthernetControllerAVBState newState)
{
	IOReturn result = kIOReturnSuccess;
	
	setProperty("AVBControllerState", newState, 64);
	
	if(_reserved)
	{
		IOEthernetControllerAVBState oldState = _reserved->fAVBControllerState;
		_reserved->fAVBControllerState = newState;
		
		IOLockLock(_reserved->fStateChangeNotifiersLock);
		
		unsigned int count = _reserved->fStateChangeNotifiers->getCount();
		for(unsigned int index = 0; index < count; index++)
		{
			IOECStateNotifier *stateNotifier = OSDynamicCast(IOECStateNotifier, _reserved->fStateChangeNotifiers->getObject(index));
			
			if(stateNotifier)
			{
				stateNotifier->notify(oldState, newState);
			}
		}
		
		IOLockUnlock(_reserved->fStateChangeNotifiersLock);
		
		if((kIOEthernetControllerAVBStateAVBEnabled == newState || kIOEthernetControllerAVBStateTimeSyncEnabled == newState) && (kIOEthernetControllerAVBStateAVBEnabled != oldState && kIOEthernetControllerAVBStateTimeSyncEnabled != oldState))
		{
			kern_return_t err;
			_reserved->fTimeSyncCallbackThreadShouldKeepRunning = true;
			
			err = kernel_thread_start(&timeSyncCallbackThreadEntry, this, &_reserved->fTimeSyncCallbackThread);
			if(!err)
			{
				semaphore_wait(_reserved->fTimeSyncCallbackStartSemaphore);
			}
		}
		else if((kIOEthernetControllerAVBStateAVBEnabled == oldState || kIOEthernetControllerAVBStateTimeSyncEnabled == oldState) && (kIOEthernetControllerAVBStateAVBEnabled != newState && kIOEthernetControllerAVBStateTimeSyncEnabled != newState))
		{
			bool wasRunning = _reserved->fTimeSyncCallbackThreadIsRunning;
			
			_reserved->fTimeSyncCallbackThreadShouldKeepRunning = false;
			
			if(wasRunning)
			{
				semaphore_wait(_reserved->fTimeSyncCallbackStopSemaphore);
			}
		}
	}
	
	return result;
}

#pragma mark Realtime Transmit
/*
 Get the minimum amount of time required between when transmitRealtimePacket() is called and the launch timestamp.
 */
uint64_t IOEthernetController::getTransmitQueuePacketLatency(uint32_t queueIndex) const
{
	uint64_t result = UINT64_MAX;
	
	if(_reserved)
	{
		if(queueIndex < _reserved->fNumberOfRealtimeTransmitQueues && _reserved->fTransmitQueuePacketLatency)
		{
			result = _reserved->fTransmitQueuePacketLatency[queueIndex];
		}
	}
	
	return result;
}

/*
 Get the maximum amount of time required between when NIC will DMA the packet contents and the launch timestamp.
 */
uint64_t IOEthernetController::getTransmitQueuePrefetchDelay(uint32_t queueIndex) const
{
	uint64_t result = UINT64_MAX;
	
	if(_reserved)
	{
		if(queueIndex < _reserved->fNumberOfRealtimeTransmitQueues && _reserved->fTransmitQueuePrefetchDelay)
		{
			result = _reserved->fTransmitQueuePrefetchDelay[queueIndex];
		}
	}
	
	return result;
}

/*
 Transmit an AVB packet on a realtime transmit queue.
 */
OSMetaClassDefineReservedUsed( IOEthernetController,  3);
IOReturn IOEthernetController::transmitRealtimePackets(uint32_t queueIndex, IOEthernetAVBPacket **packets, uint32_t packetCount, bool commonTimestamp, uint32_t *successfulPacketCount)
{
	if(successfulPacketCount)
	{
		*successfulPacketCount = 0;
	}
	return kIOReturnUnsupported;
}

/*
 Cleanup the realtime transmit queue synchronously with AVB adding frames.
 */
OSMetaClassDefineReservedUsed( IOEthernetController,  4);
IOReturn IOEthernetController::cleanupTransmitQueue(uint32_t queueIndex)
{
	return kIOReturnSuccess;
}

/*
 Sets up the realtime transmit queue count in the AVB support information.
 */
void IOEthernetController::setNumberOfRealtimeTransmitQueues(uint32_t numberOfTransmitQueues)
{
	if(_reserved)
	{
		if(_reserved->fTransmitQueuePacketLatency)
		{
			IOFree(_reserved->fTransmitQueuePacketLatency, sizeof(uint64_t) * _reserved->fNumberOfRealtimeTransmitQueues);
			_reserved->fTransmitQueuePacketLatency = NULL;
		}
		if(_reserved->fTransmitQueuePrefetchDelay)
		{
			IOFree(_reserved->fTransmitQueuePrefetchDelay, sizeof(uint64_t) * _reserved->fNumberOfRealtimeTransmitQueues);
			_reserved->fTransmitQueuePrefetchDelay = NULL;
		}
		_reserved->fNumberOfRealtimeTransmitQueues = numberOfTransmitQueues;
		if(_reserved->fNumberOfRealtimeTransmitQueues)
		{
			_reserved->fTransmitQueuePacketLatency = (uint64_t *)IOMalloc(sizeof(uint64_t) * _reserved->fNumberOfRealtimeTransmitQueues);
			
			if(_reserved->fTransmitQueuePacketLatency)
			{
				for(uint32_t index = 0 ; index < _reserved->fNumberOfRealtimeTransmitQueues; index++)
				{
					_reserved->fTransmitQueuePacketLatency[index] = UINT64_MAX;
				}
			}
			
			_reserved->fTransmitQueuePrefetchDelay = (uint64_t *)IOMalloc(sizeof(uint64_t) * _reserved->fNumberOfRealtimeTransmitQueues);
			
			if(_reserved->fTransmitQueuePrefetchDelay)
			{
				for(uint32_t index = 0 ; index < _reserved->fNumberOfRealtimeTransmitQueues; index++)
				{
					_reserved->fTransmitQueuePrefetchDelay[index] = UINT64_MAX;
				}
			}
		}
	}
}

/*
 Set the value returned by getTransmitQueuePacketLatency() for a given queue.
 */
void IOEthernetController::setTransmitQueuePacketLatency(uint32_t queueIndex, uint64_t packetLatency)
{
	if(_reserved)
	{
		if(queueIndex < _reserved->fNumberOfRealtimeTransmitQueues && _reserved->fTransmitQueuePacketLatency)
		{
			_reserved->fTransmitQueuePacketLatency[queueIndex] = packetLatency;
		}
	}
}

/*
 Set the value returned by getTransmitQueuePrefetchDelay() for a given queue.
 */
void IOEthernetController::setTransmitQueuePrefetchDelay(uint32_t queueIndex, uint64_t prefetchDelay)
{
	if(_reserved)
	{
		if(queueIndex < _reserved->fNumberOfRealtimeTransmitQueues && _reserved->fTransmitQueuePrefetchDelay)
		{
			_reserved->fTransmitQueuePrefetchDelay[queueIndex] = prefetchDelay;
		}
	}
}

#pragma mark Realtime Receive
/*
 Set the filter being used for a receive queue
 */

OSMetaClassDefineReservedUsed( IOEthernetController,  5);
IOReturn IOEthernetController::setRealtimeReceiveQueueFilter(uint32_t queueIndex, IOEthernetAVBIngressFilterElement *filterElements, uint32_t filterElementCount)
{
	return kIOReturnUnsupported;
}

/*
 Get the filter being used for a receive queue
 */
OSMetaClassDefineReservedUsed( IOEthernetController,  6);
IOReturn IOEthernetController::getRealtimeReceiveQueueFilter(uint32_t queueIndex, IOEthernetAVBIngressFilterElement *filterElements, uint32_t *filterElementCount)
{
	return kIOReturnUnsupported;
}

/*
 Set the packet handler callback function for a realtime receive queue.
 */
OSMetaClassDefineReservedUsed( IOEthernetController,  7);
IOReturn IOEthernetController::setRealtimeReceiveQueuePacketHandler(uint32_t queueIndex, avb_packet_callback_t callback, void *context)
{
	return kIOReturnUnsupported;
}

/*! @function setRealtimeReceiveDestinationMACList
 @abstract Set the list of destination MAC addreses used for a realtime receive queue.
 @discussion Set the list of destination MAC addresses that are being received on a realtime receive queue. These multicast
 addresses are *not* included in the list supplied to the setMulticastList call.
 @param queueIndex index of the realtime receive queue.
 @param addresses An array of ethernet destination MAC addresses. This shall be ignored if addressCount is 0.
 @param addressCount The number of elements in the addresses array.
 @return IOReturn indicating success or reason for failure.
 */
OSMetaClassDefineReservedUsed( IOEthernetController,  8);
IOReturn IOEthernetController::setRealtimeReceiveDestinationMACList(uint32_t queueIndex, IOEthernetAddress *addresses, int addressCount)
{
	return kIOReturnUnsupported;
}

/*
 Sets up the realtime receive queue count in the AVB support information.
 */
void IOEthernetController::setNumberOfRealtimeReceiveQueues(uint32_t numberOfReceiveQueues)
{
	if(_reserved)
	{
		_reserved->fNumberOfRealtimeReceiveQueues = numberOfReceiveQueues;
	}
}

#pragma mark Time Sync
/*
 Add a callback function to the list of Time Sync receive callbacks.
 */
IOReturn IOEthernetController::addTimeSyncReceivePacketHandler(avb_packet_callback_t callback, void *context)
{
	IOReturn result = kIOReturnInternalError;
	
	if(_reserved)
	{
		if(callback)
		{
			result = kIOReturnError;
			
			IOLockLock(_reserved->fTimeSyncReceiveHandlersLock);
			
			bool found = false;
			
			unsigned int count = _reserved->fTimeSyncReceiveHandlers->getCount();
			for(unsigned int index = 0; index < count; index++)
			{
				IOECTimeSyncHandler *handler = OSDynamicCast(IOECTimeSyncHandler, _reserved->fTimeSyncReceiveHandlers->getObject(index));
				
				if(handler)
				{
					if(handler->isCallbackAndContext(callback, context))
					{
						found = true;
						break;
					}
				}
			}
			
			if(!found)
			{
				IOECTimeSyncHandler *handler = OSTypeAlloc(IOECTimeSyncHandler);
				
				if(handler)
				{
					if(handler->init(callback, context, 0))
					{
						_reserved->fTimeSyncReceiveHandlers->setObject(handler);
						result = kIOReturnSuccess;
					}
					handler->release();
				}
				else
				{
					result = kIOReturnNoMemory;
				}
			}
			else
			{
				result = kIOReturnExclusiveAccess;
			}
			
			IOLockUnlock(_reserved->fTimeSyncReceiveHandlersLock);
		}
		else
		{
			result = kIOReturnBadArgument;
		}
	}
	
	return result;
}

/*
 Remove a callback function that was previously added with addTimeSyncReceivePacketHandler().
 */
IOReturn IOEthernetController::removeTimeSyncReceivePacketHandler(avb_packet_callback_t callback, void *context)
{
	IOReturn result = kIOReturnInternalError;
	
	if(_reserved)
	{
		if(_reserved->fTimeSyncReceiveHandlersLock && _reserved->fTimeSyncReceiveHandlers)
		{
			if(callback)
			{
				result = kIOReturnNotFound;
				
				IOLockLock(_reserved->fTimeSyncReceiveHandlersLock);
				
				unsigned int count = _reserved->fTimeSyncReceiveHandlers->getCount();
				for(unsigned int index = 0; index < count; index++)
				{
					IOECTimeSyncHandler *handler = OSDynamicCast(IOECTimeSyncHandler, _reserved->fTimeSyncReceiveHandlers->getObject(index));
					
					if(handler)
					{
						if(handler->isCallbackAndContext(callback, context))
						{
							_reserved->fTimeSyncReceiveHandlers->removeObject(index);
							result = kIOReturnSuccess;
							break;
						}
					}
				}
				
				IOLockUnlock(_reserved->fTimeSyncReceiveHandlersLock);
			}
			else
			{
				result = kIOReturnBadArgument;
			}
		}
	}
	
	return result;
}

/*
 Add a callback function to be called after transmitting an egress timestamped Time Sync packet.
 */
IOReturn IOEthernetController::addTimeSyncTransmitPacketHandler(avb_packet_callback_t callback, void *context, uint32_t *callbackRef)
{
	IOReturn result = kIOReturnInternalError;
	
	if(_reserved)
	{
		if(callback && callbackRef)
		{
			if(_reserved->fHasTimeSyncTransmitCallbackIDAvailable)
			{
				result = kIOReturnError;
				
				IOLockLock(_reserved->fTimeSyncTransmitHandlersLock);
				
				bool found = false;
				
				unsigned int count = _reserved->fTimeSyncTransmitHandlers->getCount();
				for(unsigned int index = 0; index < count; index++)
				{
					IOECTimeSyncHandler *handler = OSDynamicCast(IOECTimeSyncHandler, _reserved->fTimeSyncTransmitHandlers->getObject(index));
					
					if(handler)
					{
						if(handler->isCallbackAndContext(callback, context))
						{
							found = true;
							break;
						}
					}
				}
				
				if(!found)
				{
					IOECTimeSyncHandler *handler = OSTypeAlloc(IOECTimeSyncHandler);
					
					if(handler)
					{
						if(handler->init(callback, context, _reserved->fNextTimeSyncTransmitCallbackID))
						{
							_reserved->fTimeSyncTransmitHandlers->setObject(handler);
							result = kIOReturnSuccess;
							*callbackRef = _reserved->fNextTimeSyncTransmitCallbackID;
							
							if(UINT32_MAX == _reserved->fTimeSyncTransmitHandlers->getCount())
							{
								_reserved->fNextTimeSyncTransmitCallbackID++;
								_reserved->fHasTimeSyncTransmitCallbackIDAvailable = false;
							}
							else
							{
								bool inUse = true;
								while(inUse)
								{
									inUse = false;
									_reserved->fNextTimeSyncTransmitCallbackID++;
									count = _reserved->fTimeSyncTransmitHandlers->getCount();
									for(unsigned int index = 0; index < count; index++)
									{
										IOECTimeSyncHandler *candidate = OSDynamicCast(IOECTimeSyncHandler, _reserved->fTimeSyncTransmitHandlers->getObject(index));
										
										if(candidate)
										{
											if(candidate->callbackID() == _reserved->fNextTimeSyncTransmitCallbackID)
											{
												inUse = true;
												break;
											}
										}
									}
								}
							}
						}
						handler->release();
					}
					else
					{
						result = kIOReturnNoMemory;
					}
				}
				else
				{
					result = kIOReturnExclusiveAccess;
				}
				
				IOLockUnlock(_reserved->fTimeSyncTransmitHandlersLock);
			}
			else
			{
				result = kIOReturnNoResources;
			}
		}
		else
		{
			result = kIOReturnBadArgument;
		}
	}
	
	return result;
}

/*
 Remove a callback function previously added with addTimeSyncTransmitPacketHandler()
 */
IOReturn IOEthernetController::removeTimeSyncTransmitPacketHandler(uint32_t callbackRef)
{
	IOReturn result = kIOReturnInternalError;
	
	if(_reserved)
	{
		if(_reserved->fTimeSyncTransmitHandlersLock && _reserved->fTimeSyncTransmitHandlers)
		{
			result = kIOReturnNotFound;
			
			IOLockLock(_reserved->fTimeSyncTransmitHandlersLock);
			
			unsigned int count = _reserved->fTimeSyncTransmitHandlers->getCount();
			for(unsigned int index = 0; index < count; index++)
			{
				IOECTimeSyncHandler *handler = OSDynamicCast(IOECTimeSyncHandler, _reserved->fTimeSyncTransmitHandlers->getObject(index));
				
				if(handler)
				{
					if(handler->callbackID() == callbackRef)
					{
						_reserved->fTimeSyncTransmitHandlers->removeObject(index);
						_reserved->fHasTimeSyncTransmitCallbackIDAvailable = true;
						result = kIOReturnSuccess;
						break;
					}
				}
			}
			
			IOLockUnlock(_reserved->fTimeSyncTransmitHandlersLock);
		}
	}
	
	return result;
}

/*
 Transmit a time sync packet and capture it's egress timestamp.
 */
OSMetaClassDefineReservedUsed( IOEthernetController,  9);
IOReturn IOEthernetController::transmitTimeSyncPacket(IOEthernetAVBPacket * packet, uint64_t expiryTime)
{
	return kIOReturnUnsupported;
}


/*
 Set the gPTP present flag on the controller and trigger the AVB stack loading.
 */
IOReturn IOEthernetController::setGPTPPresent(bool gPTPPresent)
{
	IOReturn result = kIOReturnSuccess;
	
	if(_reserved)
	{
		if(gPTPPresent != _reserved->fgPTPPresent)
		{
			setProperty("gPTPPresent", gPTPPresent);
			
			if(gPTPPresent)
			{
				registerService();
			}
			
			messageClients(kIOMessageServicePropertyChange);
			
			_reserved->fgPTPPresent = gPTPPresent;
		}
	}
	
	return result;
}

/*
 Send the received time sync packet to the callback functions.
 */
void IOEthernetController::receivedTimeSyncPacket(IOEthernetAVBPacket *packet)
{
	if(_reserved && packet)
	{
		IOECTSCallbackEntry *callback = (IOECTSCallbackEntry *)IOMalloc(sizeof(IOECTSCallbackEntry));
		
		if(callback)
		{
			callback->packet = packet;
			callback->next = NULL;
			
			IOLockLock(_reserved->fTimeSyncReceiveCallbackQueueLock);
			
			if(_reserved->fTimeSyncReceiveCallbackQueue)
			{
				IOECTSCallbackEntry *potential = _reserved->fTimeSyncReceiveCallbackQueue;
				while(potential->next)
				{
					potential = potential->next;
				}
				potential->next = callback;
			}
			else
			{
				_reserved->fTimeSyncReceiveCallbackQueue = callback;
			}
			
			IOLockUnlock(_reserved->fTimeSyncReceiveCallbackQueueLock);
			
			semaphore_signal(_reserved->fTimeSyncCallbackQueueSemaphore);
		}
		else
		{
			completeAVBPacket(packet);
		}
		
		setGPTPPresent(true);
	}
}

/*
 Send the transmitted time sync packet to the transmit callback.
 */
void IOEthernetController::transmittedTimeSyncPacket(IOEthernetAVBPacket *packet, bool expired)
{
	//This is called from a secondary interrupt context so we need to get the callback off of this thread and let it continue.
	
	if(_reserved && packet)
	{
		IOECTSCallbackEntry *callback = (IOECTSCallbackEntry *)IOMalloc(sizeof(IOECTSCallbackEntry));
		
		if(callback)
		{
			callback->packet = packet;
			callback->next = NULL;
			
			IOLockLock(_reserved->fTimeSyncTransmitCallbackQueueLock);
			
			if(_reserved->fTimeSyncTransmitCallbackQueue)
			{
				IOECTSCallbackEntry *potential = _reserved->fTimeSyncTransmitCallbackQueue;
				while(potential->next)
				{
					potential = potential->next;
				}
				potential->next = callback;
			}
			else
			{
				_reserved->fTimeSyncTransmitCallbackQueue = callback;
			}
			
			IOLockUnlock(_reserved->fTimeSyncTransmitCallbackQueueLock);
			
			semaphore_signal(_reserved->fTimeSyncCallbackQueueSemaphore);
		}
		else
		{
			completeAVBPacket(packet);
		}
	}
}

/*
 Sets up the time sync support in the AVB support information
 */
void IOEthernetController::setTimeSyncPacketSupport(IOEthernetControllerAVBTimeSyncSupport timeSyncPacketSupport)
{
	if(_reserved)
	{
		_reserved->fTimeSyncSupport = timeSyncPacketSupport;
	}
}

/*
 Call the packet's completion callback to hand the packet back to the allocator of the packet for reuse or destruction.
 */
void IOEthernetController::completeAVBPacket(IOEthernetAVBPacket *packet)
{
	if(packet && packet->completion_callback)
	{
		packet->completion_callback(packet->completion_context, packet);
	}
}

/*
 Allocate a packet from the AVB packet pool.
 */
IOEthernetController::IOEthernetAVBPacket * IOEthernetController::allocateAVBPacket(bool fromRealtimePool)
{
	IOEthernetAVBPacket *result = NULL;
	
	if(_reserved)
	{
		if(fromRealtimePool)
		{
			//TBD: This still needs to be written
		}
		else
		{
			bool failed = true;
			bool prepared = false;
			IOReturn status;
			IOBufferMemoryDescriptor *buffer;
			
			
			vm_size_t maxPacketSize = 2000;
			vm_size_t allocationSize = maxPacketSize;
			vm_offset_t alignment = 1;
			IOOptionBits options = kIODirectionOutIn | kIOMemoryPhysicallyContiguous |
			kIOMapWriteThruCache;
			
			result = (IOEthernetAVBPacket *)IOMalloc(sizeof(IOEthernetAVBPacket));
			require(result, FailedAllocate);
			
			memset(result, 0, sizeof(IOEthernetAVBPacket));
			
			result->completion_context = this;
			result->completion_callback = &allocatedAVBPacketCompletion;
			
			if(_reserved->fAVBPacketMapper)
			{
				vm_offset_t pageSize = PAGE_SIZE;
				
				if(allocationSize % pageSize)
				{
					allocationSize += (pageSize - (allocationSize % pageSize));
				}
				
				if(alignment < pageSize)
				{
					alignment = pageSize;
				}
			}
			
			buffer = IOBufferMemoryDescriptor::withOptions(options, allocationSize, alignment);
			require(buffer, FailedAllocate);
			result->desc_buffer = buffer;
			
			status = ((IOBufferMemoryDescriptor *)(result->desc_buffer))->prepare();
			require(kIOReturnSuccess == status, FailedAllocate);
			prepared = true;
			
			result->numberOfEntries = 1;
			result->virtualRanges[0].address = (IOVirtualAddress)buffer->getBytesNoCopy();
			result->virtualRanges[0].length = maxPacketSize;
			result->physicalRanges[0].address = buffer->getPhysicalAddress();
			result->physicalRanges[0].length = maxPacketSize;
			
			failed = false;
			
		FailedAllocate:
			if(failed && result)
			{
				if(result->desc_buffer)
				{
					if(prepared)
					{
						((IOBufferMemoryDescriptor *)(result->desc_buffer))->complete();
					}
					((IOBufferMemoryDescriptor *)(result->desc_buffer))->release();
				}
				
				IOFree(result, sizeof(IOEthernetAVBPacket));
				result = NULL;
			}
		}
	}
	
	return result;
}



void IOEthernetController::allocatedAVBPacketCompletion(void *context, IOEthernetAVBPacket *packet)
{
	if(packet)
	{
		if(packet->desc_buffer)
		{
			((IOBufferMemoryDescriptor *)(packet->desc_buffer))->complete();
			((IOBufferMemoryDescriptor *)(packet->desc_buffer))->release();
		}
		
		IOFree(packet, sizeof(IOEthernetAVBPacket));
	}
}

void IOEthernetController::realtimePoolAVBPacketCompletion(IOEthernetAVBPacket *packet)
{
	//TBD: this still needs to be written
}

#pragma mark TimeSync Callback Queue

void IOEthernetController::timeSyncCallbackThreadEntry(void *param, wait_result_t waitResult)
{
	IOEthernetController *controller = (IOEthernetController *)param;
	
	controller->timeSyncCallbackThread();
}

void IOEthernetController::timeSyncCallbackThread()
{
	if(_reserved)
	{
		retain();
		
		_reserved->fTimeSyncCallbackThreadIsRunning = true;
		
		semaphore_signal(_reserved->fTimeSyncCallbackStartSemaphore);
		
		while (_reserved->fTimeSyncCallbackThreadShouldKeepRunning)
		{
			if(KERN_SUCCESS == semaphore_wait_deadline(_reserved->fTimeSyncCallbackQueueSemaphore, mach_absolute_time()+_reserved->fTimeSyncCallbackTimeoutTime))
			{
				bool processedTransmit = false;
				
				{
					struct IOECTSCallbackEntry *callback = NULL;
					
					IOLockLock(_reserved->fTimeSyncTransmitCallbackQueueLock);
					
					if(_reserved->fTimeSyncTransmitCallbackQueue)
					{
						callback = _reserved->fTimeSyncTransmitCallbackQueue;
						_reserved->fTimeSyncTransmitCallbackQueue = _reserved->fTimeSyncTransmitCallbackQueue->next;
						processedTransmit = true;
					}
					
					IOLockUnlock(_reserved->fTimeSyncTransmitCallbackQueueLock);
					
					if(callback)
					{
						IOEthernetAVBPacket *packet = callback->packet;
						
						IOLockLock(_reserved->fTimeSyncTransmitHandlersLock);
						
						unsigned int count = _reserved->fTimeSyncTransmitHandlers->getCount();
						for(unsigned int index = 0; index < count; index++)
						{
							IOECTimeSyncHandler *handler = OSDynamicCast(IOECTimeSyncHandler, _reserved->fTimeSyncTransmitHandlers->getObject(index));
							
							if(handler)
							{
								if(handler->callbackID() == packet->transmittedTimeSyncCallbackRef)
								{
									handler->call(packet);
									break;
								}
							}
						}
						
						IOLockUnlock(_reserved->fTimeSyncTransmitHandlersLock);
						
						completeAVBPacket(packet);
						
						IOFree(callback, sizeof(struct IOECTSCallbackEntry));
					}
				}
				
				if(!processedTransmit)
				{
					struct IOECTSCallbackEntry *callback = NULL;
					
					IOLockLock(_reserved->fTimeSyncReceiveCallbackQueueLock);
					
					if(_reserved->fTimeSyncReceiveCallbackQueue)
					{
						callback = _reserved->fTimeSyncReceiveCallbackQueue;
						_reserved->fTimeSyncReceiveCallbackQueue = _reserved->fTimeSyncReceiveCallbackQueue->next;
					}
					
					IOLockUnlock(_reserved->fTimeSyncReceiveCallbackQueueLock);
					
					if(callback)
					{
						IOEthernetAVBPacket *packet = callback->packet;
						
						IOLockLock(_reserved->fTimeSyncReceiveHandlersLock);
						
						unsigned int count = _reserved->fTimeSyncReceiveHandlers->getCount();
						for(unsigned int index = 0; index < count; index++)
						{
							IOECTimeSyncHandler *handler = OSDynamicCast(IOECTimeSyncHandler, _reserved->fTimeSyncReceiveHandlers->getObject(index));
							
							if(handler)
							{
								handler->call(packet);
							}
						}
						
						IOLockUnlock(_reserved->fTimeSyncReceiveHandlersLock);
						
						completeAVBPacket(packet);
						
						IOFree(callback, sizeof(struct IOECTSCallbackEntry));
					}
				}
			}
		}
		
		_reserved->fTimeSyncCallbackThreadIsRunning = false;
		
		thread_t thread = current_thread();
		
		semaphore_signal(_reserved->fTimeSyncCallbackStopSemaphore);
		
		_reserved->fTimeSyncCallbackThread = NULL;
		
		release();
		
		thread_deallocate(thread);
		thread_terminate(thread);
	}
	else
	{
		thread_t thread = current_thread();
		thread_deallocate(thread);
		thread_terminate(thread);
	}
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef super

#pragma mark Notifiers Helper Class

OSDefineMetaClassAndStructors(IOECStateNotifier, OSObject);

#define super OSObject

bool IOECStateNotifier::init(IOEthernetController::avb_state_callback_t callback, void *context)
{
	fCallback = callback;
	fContext = context;
	
	return super::init();
}

bool IOECStateNotifier::isCallbackAndContext(IOEthernetController::avb_state_callback_t callback, void *context)
{
	return (fCallback == callback) && (fContext == context);
}

void IOECStateNotifier::notify(IOEthernetControllerAVBState oldState, IOEthernetControllerAVBState newState)
{
	fCallback(fContext, oldState, newState);
}

#pragma mark Time Sync Handler Helper Class

OSDefineMetaClassAndStructors(IOECTimeSyncHandler, OSObject);

#define super OSObject

bool IOECTimeSyncHandler::init(IOEthernetController::avb_packet_callback_t callback, void *context, uint32_t callbackID)
{
	fCallback = callback;
	fContext = context;
	fCallbackID = callbackID;
	
	return super::init();
}

bool IOECTimeSyncHandler::isCallbackAndContext(IOEthernetController::avb_packet_callback_t callback, void *context)
{
	return (fCallback == callback) && (fContext == context);
}

void IOECTimeSyncHandler::call(IOEthernetController::IOEthernetAVBPacket *packet)
{
	fCallback(fContext, packet);
}

uint32_t IOECTimeSyncHandler::callbackID()
{
	return fCallbackID;
}




