/*
 * Copyright (c) 1998-2011 Apple Inc. All rights reserved.
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
 * IONetworkStack.cpp - An IOKit proxy for the BSD networking stack.
 *
 * HISTORY
 *
 * IONetworkStack abstracts essential network stack services. These
 * include registering/unregistering network interfaces, and interface
 * name space management.
 *
 * Only a single IONetworkStack object is instantiated. This object will
 * register to receive a notification when a network interface object is
 * first published. The notification handler is responsible for attaching
 * the network stack object to the interface object as a client. When the
 * interface is terminated, this linkage is severed.
 *
 * This object does not participate in the data/packet flow. The interface
 * object will interact directly with DLIL to send and to receive packets.
 */

extern "C" {
#include <net/if.h>
#include <sys/sockio.h>
}

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>  // for setAggressiveness()
#include "IONetworkStack.h"
#include "IONetworkDebug.h"

#define super IOService
OSDefineMetaClassAndFinalStructors( IONetworkStack, IOService )

#define NIF_VAR(n)          ((n)->_clientVar[0])
#define NIF_SET(n, x)       (NIF_VAR(n) |= (x))
#define NIF_CLR(n, x)       (NIF_VAR(n) &= ~(x))
#define NIF_TEST(n, x)      (NIF_VAR(n) & (x))

#define NIF_CAST(x)         ((IONetworkInterface *) x)
#define NIF_SAFECAST(x)     (OSDynamicCast(IONetworkInterface, x))

#define LOCK()              IOLockLock(_stateLock)
#define UNLOCK()            IOLockUnlock(_stateLock)

static IONetworkStack * gIONetworkStack = 0;

// Flags encoded on the interface object.
//
enum {
    kInterfaceStateInactive     = 0x01, // terminated
    kInterfaceStatePublished    = 0x02, // waiting to attach
    kInterfaceStateAttaching    = 0x04, // attaching to BSD
    kInterfaceStateAttached     = 0x08  // attached to BSD
};

// IONetworkStackUserClient definition
//
#include <IOKit/IOUserClient.h>

class IONetworkStackUserClient : public IOUserClient
{
    OSDeclareFinalStructors( IONetworkStackUserClient )

protected:
    IONetworkStack * _provider;

public:
    virtual bool initWithTask(	task_t			owningTask,
                                void *			securityID,
                                UInt32			type,
                                OSDictionary *	properties );
    virtual bool start( IOService * provider );
    virtual IOReturn clientClose( void );
    virtual IOReturn clientDied( void );
    virtual IOReturn setProperties( OSObject * properties );
};

//------------------------------------------------------------------------------

bool IONetworkStack::start( IOService * provider )
{
    OSDictionary *      matching;
    IOService *         rootDomain;
    const OSSymbol *    ucClassName;

    DLOG("IONetworkStack::start(%p) %p\n", provider, this);

    // Only a single IONetworkStack object is created, and a reference
    // to this object is stored in a global variable.
    // When the boot process is VERY VERY slow for some unknown reason
    // we get two instances of IONetworkStack and theassert below fires.
    // so I am commenting the assert and replacing it with an if statement.
    // assert( gIONetworkStack == 0 );

    if (gIONetworkStack)
        goto fail;

    if (super::start(provider) == false)
        goto fail;

    _stateLock = IOLockAlloc();
    if (!_stateLock)
        goto fail;

    _ifListNaming = OSSet::withCapacity(8);
    if (!_ifListNaming)
        goto fail;

    _ifPrefixDict = OSDictionary::withCapacity(4);
    if (!_ifPrefixDict)
        goto fail;

    _asyncThread = thread_call_allocate(
        OSMemberFunctionCast(thread_call_func_t, this,
            &IONetworkStack::asyncWork), this);
    if (!_asyncThread)
        goto fail;

    // Call the IOPMrootDomain object, which sits at the root of the
    // power management hierarchy, to set the default Ethernet WOL
    // settings. Setting the WOL through setAggressiveness rather
    // than on each interface object makes it easy to handle an
    // IOPMSetAggressiveness() call from loginwindow since it will
    // simply override the default value set here.

    if ((rootDomain = (IOService *) getPMRootDomain()))
    {
        rootDomain->IOService::setAggressiveness(kPMEthernetWakeOnLANSettings,
            kIOEthernetWakeOnMagicPacket | kIOEthernetWakeOnPacketAddressMatch);
    }

    // Sign up for a notification when a network interface is first published.

    matching = serviceMatching("IONetworkInterface");
    if (!matching)
        goto fail;

    gIONetworkStack = this;

    _ifNotifier = addMatchingNotification(
            /* type     */ gIOFirstPublishNotification,
            /* match    */ matching,
            /* action   */ OSMemberFunctionCast(
                                    IOServiceMatchingNotificationHandler,
                                    this, &IONetworkStack::interfacePublished),
            /* target   */ this,
            /* refCon   */ this,
            /* priority */ 1000 );

    matching->release();
    if (!_ifNotifier)
        goto fail;

    ucClassName = OSSymbol::withCStringNoCopy("IONetworkStackUserClient");
    if (ucClassName)
    {
        setProperty(gIOUserClientClassKey, (OSObject *) ucClassName);
        ucClassName->release();
    }

    registerService();
    return true;

fail:
    LOG("IONetworkStack::start(%p) %p failed\n", provider, this);
    return false;
}

//------------------------------------------------------------------------------

void IONetworkStack::free( void )
{
    LOG("IONetworkStack::free() %p\n", this);
    
    if (this == gIONetworkStack)
        gIONetworkStack = 0;

    if ( _ifNotifier )
    {
        _ifNotifier->remove();
        _ifNotifier = 0;
    }

    if (_ifPrefixDict)
    {
        _ifPrefixDict->release();
        _ifPrefixDict = 0;
    }

    if (_ifListNaming)
    {
        _ifListNaming->release();
        _ifListNaming = 0;
    }

    if (_ifListDetach)
    {
        _ifListDetach->release();
        _ifListDetach = 0;
    }

    if (_asyncThread)
    {
        thread_call_free(_asyncThread);
        _asyncThread = 0;
    }

    if (_stateLock)
    {
        IOLockFree(_stateLock);
        _stateLock = 0;
    }

    super::free();
}

//------------------------------------------------------------------------------

bool IONetworkStack::insertNetworkInterface( IONetworkInterface * netif )
{
    const char *   prefix = netif->getNamePrefix();
    OSOrderedSet * set;
    bool           ok = true;

    if (prefix == 0)
        return false;

    // Look for a Set object associated with the name prefix.

    set = (OSOrderedSet *) _ifPrefixDict->getObject(prefix);

    // If not found, then create one and add it to the dictionary.

    if (!set && ((set = OSOrderedSet::withCapacity(8, orderNetworkInterfaces))))
    {
        ok = _ifPrefixDict->setObject(prefix, set);
        set->release();
    }

    // Add the interface object to the named set.
    // All objects in a set will have the same name prefix.

    ok = (set && ok) ? set->setObject(netif) : false;

    if (set && (set->getCount() == 0))
    {
        _ifPrefixDict->removeObject(prefix);
    }

    return ok;
}

void IONetworkStack::removeNetworkInterface( IONetworkInterface * netif )
{
    const char *   prefix = netif->getNamePrefix();
    OSOrderedSet * set;

    if ( prefix )
    {
        set = (OSOrderedSet *) _ifPrefixDict->getObject(prefix);
        if ( set )
        {
            // Remove interface from set

            set->removeObject(netif);
            DLOG("IONetworkStack::removeNetworkInterface %s count = %d\n",
                prefix, set->getCount());

            // Remove the empty set from the dictionary

            if (set->getCount() == 0)
                _ifPrefixDict->removeObject(prefix);
        }
    }
}

//------------------------------------------------------------------------------
// Get the next available unit number in the set of registered interfaces
// with the specified prefix.

uint32_t IONetworkStack::getNextAvailableUnitNumber(
    const char *    prefix,
    uint32_t        startingUnit )
{
    IONetworkInterface * netif;
    OSOrderedSet *       set;

    assert(prefix);

    set = (OSOrderedSet *) _ifPrefixDict->getObject(prefix);
    netif = set ? NIF_CAST(set->getLastObject()) : 0;

    if (!netif || (netif->getUnitNumber() < startingUnit))
    {
        // The unit number provided is acceptable.
    }
    else if (netif->getUnitNumber() == startingUnit)
    {
        // Conflict, bump proposed unit number by one.
        startingUnit++;
    }
    else if (set)
    {
        for ( uint32_t index = 0; ; index++ )
        {
            netif = NIF_CAST(set->getObject(index));

            if (!netif || (netif->getUnitNumber() > startingUnit))
                break;
            else if (netif->getUnitNumber() < startingUnit)
                continue;
            else /* == */
                startingUnit = netif->getUnitNumber() + 1;
        }
    }

    return startingUnit;
}

//------------------------------------------------------------------------------

bool IONetworkStack::interfacePublished(
    void *          refCon __unused,
    IOService *     service,
    IONotifier *    notifier __unused )
{
    IONetworkInterface * netif = NIF_SAFECAST(service);
    bool ok = false;

    DLOG("IONetworkStack::interfacePublished(%p)\n", netif);

    if (!netif || !attach(netif))
        return false;

    LOCK();

    do {
        // Must be a new network interface
        if (NIF_VAR(netif) != 0)
            break;

        // Stop if interface is already published
        if (_ifListNaming->containsObject(netif))
        {
            ok = true;
            break;
        }

        _ifListNaming->setObject(netif);

        // Initialize private interface state
        NIF_VAR(netif) = kInterfaceStatePublished;
        ok = true;
    }
    while (false);

    UNLOCK();

    if (!ok)
        detach(netif);

    return ok;
}

//------------------------------------------------------------------------------

void IONetworkStack::asyncWork( void )
{
    IONetworkInterface * netif;

    while (1)
    {
        LOCK();
        if (_ifListDetach)
        {
            netif = NIF_CAST(_ifListDetach->getObject(0));
            if (netif)
            {
                netif->retain();
                _ifListDetach->removeObject(0);
            }
        }
        else
            netif = 0;
        UNLOCK();
        if (!netif)
            break;

        DLOG("IONetworkStack::asyncWork detach %s %p\n",
            netif->getName(), netif);

        // Interface is about to detach from DLIL
        netif->setInterfaceState( 0, kIONetworkInterfaceRegisteredState );

        // detachFromDataLinkLayer will block until BSD detach is complete
        netif->detachFromDataLinkLayer(0, 0);

        LOCK();

        assert(NIF_TEST(netif, kInterfaceStateAttached));
        assert(NIF_TEST(netif, kInterfaceStateInactive));

        NIF_CLR(netif, kInterfaceStateAttached | kInterfaceStateAttaching);

        // Drop interface from list of attached interfaces.
        // Unit number assigned to interface is up for grabs.

        removeNetworkInterface(netif);

        UNLOCK();

        // Close interface and allow it to proceed with termination
        netif->close( this );
        netif->release();
    }
}

//------------------------------------------------------------------------------

bool IONetworkStack::didTerminate(
        IOService * provider, IOOptionBits options, bool * defer )
{
    IONetworkInterface * netif = NIF_SAFECAST(provider);
    bool wakeThread = false;

    DLOG("IONetworkStack::didTerminate(%s %p, 0x%x)\n",
        provider->getName(), provider, (uint32_t) options);

    if (!netif)
        return true;

    LOCK();

    do {
        // Interface has become inactive, it is no longer possible
        // to open or to attach to the interface object.
        // Mark the interface as unfit for naming / BSD attach.

        NIF_SET(netif, kInterfaceStateInactive);
        _ifListNaming->removeObject(netif);

        // Interface is attaching to BSD. Postpone termination until
        // the attachment is complete.

        if (NIF_TEST(netif, kInterfaceStateAttaching))
            break;

        // If interface was never attached to BSD, we are done since
        // we don't have an open on the interface.

        if (NIF_TEST(netif, kInterfaceStateAttached))
        {
            // Detach interface from BSD asynchronously.
            // The interface termination will be waiting for our close.

            if (!_ifListDetach)
                _ifListDetach = OSArray::withCapacity(8);
            if (_ifListDetach)
            {
                _ifListDetach->setObject(netif);
                wakeThread = true;
            }
        }
    }
    while ( false );

    UNLOCK();

    if (wakeThread)
        thread_call_enter(_asyncThread);

    return true;
}

//------------------------------------------------------------------------------

bool IONetworkStack::reserveInterfaceUnitNumber(
    IONetworkInterface *    netif,
    uint32_t                unit,
    bool                    isUnitFixed )
{
    const char *    prefix  = netif->getNamePrefix();
    uint32_t        inUnit  = unit;
    bool            opened  = false;
    bool            ok      = false;

    DLOG("IONetworkStack::reserveInterfaceUnitNumber(%p, %s, %u)\n",
        netif, prefix ? prefix : "", unit);

    LOCK();

    // netif retained by caller
    _ifListNaming->removeObject(netif);

    do {
        if (!prefix)
        {
            LOG("interface name prefix is null\n");
            break;
        }

        // Interface must be in the published state.

        if (NIF_VAR(netif) != kInterfaceStatePublished )
        {
            LOG("unable to name interface in state 0x%x as %s%u\n",
                (uint32_t) NIF_VAR(netif), prefix, inUnit);
            break;
        }

        // The unit argument provided is a hint to indicate the lowest unit
        // number that can be assigned to the interface. We are allowed to
        // increment the unit number provided if the number is already
        // taken.

        unit = getNextAvailableUnitNumber(prefix, unit);
        if ((isUnitFixed == true) && (unit != inUnit))
        {
            LOG("interface name %s%u is unavailable\n", prefix, inUnit);
            break;
        }

        // Open the interface object. This will fail if the interface
        // object has become inactive.

        UNLOCK();
        ok = netif->open(this);
        LOCK();

        if (!ok)
        {
            LOG("interface %s%u open failed\n", prefix, inUnit);
            break;
        }
        opened = true;
        ok = false;

        // Check if preempted by interface termination
        if (NIF_TEST(netif, kInterfaceStateInactive))
        {
            LOG("interface %s%u became inactive\n", prefix, inUnit);
            break;
        }

        // Update interface unit and add the interface to the named
        // collection to reserve its assigned unit number.

        if ((netif->setUnitNumber(unit)    == false) ||
            (insertNetworkInterface(netif) == false))
        {
            LOG("interface %s%u name assigment failed\n", prefix, inUnit);
            break;
        }

        ok = true;
    }
    while ( false );

    if (ok)
        NIF_SET(netif, kInterfaceStateAttaching);

    UNLOCK();

    if (!ok && opened)
    {
        netif->close(this);
    }

    return ok;
}

//------------------------------------------------------------------------------

IOReturn IONetworkStack::attachNetworkInterfaceToBSD( IONetworkInterface * netif )
{
    bool        attachOK     = false;
    bool        detachWakeup = false;
    bool        pingMatching = false;
    char        ifname[32];
    IOReturn    result;

    assert( netif );
    if ((result = netif->attachToDataLinkLayer(0, 0)) == kIOReturnSuccess)
    {
        // Hack to sync up the interface flags. The UP flag may already
        // be set, and this will issue an SIOCSIFFLAGS to the interface.

        ifnet_ioctl(netif->getIfnet(), 0, SIOCSIFFLAGS, 0);

        // Interface is now attached to BSD.
        netif->setInterfaceState( kIONetworkInterfaceRegisteredState );

        // Add a kIOBSDNameKey property to the interface AFTER the interface
        // has attached to BSD. The order is very important to avoid rooting
        // from an interface which is not yet known by BSD.

        snprintf(ifname, sizeof(ifname), "%s%u",
            netif->getNamePrefix(), netif->getUnitNumber());
        netif->setProperty(kIOBSDNameKey, ifname);
        attachOK = true;
    }

    // Update state bits and detect for untimely interface termination.

    LOCK();
    assert(( NIF_VAR(netif) &
           ( kInterfaceStateAttaching | kInterfaceStateAttached )) ==
             kInterfaceStateAttaching );
    NIF_CLR( netif, kInterfaceStateAttaching );

    if (attachOK)
    {
        NIF_SET( netif, kInterfaceStateAttached  );
        pingMatching = true;

        if (NIF_TEST(netif, kInterfaceStateInactive))
        {
            if (!_ifListDetach)
                _ifListDetach = OSArray::withCapacity(8);
            if (_ifListDetach)
            {
                _ifListDetach->setObject(netif);
                detachWakeup = true;
            }
            pingMatching = false;
        }
    }
    else
    {
        // BSD attach failed, drop from list of attached interfaces
        removeNetworkInterface(netif);
    }

    UNLOCK();

    if (!attachOK)
    {
        netif->close( this );
        detach(netif);
    }

    if (detachWakeup)
    {
        // In the event that an interface was terminated while attaching
        // to BSD, re-schedule the BSD detach and interface close.
        thread_call_enter(_asyncThread);
    }

    if (pingMatching)
    {
        // Re-register interface after the interface has attached to BSD.
        netif->registerService();
    }

    return result;
}

//------------------------------------------------------------------------------

IOReturn IONetworkStack::registerAllNetworkInterfaces( void )
{
    IONetworkInterface *    netif;
    OSSet *                 list;

    LOCK();

    if (!_ifListNaming || (_ifListNaming->getCount() == 0))
    {
        UNLOCK();
        return kIOReturnSuccess;
    }

    list = OSSet::withSet( _ifListNaming );

    UNLOCK();

    if (list == 0)
    {
        return kIOReturnNoMemory;
    }

    while ((netif = NIF_CAST(list->getAnyObject())))
    {
        if (reserveInterfaceUnitNumber( netif, 0, false ))
            attachNetworkInterfaceToBSD( netif );

        list->removeObject(netif);
    }

    list->release();   

    return kIOReturnSuccess;
}

//------------------------------------------------------------------------------

IOReturn IONetworkStack::registerNetworkInterface(
    IONetworkInterface * netif,
    uint32_t             unit,
    bool                 isUnitFixed )
{
    IOReturn result = kIOReturnNoSpace;

    if (reserveInterfaceUnitNumber( netif, unit, isUnitFixed ))
        result = attachNetworkInterfaceToBSD( netif );

    return result;
}

//------------------------------------------------------------------------------
// Registered interfaces are ordered by their assigned unit number. Those with
// larger unit numbers will be placed behind those with smaller unit numbers.
// This ordering makes it easier to hunt for an available unit number slot for
// a new interface.

SInt32 IONetworkStack::orderNetworkInterfaces(
    const OSMetaClassBase * obj1,
    const OSMetaClassBase * obj2,
    void *                  ref )
{
    const IONetworkInterface * netif1 = (const IONetworkInterface *) obj1;
    const IONetworkInterface * netif2 = (const IONetworkInterface *) obj2;

    assert( netif1 && netif2 );
    return (netif2->getUnitNumber() - netif1->getUnitNumber());
}

//------------------------------------------------------------------------------

IOReturn IONetworkStack::setProperties( OSObject * properties )
{
    IONetworkInterface *    netif = 0;
    OSDictionary *          dict = OSDynamicCast(OSDictionary, properties);
    OSString *              path;
    OSNumber *              num;
    OSNumber *              unit;
    OSData *                data;
    uint32_t                cmdType;
    IOReturn                error = kIOReturnBadArgument;

    do {
        if (!dict)
            break;

        // [optional] Type of interface naming operation
        // IORegisterNetworkInterface() does not set this for netboot case.

        num = OSDynamicCast( OSNumber,
                             dict->getObject(kIONetworkStackUserCommandKey));
        if (num)
            cmdType = num->unsigned32BitValue();
        else
            cmdType = kIONetworkStackRegisterInterfaceWithLowestUnit;

        if (kIONetworkStackRegisterInterfaceAll == cmdType)
        {
            error = registerAllNetworkInterfaces();
            break;
        }

        // [required] Interface unit number
        unit = OSDynamicCast(OSNumber, dict->getObject(kIOInterfaceUnit));
        if (!unit)
            break;

        // [optional] Registry entry ID for interface object
        data = OSDynamicCast(OSData, dict->getObject(kIORegistryEntryIDKey));

        // [optional] Device path to interface objecy
        path = OSDynamicCast(OSString, dict->getObject(kIOPathMatchKey));

        if (data && (data->getLength() == sizeof(uint64_t)))
        {
            OSDictionary *  matching;
            OSIterator *    iter;
            uint64_t        entryID = *(uint64_t *) data->getBytesNoCopy();

            matching = registryEntryIDMatching(entryID);
            if (!matching)
            {
                error = kIOReturnNoMemory;
                break;
            }

            iter = getMatchingServices(matching);
            matching->release();
            if (!iter)
            {
                error = kIOReturnNotFound;
                break;
            }

            if (iter)
            {
                netif = OSDynamicCast(IONetworkInterface, iter->getNextObject());
                if (netif)
                    netif->retain();
                iter->release();
            }
        }
        else if (path)
        {
            IORegistryEntry * entry;

            entry = IORegistryEntry::fromPath(path->getCStringNoCopy());
            if (entry && OSDynamicCast(IONetworkInterface, entry))
                netif = (IONetworkInterface *) entry;
            else if (entry)
                entry->release();
        }
        else
        {
            // no path nor registry entry ID provided
            break;
        }

        if (!netif)
        {
            error = kIOReturnNoDevice;
            break;
        }

        switch ( cmdType )
        {
            case kIONetworkStackRegisterInterfaceWithUnit:
                error = registerNetworkInterface(
                            netif,
                            unit->unsigned32BitValue(),
                            true  /* fixedUnit   */ );
                break;

            case kIONetworkStackRegisterInterfaceWithLowestUnit:
                error = registerNetworkInterface(
                            netif,
                            unit->unsigned32BitValue(),
                            false /* fixedUnit   */ );
                break;

            default:
                error = kIOReturnUnsupported;
                break;
        }
    } while (false);

    if (netif)
        netif->release();

    return error;
}

//------------------------------------------------------------------------------
// IONetworkStackUserClient

#undef  super
#define super IOUserClient
OSDefineMetaClassAndFinalStructors( IONetworkStackUserClient, IOUserClient )

bool IONetworkStackUserClient::initWithTask(	task_t			owningTask,
                                                void *			securityID,
                                                UInt32			type,
                                                OSDictionary *	properties )
{
	if (!super::initWithTask(owningTask, securityID, type, properties))
		return false;

	if (IOUserClient::clientHasPrivilege(
		securityID, kIOClientPrivilegeAdministrator) != kIOReturnSuccess)
		return false;

    return true;
}

bool IONetworkStackUserClient::start( IOService * provider )
{
    if ( super::start(provider) == false )
        return false;

    if ( provider->open(this) == false )
        return false;

    _provider = OSDynamicCast(IONetworkStack, provider);
    if (!_provider)
    {
        provider->close(this);
        return false;
    }

    return true;
}

IOReturn IONetworkStackUserClient::clientClose( void )
{
    if (_provider)
    {
        _provider->close(this);
        detach(_provider);
    }
    return kIOReturnSuccess;
}

IOReturn IONetworkStackUserClient::clientDied( void )
{
    return clientClose();
}

IOReturn IONetworkStackUserClient::setProperties( OSObject * properties )
{
    if (kIOReturnSuccess != IOUserClient::clientHasPrivilege(
        current_task(), kIOClientPrivilegeAdministrator))
    {
        return kIOReturnNotPrivileged;
    }

    return ( _provider ) ?
        _provider->setProperties( properties ) :
        kIOReturnNotReady;
}
