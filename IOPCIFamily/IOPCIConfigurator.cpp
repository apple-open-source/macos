/*
 * Copyright (c) 2004-2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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

#include <IOKit/assert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/pci/IOPCIConfigurator.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/pci/IOPCIPrivate.h>
#include <libkern/sysctl.h>

__BEGIN_DECLS

#if defined(__i386__) || defined(__x86_64__)

#include <i386/cpuid.h>
#include <i386/cpu_number.h>

extern void mp_rendezvous_no_intrs(
               void (*action_func)(void *),
               void *arg);
#else

#define NO_RENDEZVOUS_KERNEL	1
#define cpu_number()	(0)

#endif

__END_DECLS

#define DLOGC(configurator, fmt, args...)                  \
    do {                                    \
        if (configurator->fFlags & kIOPCIConfiguratorIOLog)   \
            IOLog(fmt, ## args);            \
        if (configurator->fFlags & kIOPCIConfiguratorKPrintf) \
            kprintf(fmt, ## args);          \
    } while(0)


#define DLOG(fmt, args...)	DLOGC(this, fmt, ## args);

static const char * gPCIResourceTypeName[kIOPCIResourceTypeCount] =
{
    "MEM", "PFM", "I/O", "BUS"
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define CLASS IOPCIConfigurator
#define super IOService

OSDefineMetaClassAndStructors( IOPCIConfigurator, IOService )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool CLASS::start( IOService * provider )
{
    uint64_t cacheLineSize;
    size_t   siz;

    fRootBridge = OSDynamicCast(IOPCIBridge, provider);
    if (!fRootBridge) return false;

    super::start(provider);

    // Fetch resources assigned to the root bridge.
    if (!createRoot(fRootBridge))
        return false;

    fFlags = kIOPCIConfiguratorEnable;

    if (kPCIHotPlug == fPCIBridgeList[0]->supportsHotPlug)
    {
	fFlags |= 0
	       | kIOPCIConfiguratorAllocate
//	       | kIOPCIConfiguratorIOLog | kIOPCIConfiguratorKPrintf
	       ;
	fCacheLineSize = 0x40;
	if (provider->getProperty(kIOPCIResetKey))
	{
	    provider->removeProperty(kIOPCIResetKey);
	    fFlags |= kIOPCIConfiguratorReset;
	}
    }
    else
    {
	siz = sizeof(cacheLineSize);
	if (0 != sysctlbyname("hw.cachelinesize", &cacheLineSize, &siz, 0, 0))
	    cacheLineSize = 32;
	fCacheLineSize = cacheLineSize >> 2;
    }

    DLOG("PCI cache line size = %u bytes\n", fCacheLineSize);

    DLOG("[ PCI configuration begin ]\n");
    checkPCIConfiguration();
    DLOG("[ PCI configuration end ]\n");

    if (fBridgeConfigCount || fDeviceConfigCount || fYentaConfigCount)
    {
        IOLog("PCI configuration changed (bridge=%d device=%d yenta=%d)\n",
              fBridgeConfigCount, fDeviceConfigCount, fYentaConfigCount);
    }

    provider->setProperty(kIOPCIConfiguredKey, kOSBooleanTrue);

    return false;   // terminate and unload module
}

void CLASS::free( void )
{
    pci_dev_t bridge;
    pci_dev_t child;

    DLOG("ApplePCIConfigurator::free(%p)\n", this);

    for (int i = 0; i < kIOPCIResourceTypeCount; i++)
    {
	IOPCIRange *
	range = fRootBridge->reserved->rangeLists[i];
	if (range)
	{
	    range->subRange     = NULL;
	    range->nextSubRange = NULL;
	}
    }

    for (int i = 0; i < kPCIBridgeMaxCount; i++)
    {
        if ((bridge = fPCIBridgeList[i]))
        {
            for (child = bridge->child; child; )
            {
                pci_dev_t lastChild = child;
                child = child->peer;
                if (!lastChild->isBridge)
		{
                    IODelete(lastChild, pci_dev, 1);
		}
            }
            IODelete(bridge, pci_dev, 1);
            fPCIBridgeList[i] = 0;
        }
    }

    super::free();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * PCI Configurator
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void CLASS::checkPCIConfiguration( void )
{
    pci_dev_t   bridge;
    UInt8       busNum;

    // [  Phase-1 :: Mapping  ]
    //
    // Map out the entire tree behind the PCI bridge.
    // Resource requirements are probed and recorded.  

    DLOG("[ PCI configuration Phase-1 ]\n");

    bridge = fPCIBridgeList[0];
    busNum = bridge->secBusNum;
    pciBridgeScanBus(bridge, busNum, &busNum, bridge->subBusNum);

    // [  Phase-1b :: sort bridges  ]
    //

    DLOG("[ PCI configuration Phase-1b ]\n");

    fPCIBridgeIndex     = 0;
    fPCIBridgeTailIndex = 0;
    do
    {
	bridge = fPCIBridgeList[fPCIBridgeIndex++];

	// List all bridges in order they were discovered.
	// Bridge list is
	// sorted by tree depth with host/root bridge at index 0.
	// This is a breadth-first tree scan.

	FOREACH_CHILD(bridge, child)
	{
	    if (child->isBridge &&
		(fPCIBridgeTailIndex < (kPCIBridgeMaxCount-1)))
	    {
		fPCIBridgeList[++fPCIBridgeTailIndex] = child;
		DLOG("  added bridge %p bus %u:%u to index %d\n",
		     child, child->secBusNum, child->subBusNum,
		     fPCIBridgeTailIndex);
	    }
	}
    }
    while (fPCIBridgeIndex <= fPCIBridgeTailIndex);

    if (fFlags & kIOPCIConfiguratorAllocate)
    {
        // [  Phase-2 :: Checking  ]
        //
        // Traverse the PCI bridges in reverse order, from leaf bridges
        // towards the host bridge, and check for PCI bridges that were
        // misconfigured by the BIOS. For each bridge, compute the size
        // needed for each resource type based on client requirements.

        DLOG("[ PCI configuration Phase-2 ]\n");
        for (int bridgeIndex = fPCIBridgeTailIndex; bridgeIndex; bridgeIndex--)
        {
            pciBridgeCheckConfiguration(fPCIBridgeList[bridgeIndex]);
        }

        // [  Phase-3 :: Repairing  ]
        //
        // Fix any problems detected during phase 2 (hopefully none).

        DLOG("[ PCI configuration Phase-3 ]\n");
        for (int bridgeIndex = 0; bridgeIndex <= fPCIBridgeTailIndex; bridgeIndex++)
        {
            bridge = fPCIBridgeList[bridgeIndex];
            pciBridgeAllocateResource(bridge);
            pciBridgeDistributeResource(bridge);
        }
    }

    // [  Phase-4 :: Device Tree  ]
    //
    // Associate PCI devices with the device-tree nodes.

    DLOG("[ PCI configuration Phase-4 ]\n");
    for (int bridgeIndex = 0; bridgeIndex <= fPCIBridgeTailIndex; bridgeIndex++)
    {
        bridge = fPCIBridgeList[bridgeIndex];
        pciBridgeConstructDeviceTree(bridge);
    }
}

//---------------------------------------------------------------------------

void CLASS::constructAddressingProperties( pci_dev_t device, OSDictionary * propTable )
{
    IOPCIRange *		 range;
    IOPCIPhysicalAddress regData;
    OSData *		 prop;
    OSData *		 regProp;
    OSData *		 assignedProp;
    OSData *		 rangeProp;

    assignedProp = OSData::withCapacity(32);
    regProp      = OSData::withCapacity(32);
    rangeProp    = OSData::withCapacity(32);
    if (!assignedProp || !regProp || !rangeProp)
	return;

    regData.physHi  = device->space;
    regData.physMid = 0;
    regData.physLo  = 0;
    regData.lengthHi  = 0;
    regData.lengthLo  = 0;
    regProp->appendBytes(&regData, sizeof(regData));

    for (uint32_t i = 0; i < kIOPCIRangeCount; i++)
    {
	static const uint8_t barRegisters[kIOPCIRangeExpansionROM + 1] = { 0x10, 0x14, 0x18, 0x1c, 0x20, 0x24, 0x30 };

	range = &device->ranges[i];
	if (range->size == 0)
	    continue;
	if (kIOPCIResourceTypeBusNumber == range->type)
	    continue;

	regData.physHi = device->space;
	switch (range->type)
	{
	  case kIOPCIResourceTypeMemory:
	    regData.physHi.s.space    = kIOPCI32BitMemorySpace;
	    break;
	  case kIOPCIResourceTypePrefetchMemory:
	    regData.physHi.s.space    = kIOPCI32BitMemorySpace;
	    regData.physHi.s.prefetch = 1;
	    break;
	  case kIOPCIResourceTypeIO:
	    regData.physHi.s.space    = kIOPCIIOSpace;
	    break;
	}

	regData.physMid  = 0;
	regData.physLo   = 0;
	regData.lengthHi = (range->size >> 32ULL);
	regData.lengthLo = range->size;

	if (i <= kIOPCIRangeExpansionROM)
	{
	    regData.physHi.s.registerNum = barRegisters[i];
	    regProp->appendBytes(&regData, sizeof(regData));
	    if (range->start)
	    {
		regData.physHi.s.reloc = 1;
		regData.physMid = (range->start >> 32ULL);
		regData.physLo  = range->start;
		assignedProp->appendBytes(&regData, sizeof(regData));
	    }
	}
	else
	{
	    regData.physHi.s.reloc       = 1;
	    regData.physHi.s.reloc       = 1;
	    regData.physMid              = (range->start >> 32ULL);
	    regData.physLo               = range->start;
	    regData.physHi.s.busNum      = 0;
	    regData.physHi.s.deviceNum   = 0;
	    regData.physHi.s.functionNum = 0;
	    regData.physHi.s.registerNum = 0;
	    rangeProp->appendBytes(&regData, sizeof(regData.physHi) + sizeof(regData.physMid) + sizeof(regData.physLo));
	    rangeProp->appendBytes(&regData, sizeof(regData.physHi) + sizeof(regData.physMid) + sizeof(regData.physLo));
	    rangeProp->appendBytes(&regData.lengthHi, sizeof(regData.lengthHi) + sizeof(regData.lengthLo));
	}
    }
    propTable->setObject("reg", regProp);
    regProp->release();
    if (assignedProp->getLength())
	propTable->setObject("assigned-addresses", assignedProp);
    assignedProp->release();
    if (rangeProp->getLength())
    {
	propTable->setObject("ranges", rangeProp);

	regData.lengthLo = 3;
	prop = OSData::withBytes( &regData.lengthLo, sizeof(regData.lengthLo) );
	if (prop)
	{
	    propTable->setObject("#address-cells", prop );
	    prop->release();
	}
	regData.lengthLo = 2;
	prop = OSData::withBytes( &regData.lengthLo, sizeof(regData.lengthLo) );
	if (prop)
	{
	    propTable->setObject("#size-cells", prop );
	    prop->release();
	}
    }
    rangeProp->release();
}

OSDictionary * CLASS::constructProperties( pci_dev_t device )
{
    IOPCIAddressSpace   space = device->space;
    OSDictionary *	propTable;
    UInt32		value;
    UInt32		vendor, product, classCode, revID;
    UInt32		subVendor = 0, subProduct = 0;
    OSData *		prop;
    const char *	name;
    const OSSymbol *	nameProp;
    char		compatBuf[128];
    char *		out;

    struct IOPCIGenericNames
    {
        const char *	name;
        UInt32		mask;
        UInt32		classCode;
    };
    static const IOPCIGenericNames genericNames[] = {
                { "display", 	0xffffff, 0x000100 },
                { "scsi", 	0xffff00, 0x010000 },
                { "ethernet", 	0xffff00, 0x020000 },
                { "display", 	0xff0000, 0x030000 },
                { "pci-bridge", 0xffff00, 0x060400 },
                { 0, 0, 0 }
            };
    const IOPCIGenericNames *	nextName;

    propTable = OSDictionary::withCapacity( 8 );
    if (!propTable)
	return (NULL);

    constructAddressingProperties(device, propTable);

    value = configRead32( space, kIOPCIConfigVendorID );
    vendor = value & 0xffff;
    product = value >> 16;

    prop = OSData::withBytes( &vendor, sizeof(vendor) );
    if (prop)
    {
	propTable->setObject("vendor-id", prop );
	prop->release();
    }

    prop = OSData::withBytes( &product, sizeof(product) );
    if (prop)
    {
	propTable->setObject("device-id", prop );
	prop->release();
    }

    value = configRead32( space, kIOPCIConfigRevisionID );
    revID = value & 0xff;
    prop = OSData::withBytes( &revID, sizeof(revID) );
    if (prop)
    {
	propTable->setObject("revision-id", prop );
	prop->release();
    }

    classCode = value >> 8;
    prop = OSData::withBytes( &classCode, sizeof(classCode) );
    if (prop)
    {
	propTable->setObject("class-code", prop );
	prop->release();
    }

    // make generic name

    name = 0;
    for (nextName = genericNames;
	    (0 == name) && nextName->name;
	    nextName++)
    {
	if ((classCode & nextName->mask) == nextName->classCode)
	    name = nextName->name;
    }

    // or name from IDs

    value = configRead32( space, kIOPCIConfigSubSystemVendorID );
    if (value)
    {
	subVendor = value & 0xffff;
	subProduct = value >> 16;

	prop = OSData::withBytes( &subVendor, sizeof(subVendor) );
	if (prop)
	{
	    propTable->setObject("subsystem-vendor-id", prop );
	    prop->release();
	}
	prop = OSData::withBytes( &subProduct, sizeof(subProduct) );
	if (prop)
	{
	    propTable->setObject("subsystem-id", prop );
	    prop->release();
	}
    }

    out = compatBuf;
    if ((subVendor || subProduct)
	    && ((subVendor != vendor) || (subProduct != product)))
	out += sprintf(out, "pci%lx,%lx", subVendor, subProduct) + 1;
    if (0 == name)
	name = out;
    out += sprintf(out, "pci%lx,%lx", vendor, product) + 1;
    out += sprintf(out, "pciclass,%06lx", classCode) + 1;

    prop = OSData::withBytes( compatBuf, out - compatBuf );
    if (prop)
    {
	propTable->setObject("compatible", prop );
	prop->release();
    }

    nameProp = OSSymbol::withCString( name );
    if (nameProp)
    {
	propTable->setObject( "name", (OSSymbol *) nameProp);
	propTable->setObject( gIONameKey, (OSSymbol *) nameProp);
	nameProp->release();
    }

    if (kPCIHotPlug == device->supportsHotPlug)
	propTable->setObject(kIOPCIHotPlugKey, kOSBooleanTrue);
    else if (kPCILinkChange == device->supportsHotPlug)
	propTable->setObject(kIOPCILinkChangeKey, kOSBooleanTrue);

    return (propTable);
}


//---------------------------------------------------------------------------

static IOACPIPlatformDevice * IOPCICopyACPIDevice( IORegistryEntry * device )
{
    IOACPIPlatformDevice * acpiDevice = 0;
    OSString *             acpiPath;

    if (device)
    {
	acpiPath = (OSString *) device->copyProperty(kACPIDevicePathKey);
	if (acpiPath && !OSDynamicCast(OSString, acpiPath))
	{
	    acpiPath->release();
	    acpiPath = 0;
	}

	if (acpiPath)
	{
	    IORegistryEntry * entry;

	    entry = IORegistryEntry::fromPath(acpiPath->getCStringNoCopy());
	    acpiPath->release();

	    if (entry && entry->metaCast("IOACPIPlatformDevice"))
		acpiDevice = (IOACPIPlatformDevice *) entry;
	    else if (entry)
		entry->release();
	}
    }

    return (acpiDevice);
}

//---------------------------------------------------------------------------

static UInt8 IOPCIIsHotplugPort(IORegistryEntry * bridgeDevice)
{
    UInt8                  type = kPCIStatic;
    IOACPIPlatformDevice * rp;
    IOACPIPlatformDevice * child;
    const IORegistryPlane * plane = IORegistryEntry::getPlane("IOACPIPlane");

    rp = IOPCICopyACPIDevice(bridgeDevice);
    if (rp && plane)
    {
	child = (IOACPIPlatformDevice *) rp->getChildEntry(plane);
	if (child)
	{
	    IOReturn   ret;
	    UInt32     result32 = 0;
	    OSObject * obj;

	    ret = child->evaluateInteger("_RMV", &result32);
	    if (kIOReturnSuccess == ret)
	    {
		if (result32)
		    type = kPCIHotPlug;
	    }
	    else if ((obj = child->copyProperty(kACPIDevicePropertiesKey)))
	    {
		OSDictionary * dict;
		if ((dict = OSDynamicCast(OSDictionary, obj)) 
		  && dict->getObject(kACPIPCILinkChangeKey))
		    type = kPCILinkChange;
	    }
	}
    }
    if (rp)
	rp->release();

    return (type);
}

//---------------------------------------------------------------------------

struct MatchDTEntryContext
{
    CLASS *   	me;
    pci_dev_t	bridge;
};

void CLASS::matchDTEntry( IORegistryEntry * dtEntry, void * _context )
{
    MatchDTEntryContext * context = (MatchDTEntryContext *) _context;
    pci_dev_t             bridge = context->bridge;
    pci_dev_t             match = 0;
    const OSSymbol *      location;	

    assert(bridge);
    assert(dtEntry);

    location = dtEntry->copyLocation();
    if (!location)
        return;

    UInt32 devfn = strtoul(location->getCStringNoCopy(), NULL, 16);
    UInt32 deviceNum   = ((devfn >> 16) & 0x1f);
    UInt32 functionNum = (devfn & 0x7);
    bool   functionAll = ((devfn & 0xffff) == 0xffff);

    FOREACH_CHILD( bridge, child )
    {
        if (child->space.s.deviceNum == deviceNum &&
            (functionAll || (child->space.s.functionNum == functionNum)))
        {
            match = child;
            break;
        }
    }

    if (dtEntry->inPlane(gIOServicePlane))
	match = 0;

    if (match)
    {
        match->dtNub = dtEntry;
        DLOGC(context->me, "Found PCI device for DT entry [%s] %x:%x\n",
             dtEntry->getName(), match->space.s.deviceNum,
             match->space.s.functionNum);
    }
    else
    {
	DLOGC(context->me, "NOT FOUND: PCI device for DT entry [%s] %u:%u\n", 
		dtEntry->getName(), (uint32_t) deviceNum, (uint32_t) functionNum);
    }

    if (location)
        location->release();
}

void CLASS::matchACPIEntry( IORegistryEntry * dtEntry, void * _context )
{
    MatchDTEntryContext * context = (MatchDTEntryContext *) _context;
    pci_dev_t             bridge = context->bridge;
    pci_dev_t             match = 0;
    OSNumber *            adr;

    assert(bridge);
    assert(dtEntry);

    adr = OSDynamicCast(OSNumber, dtEntry->getProperty("_ADR"));
    if (!adr)
        return;

    UInt32 devfn = adr->unsigned32BitValue();
    UInt32 deviceNum = ((devfn >> 16) & 0x1f);
    UInt32 functionNum = (devfn & 0x7);
    bool   functionAll = ((devfn & 0xffff) == 0xffff);

    FOREACH_CHILD( bridge, child )
    {
        if (child->space.s.deviceNum == deviceNum &&
            (functionAll || (child->space.s.functionNum == functionNum)))
        {
            match = child;
	    if (!functionAll)
		break;
        }
    }

    if (match)
    {
        match->acpiDevice = dtEntry;
        DLOGC(context->me, "Found PCI device for ACPI entry [%s] %x:%x\n",
             dtEntry->getName(), match->space.s.deviceNum,
             match->space.s.functionNum);
    }
    else
    {
	DLOGC(context->me, "NOT FOUND: PCI device for ACPI entry [%s] %u:%u\n",
		dtEntry->getName(), (uint32_t) deviceNum, (uint32_t) functionNum);
    }
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeConnectDeviceTree( pci_dev_t bridge )
{
    IORegistryEntry *	dtBridge = bridge->dtNub;
    MatchDTEntryContext context;

    if (dtBridge)
    {
	context.me     = this;
	context.bridge = bridge;
        dtBridge->applyToChildren(&matchDTEntry, &context, gIODTPlane);

	if (gIOPCIACPIPlane)
	{
	    IORegistryEntry *
	    acpiBridgeDevice = IOPCICopyACPIDevice(dtBridge);
	    if (acpiBridgeDevice)
	    {
		acpiBridgeDevice->applyToChildren(&matchACPIEntry, &context, gIOPCIACPIPlane);
		acpiBridgeDevice->release();
	    }
	}
    }
    FOREACH_CHILD(bridge, child)
    {
        if (!child->isBridge)
	    continue;

	if (child->headerType == kPCIHeaderType2)
	    child->supportsHotPlug = kPCIHotPlug;
	else if (child->dtNub)
	    child->supportsHotPlug = IOPCIIsHotplugPort(child->dtNub);
	else
	    child->supportsHotPlug = kPCIStatic;
    }
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeConstructDeviceTree( pci_dev_t bridge )
{
    IORegistryEntry *	dtBridge = bridge->dtNub;
    uint32_t		int32;

    if (dtBridge)
    {
	int32 = 3;
	dtBridge->setProperty("#address-cells", &int32, sizeof(int32));
	int32 = 2;
	dtBridge->setProperty("#size-cells", &int32, sizeof(int32));

        // Create missing device-tree entries for any child devices.

        FOREACH_CHILD( bridge, child )
        {
	    OSDictionary *     propTable;
	    OSObject *         obj;
	    const OSSymbol *   sym;

	    propTable = constructProperties(child);
	    if (!propTable)
		continue;

	    if (!child->dtNub)
	    {
		IOService *    nub;

		if (child->acpiDevice)
		{
		    OSObject  *    obj;
		    OSDictionary * mergeProps;
		    obj = child->acpiDevice->copyProperty(kACPIDevicePropertiesKey);
		    if (obj)
		    {
			if ((mergeProps = OSDynamicCast(OSDictionary, obj)))
			    propTable->merge(mergeProps);
			obj->release();
		    }
		}

		nub = OSTypeAlloc(IOService);
		if (nub && 
		    (!nub->init(propTable) || (!nub->attachToParent(bridge->dtNub, gIODTPlane))))
		{
		    nub->release();
		    nub = 0;
		}
		child->dtNub = nub;
		if ((sym = OSDynamicCast(OSSymbol, propTable->getObject(gIONameKey))))
		    child->dtNub->setName(sym);

		if (child->acpiDevice)
		{
		    const OSSymbol * sym;
		    if ((sym = child->acpiDevice->copyName()))
		    {
			nub->setName(sym);
			sym->release();
		    }
		    if ((sym = child->acpiDevice->copyLocation()))
		    {
			nub->setLocation(sym);
			sym->release();
		    }
		}
	    }
	    else
	    {
		OSCollectionIterator * propIter =
		    OSCollectionIterator::withCollection(propTable);
		if (propIter)
		{
		    const OSSymbol * propKey;
		    while ((propKey = (const OSSymbol *)propIter->getNextObject()))
		    {
			if (child->dtNub->getProperty(propKey))
			    continue;
			obj = propTable->getObject(propKey);
			child->dtNub->setProperty(propKey, obj);
		    }
		    propIter->release();
		}
	    }
	    propTable->release();
        }

	if ((kIOPCIConfiguratorAllocate & fFlags)
	 || (kPCIHotPlug != bridge->supportsHotPlug))
	{
	    dtBridge->setProperty(kIOPCIConfiguredKey, kOSBooleanTrue);
	}
    }
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeScanBus( pci_dev_t bridge, 
			      UInt8 busNum, UInt8 * nextBusNum, UInt8 lastBusNum )
{
    UInt8               scanDevice, scanFunction, lastFunction;
    IOPCIAddressSpace   space;
    UInt32              vendor;
    UInt8		nextChildBusNum;

    DLOG("Scanning PCI bridge %p, bus %u (%u:%u)\n", bridge, busNum, *nextBusNum, lastBusNum);

    assert(bridge->headerType  == kPCIHeaderType1);
    assert(bridge->deviceState == kPCIDeviceStateBIOSConfig);

    // Scan all PCI devices and functions on the secondary bus.

    space.bits = 0;
    space.s.busNum = busNum;

    for (scanDevice = 0; scanDevice <= 31; scanDevice++)
    {
        lastFunction = 0;
        for (scanFunction = 0; scanFunction <= lastFunction; scanFunction++)
        {
            space.s.deviceNum   = scanDevice;
            space.s.functionNum = scanFunction;

	    vendor = configRead32(space, kIOPCIConfigVendorID);
            vendor &= 0x0000ffff;
            if ((0 == vendor) || (0xffff == vendor))
	    {
                continue;
	    }

            pciBridgeProbeChild(bridge, space);

            // look in function 0 for multi function flag
            if ((0 == scanFunction)
                && (0x00800000 & configRead32(space, kIOPCIConfigCacheLineSize)))
                lastFunction = 7;
        }
    }

    // Associate bootrom devices.

    pciBridgeConnectDeviceTree(bridge);

    // Probe below any bridges, assigning bus numbers.

    nextChildBusNum = busNum;
    lastBusNum = bridge->subBusNum;

    FOREACH_CHILD(bridge, child)
    {
	UInt8 childBusNum;
	bool  didAllocateBus;

        if (child->headerType != kPCIHeaderType1)
	    continue;

	if ((kPCIHotPlug == child->supportsHotPlug) && !(kIOPCIConfiguratorAllocate & fFlags))
	    continue;

	didAllocateBus = false;
	childBusNum = child->secBusNum;
	if (childBusNum <= busNum)
	{
	    childBusNum = nextChildBusNum;
	    if (childBusNum >= lastBusNum)
	    {
		DLOG("bridge %p ran out of bus numbers bus %u:%u\n",
		     child, child->secBusNum, child->subBusNum);
		continue;
	    }
	    childBusNum++;
	    DLOG("bridge %p allocated bus %u\n", child, childBusNum);
	    nextChildBusNum = childBusNum;
	    child->secBusNum = childBusNum;
	    // parent is wide open during scan
	    child->subBusNum = lastBusNum;

	    uint32_t reg32 = configRead32(child->space, kPCI2PCIPrimaryBus);
	    reg32 &= ~0x00ffffff;
	    reg32 |= busNum | (child->secBusNum << 8) | (child->subBusNum << 16);
	    configWrite32(child->space, kPCI2PCIPrimaryBus, reg32);
	    didAllocateBus = true;
	}

	pciBridgeScanBus(child, childBusNum, &nextChildBusNum, lastBusNum);

	if (didAllocateBus)
	{
	    // close down to the allocated range
	    child->subBusNum = nextChildBusNum;
	    configWrite8( child->space, kPCI2PCISubordinateBus, child->subBusNum );
	    *nextBusNum = nextChildBusNum;
	}

	DLOG("bridge %p scan final bus range %u:%u\n",
	     child, child->secBusNum, child->subBusNum);
    }
}

//---------------------------------------------------------------------------

void CLASS::pciRangeAppendSubRange( IOPCIRange * headRange,
                                    IOPCIRange * newRange )
{
    IOPCIRange * *   prevRange;
    IOPCIRange *     nextRange;

    assert(newRange->nextSubRange == 0);
    prevRange = &headRange->subRange;
    nextRange = *prevRange;
    while (nextRange && (newRange->alignment < nextRange->alignment))
    {
        // new range has smaller alignment, keep walking down the list
        prevRange = &nextRange->nextSubRange;
        nextRange = *prevRange;
    }
    *prevRange = newRange;
    newRange->nextSubRange = nextRange;
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeCheckConfiguration( pci_dev_t bridge )
{
    bool	 reconfig = false;
    IOPCIRange * newRanges;
    IOPCIRange * childRange;
    IOPCIRange * range;

    static const IOPCIScalar minBridgeAlignments[kIOPCIResourceTypeCount] = {
	kPCIBridgeMemoryAlignment, kPCIBridgeMemoryAlignment, 
	kPCIBridgeIOAlignment, kPCIBridgeBusNumberAlignment
    };

    DLOG("pciBridgeCheckConfiguration(bus %u, state %d)\n", bridge->secBusNum, bridge->deviceState);

    assert(bridge != fPCIBridgeList[0]);
    assert(bridge->isBridge);
    assert(bridge->deviceState == kPCIDeviceStateBIOSConfig);

    // Limited to PCI-PCI bridges. Skip subtractive decode bridges.

    if (bridge->headerType != kPCIHeaderType1)
        return;

    if ((bridge->classCode & 0xFFFFFF) != 0x060400)
        return;

    DLOG("Checking PCI bus %u\n", bridge->secBusNum);

    newRanges = IONew(IOPCIRange, kIOPCIResourceTypeCount);
    if (!newRanges)
    {
        DLOG("  ERROR: memory allocation failed\n");
        return;
    }

    memset(newRanges, 0, sizeof(IOPCIRange) * kIOPCIResourceTypeCount);
    for (int i = 0; i < kIOPCIResourceTypeCount; i++)
    {
        newRanges[i].type = i;
	newRanges[i].alignment = minBridgeAlignments[i];
	if (kIOPCIResourceTypeBusNumber == i)
	    newRanges[i].size = 1;
    }

    // Calculate total child resource requirements and propose new
    // resource apertures for the current bridge. Child ranges are
    // sorted and linked as sub-ranges to the bridge range.

    FOREACH_CHILD(bridge, child)
    {
        for (int i = 0; i < kIOPCIRangeCount; i++)
        {
	    childRange = &child->ranges[i];
            if (childRange->size == 0)
                continue;

            range = &newRanges[childRange->type];
            range->flags |= childRange->flags;
            range->size  += childRange->size;
            range->alignment = max(childRange->alignment, range->alignment);
            assert(childRange->size >= childRange->alignment);

	    if (kIOPCIRangeFlagMaximizeSize & range->flags)
		reconfig = true;
        }
    }

    // Bridge with closed I/O window, but child need I/O resources.

    if (bridge->ranges[kIOPCIRangeBridgeIO].size == 0 &&
        newRanges[kIOPCIResourceTypeIO].size)
    {
        reconfig = true;
    }

    // Bridge with closed memory window, but child need memory resources.

    if (bridge->ranges[kIOPCIRangeBridgeMemory].size == 0 &&
        newRanges[kIOPCIResourceTypeMemory].size)
    {
        reconfig = true;
    }

    // Invalidate BIOS configuration and wait for parent bridge to assign
    // the needed resources for this bridge.

    if (reconfig)
    {
        for (int i = 0; i < kIOPCIResourceTypeCount; i++)
        {
            // For bridges, range size should be larger than or equal to
            // alignment. For devices, all ranges are naturally aligned.

            newRanges[i].size = IORound(newRanges[i].size,
					minBridgeAlignments[i]);

            memcpy(&bridge->ranges[BRIDGE_RANGE_NUM(i)],
                   &newRanges[i], sizeof(IOPCIRange));

            DLOG("  %s: new range size %llx align %llx flags %x\n",
                 gPCIResourceTypeName[i],
                 newRanges[i].size, newRanges[i].alignment, (uint32_t) newRanges[i].flags);
        }

	bridge->deviceState = kPCIDeviceStateResourceWait;
    }
    else
    {
        DLOG("  BIOS config retained\n");
    }

    IODelete(newRanges, IOPCIRange, kIOPCIResourceTypeCount);
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeClipRanges( IOPCIRange * rangeList,
                                 IOPCIScalar start, IOPCIScalar size )
{
    IOPCIRange *   thisRange;
    IOPCIScalar    end;
    IOPCIScalar    overlap;
    IOPCIScalar    rangeEnd;

    if (!rangeList || !size)
        return;

    for (thisRange = rangeList; thisRange; thisRange = thisRange->next)
    {
        if (thisRange->size == 0)
            continue;

        end = start + size - 1;
        if (thisRange->start > end)
            continue;   // no overlap

        rangeEnd = thisRange->start + thisRange->size - 1;

        if (thisRange->start >= start)
        {
            overlap = end - thisRange->start + 1;
            if (thisRange->size <= overlap)
            {
                thisRange->size = 0;
            }
            else
            {
                thisRange->size  -= overlap;
                thisRange->start += overlap;
            }
            DLOG("  clipped bridge %s range head to %llx:%llx for %llx:%llx overlap %llx\n",
                 gPCIResourceTypeName[thisRange->type],
                 thisRange->start, thisRange->size, start, size, overlap);
        }
        else if (rangeEnd >= start && rangeEnd < end)
        {
            overlap = rangeEnd - start + 1;
            thisRange->size -= overlap;
            DLOG("  clipped bridge %s range tail to %llx:%llx for %llx:%llx overlap %llx\n",
                 gPCIResourceTypeName[thisRange->type],
                 thisRange->start, thisRange->size, start, size, overlap);
        }
    }
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeAllocateResource( pci_dev_t bridge )
{
    bool               allocNeeded = false;
    IORangeAllocator * allocators[kIOPCIResourceTypeCount];
    IOPCIScalar        allocLimit[kIOPCIResourceTypeCount];
    int                waitCounts[kIOPCIResourceTypeCount];
    IOPCIRange *       bridgeRangeList[kIOPCIResourceTypeCount];
    bool               ok;

    DLOG("pciBridgeAllocateResource(bus %u, state %d)\n", bridge->secBusNum, bridge->deviceState);

    if ((bridge->deviceState != kPCIDeviceStateBIOSConfig)
     && (bridge->deviceState != kPCIDeviceStateConfigurationDone))
        return;

    // Search for child bridges waiting for resources.

    FOREACH_CHILD(bridge, child)
    {
        if (child->deviceState == kPCIDeviceStateResourceWait)
        {
            allocNeeded = true;
            break;
        }
    }
    if (!allocNeeded) return;

    DLOG("Allocating resources on bus %u\n", bridge->secBusNum);

    memset(allocators, 0, sizeof(allocators));
    memset(allocLimit, 0, sizeof(allocLimit));
    memset(waitCounts, 0, sizeof(waitCounts));
    memset(bridgeRangeList, 0, sizeof(bridgeRangeList));

    // Can't pass the bridge range(s) to the allocator yet since there
    // might be child ranges that are not completely inside the bounds
    // of its parent range. The bridge ranges must be clipped first to
    // prevent the allocator from handing out BIOS assigned areas.

    if (bridge == fPCIBridgeList[0])
    {
        bridgeRangeList[kIOPCIResourceTypeMemory]
	    = fRootBridge->reserved->rangeLists[kIOPCIResourceTypeMemory];

        bridgeRangeList[kIOPCIResourceTypePrefetchMemory]
	    = fRootBridge->reserved->rangeLists[kIOPCIResourceTypePrefetchMemory];

        bridgeRangeList[kIOPCIResourceTypeIO]
	    = fRootBridge->reserved->rangeLists[kIOPCIResourceTypeIO];

        bridgeRangeList[kIOPCIResourceTypeBusNumber]
	    = &bridge->ranges[kIOPCIRangeBridgeBusNumber];

        for (int i = 0; i < kIOPCIResourceTypeCount; i++)
        {
	    IOPCIRange * thisRange;
	    for (thisRange = bridgeRangeList[i]; thisRange; thisRange = thisRange->next)
	    {
		DLOG("root bridge resource %s %llx len %llx\n", gPCIResourceTypeName[i], thisRange->start, thisRange->size);
	    }
        }

        // Never allocate ranges below 1K I/O and 1MB Memory to avoid
        // stomping on legacy ISA, and VGA ranges.

        pciBridgeClipRanges(bridgeRangeList[kIOPCIResourceTypeIO], 
                            0, 0x400);

        pciBridgeClipRanges(bridgeRangeList[kIOPCIResourceTypeMemory], 
                            0, 0x100000);

    }
    else
    {
        for (int i = 0; i < kIOPCIResourceTypeCount; i++)
        {
            bridgeRangeList[i] = &bridge->ranges[BRIDGE_RANGE_NUM(i)];
        }
    }

    // Remove current child allocations

    FOREACH_CHILD(bridge, child)
    {
        IOPCIRange * childRange;

        if (child->deviceState != kPCIDeviceStateBIOSConfig)
            continue;

        for (int i = 0; i < kIOPCIRangeCount; i++)
        {
	    childRange = &child->ranges[i];
            if (childRange->size == 0)
                continue;

	    pciBridgeClipRanges(bridgeRangeList[childRange->type],
				childRange->start, childRange->size);

	    // clip both prefetchable and non-prefetchable memory
            if (childRange->type == kIOPCIResourceTypePrefetchMemory)
            {
                pciBridgeClipRanges(bridgeRangeList[kIOPCIResourceTypeMemory],
                                    childRange->start, childRange->size);
            }
            else if (childRange->type == kIOPCIResourceTypeMemory)
            {
                pciBridgeClipRanges(bridgeRangeList[kIOPCIResourceTypePrefetchMemory],
                                    childRange->start, childRange->size);
            }
        }
    }

    // Create an IORangeAllocator for each resource type.

    for (int i = 0; i < kIOPCIResourceTypeCount; i++)
    {
        IOPCIRange * range;

        allocators[i] = IORangeAllocator::withRange(
                            0, 1, 8/*max(8, bridgeRangeCount[i])*/);
        if (allocators[i] == 0)
            goto fail;

        // Push the clipped bridge range(s) into the allocator
        for (range = bridgeRangeList[i]; range; range = range->next)
        {
            if (range->size)
	    {
                allocators[i]->deallocate((IORangeScalar) range->start,
                                          (IORangeScalar) range->size);
	    }
        }

	if (kIOPCIResourceTypeBusNumber == i)
	    allocators[i]->allocateRange(bridge->secBusNum, 1);
    }

    // Use allocators to partition bridge resources to each child
    // with valid BIOS configs. Also count the number of resource
    // waits for each type of resource.

    FOREACH_CHILD(bridge, child)
    {
        IOPCIRange * childRange;

        assert(child->deviceState == kPCIDeviceStateBIOSConfig ||
               child->deviceState == kPCIDeviceStateResourceWait);

        for (int i = 0; i < kIOPCIRangeCount; i++)
        {
	    childRange = &child->ranges[i];
            if (childRange->size == 0)
                continue;

            // Link child ranges to form dependency chain, and sort this
            // list based on descending alignment values. This will make
            // it easier to satisfy alignment requirements for all child
            // ranges by allocating the largest sub-range first.

            pciRangeAppendSubRange(bridgeRangeList[childRange->type], 
				    childRange);

            if (child->deviceState == kPCIDeviceStateBIOSConfig)
            {
                ok = allocators[childRange->type]->allocateRange(
                                (IORangeScalar) childRange->start,
                                (IORangeScalar) childRange->size);
                if (!ok)
                {
                    // If bridge lacks prefetch range, try allocating
                    // from the non-prefetch memory range.

                    if (childRange->type == kIOPCIResourceTypePrefetchMemory)
                    {
                        ok = allocators[kIOPCIResourceTypeMemory]->allocateRange(
                                        (IORangeScalar) childRange->start,
                                        (IORangeScalar) childRange->size);
                    }
                    if (!ok)
                    {
                        DLOG("  %s: sub-range outside parent range: "
                             "0x%llx:0x%llx\n",
                             gPCIResourceTypeName[childRange->type],
                             childRange->start, childRange->size);
                    }
                }
            }
            else if (child->deviceState == kPCIDeviceStateResourceWait)
            {
                waitCounts[childRange->type]++;
            }
        }
    }

    // Come up with an upper limit on the size of each resource type
    // that can be assigned to a single child. This ensures fairness
    // when there are multiple child bridges waiting for resources.

    for (int i = 0; i < kIOPCIResourceTypeCount; i++)
    {
        if (waitCounts[i] == 0)
	    continue;
        allocLimit[i] = allocators[i]->getFreeCount() / waitCounts[i];
        DLOG("  %s: %u sub-range limited to %llx each\n",
             gPCIResourceTypeName[i], waitCounts[i], allocLimit[i]);
    }

    // Convert child bridge prefetchable memory range to non-prefetchable
    // when the parent bridge does not have a prefetchable memory range.

    if (allocLimit[kIOPCIResourceTypePrefetchMemory] == 0 &&
        waitCounts[kIOPCIResourceTypePrefetchMemory])
    {
        IOPCIRange *     to;
        IOPCIRange *     from;
        IOPCIRange *     subRange;

        FOREACH_CHILD(bridge, child)
        {
            if (child->deviceState != kPCIDeviceStateResourceWait ||
                child->ranges[kIOPCIRangeBridgePFMemory].size == 0)
                continue;

            to   = &child->ranges[kIOPCIRangeBridgeMemory];
            from = &child->ranges[kIOPCIRangeBridgePFMemory];

            // Move all prefetchable sub-ranges to the non-prefetch range.

            assert(from->subRange != 0);
            subRange = from->subRange;
            while (subRange)
            {
                IOPCIRange * tmpRange = subRange->nextSubRange;
                subRange->nextSubRange = 0;
                pciRangeAppendSubRange(to, subRange);
                subRange = tmpRange;
            }

            to->alignment = max(to->alignment, from->alignment);
            to->size += from->size;
            to->size = IORound(to->size, to->alignment);
            memset(from, 0, sizeof(*from));

            DLOG("  substitute non-prefetchable memory for bus %u %llx:%llx\n",
                 child->secBusNum, to->size, to->alignment);
        }
    }

    // Ready to assign resources to bridges not configured by BIOS.

    FOREACH_CHILD(bridge, child)
    {
        IOPCIRange *     childRange;
        IOPCIScalar    rangeStart;
        IOPCIScalar    rangeLimit;
        bool            ok = false;

        if (child->deviceState != kPCIDeviceStateResourceWait)
            continue;

        for (int i = 0; i < kIOPCIRangeCount; i++)
        {
	    childRange = &child->ranges[i];
            if (childRange->size == 0)
                continue;

            // Allocate a range with aligned per child specification.
            if (childRange->flags & kIOPCIRangeFlagMaximizeSize)
	    {
                rangeLimit = allocLimit[childRange->type];
		// non bridge ranges must be power-of-2 size
		if (i <= kIOPCIRangeBridgeMemory)
		    for (UInt32 limit = 0x80000000; limit > 0; limit >>= 1)
		{
		    if (allocLimit[i] >= limit)
		    {
			allocLimit[i] = limit;
			break;
		    }
		}
	    }
            else
                rangeLimit = childRange->size;

            ok = false;
            for (IOPCIScalar rangeSize = rangeLimit;
                 rangeSize >= childRange->size;
                 rangeSize >>= 1)
            {
		IORangeScalar allocResult;
                ok = allocators[childRange->type]->allocate(
                        rangeSize, &allocResult, childRange->alignment);

                if (!ok && childRange->type == kIOPCIResourceTypePrefetchMemory)
                {
                    ok = allocators[kIOPCIResourceTypeMemory]->allocate(
                            rangeSize, &allocResult, childRange->alignment);
                }

                if (ok)
                {
		    rangeStart = allocResult;
                    childRange->start = rangeStart;
                    childRange->size  = rangeSize;
                    DLOG("  %s: allocated block %llx:%llx\n",
                         gPCIResourceTypeName[childRange->type],
                         childRange->start, childRange->size);
                    break;
                }
            }
            if (!ok) break;
        }

//        if (ok)
            child->deviceState = kPCIDeviceStateResourceAssigned;
    }

    bridge->deviceState = kPCIDeviceStateResourceAssigned;

fail:
    for (int i = 0; i < kIOPCIResourceTypeCount; i++)
    {
        if (allocators[i])
            allocators[i]->release();
    }
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeDistributeResource( pci_dev_t bridge )
{
    DLOG("pciBridgeDistributeResource(bus %u, state %d)\n", bridge->secBusNum, bridge->deviceState);

    if (bridge->deviceState != kPCIDeviceStateResourceAssigned)
        return;

    if (bridge == fPCIBridgeList[0])
	pciApplyConfiguration(bridge);

    DLOG("Distribute resources for bus %u\n", bridge->secBusNum);

    // For each bridge range, follow its sub-range list
    // and assign a start address and a size for each.

    for (int type = 0; type < kIOPCIResourceTypeCount; type++)
    {
	pciBridgeDistributeResourceType(bridge, type);
    }

    // Apply configuration changes to all children.

    FOREACH_CHILD(bridge, child)
    {
	pciApplyConfiguration(child);
    }
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeDistributeResourceType( pci_dev_t bridge, UInt32 type )
{
    IOPCIRange *   bridgeRange = &bridge->ranges[BRIDGE_RANGE_NUM(type)];
    IOPCIRange *   nextRange;
    IOPCIScalar    nextStart;
    IOPCIScalar	   allocSize;
    IOPCIScalar    extraSize = 0;
    IOPCIScalar    requiredSize = 0;
    int            maximizeCount = 0;

    allocSize = bridgeRange->size;
    if (allocSize == 0)
        return;

    assert(bridgeRange->subRange != 0);

    // Compute the total size of sub-ranges, and the number of them
    // that wants to maximize their size.

    if (kIOPCIResourceTypeBusNumber == type)
	allocSize--;

    for (nextRange = bridgeRange->subRange; nextRange;
         nextRange = nextRange->nextSubRange)
    {
        assert(nextRange->size > 0);
        assert(nextRange->type == type);

        requiredSize += nextRange->size;
        if (nextRange->flags & kIOPCIRangeFlagMaximizeSize)
            maximizeCount++;
    }

    assert(allocSize >= requiredSize);
    if (maximizeCount && (allocSize > requiredSize))
    {
        extraSize = allocSize - requiredSize;
        extraSize /= maximizeCount;
    }

    DLOG("  %s: total size %llx, required size %llx, maximize count %u\n",
         gPCIResourceTypeName[type],
         allocSize, requiredSize, maximizeCount);

    // Partition the bridge range into blocks for each sub-range.

    nextStart = bridgeRange->start;
    if (kIOPCIResourceTypeBusNumber == type)
	nextStart++;

    for (nextRange = bridgeRange->subRange; nextRange;
         nextRange = nextRange->nextSubRange)
    {
        assert((nextStart & (nextRange->alignment - 1)) == 0);
        nextRange->start = nextStart;

        if (nextRange->flags & kIOPCIRangeFlagMaximizeSize)
            nextRange->size += IOTrunc(extraSize, nextRange->alignment);

        nextStart += nextRange->size;

        DLOG("  %s: assigned block %llx:%llx\n",
             gPCIResourceTypeName[type], nextRange->start, nextRange->size);
    }
    assert(nextStart - bridgeRange->start <= bridgeRange->size);
}

//---------------------------------------------------------------------------

UInt16 CLASS::pciDisableAccess( pci_dev_t device )
{
    UInt16  command;

    command = configRead16(device->space, kIOPCIConfigCommand);
    configWrite16(device->space, kIOPCIConfigCommand,
       command & ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace));

    return command;
}

void CLASS::pciRestoreAccess( pci_dev_t device, UInt16 command )
{
    configWrite16(device->space, kIOPCIConfigCommand, command);
}

//---------------------------------------------------------------------------

void CLASS::pciApplyConfiguration( pci_dev_t device )
{
    switch (device->headerType)
    {
        case kPCIHeaderType0:
            pciDeviceApplyConfiguration( device );
            break;
        case kPCIHeaderType1:
        case kPCIHeaderType2:
            pciBridgeApplyConfiguration( device );
            break;
    }

    pciWriteLatencyTimer( device );
    device->deviceState = kPCIDeviceStateConfigurationDone;
}

void CLASS::pciDeviceApplyConfiguration( pci_dev_t device )
{
    IOPCIRange * range;
    UInt16      reg16;

    DLOG("Applying config for device %u:%u:%u\n", PCI_ADDRESS_TUPLE(device));
    fDeviceConfigCount++;

    reg16 = pciDisableAccess(device);

    for (int i = kIOPCIRangeBAR0; i <= kIOPCIRangeBAR5; i++)
    {
	range = &device->ranges[i];
        if (range->size == 0)
            continue;
        if (range->start == 0)
            continue;

        DLOG("  bar 0x%x = %llx\n", 0x10 + i * 4, range->start);
        configWrite32(device->space, 0x10 + i * 4, range->start);
    }

    range = &device->ranges[kIOPCIRangeExpansionROM];
    if (range->size && range->start)
    {
        DLOG("  rom 0x%x = %llx\n", kIOPCIConfigExpansionROMBase, range->start);
        configWrite32(device->space, kIOPCIConfigExpansionROMBase, range->start);
    }

    reg16 &= ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace |
               kIOPCICommandBusMaster | kIOPCICommandMemWrInvalidate);
    pciRestoreAccess(device, reg16);

    DLOG("  Device Command = %08x\n", (uint32_t) 
         configRead32(device->space, kIOPCIConfigCommand));
}

void CLASS::pciBridgeApplyConfiguration( pci_dev_t bridge )
{
    UInt32       start;
    UInt32       end;
    IOPCIRange * range;
    UInt16       reg16;
    UInt32       baselim32;
    UInt16       baselim16;
    enum { 
	kBridgeCommand = (kIOPCICommandIOSpace | kIOPCICommandMemorySpace | kIOPCICommandBusMaster) 
    };

    if ((bridge == fPCIBridgeList[0])
    || (bridge->deviceState != kPCIDeviceStateResourceAssigned))
	reg16 = configRead16(bridge->space, kIOPCIConfigCommand);
    else do
    {
	bridge->secBusNum = bridge->ranges[kIOPCIRangeBridgeBusNumber].start;
	bridge->subBusNum = bridge->secBusNum + bridge->ranges[kIOPCIRangeBridgeBusNumber].size - 1;

	DLOG("Applying config for bridge serving bus %u\n", bridge->secBusNum);

	DLOG("  MEM: start/size = %08llx:%08llx\n",
	     bridge->ranges[kIOPCIRangeBridgeMemory].start,
	     bridge->ranges[kIOPCIRangeBridgeMemory].size);
	DLOG("  I/O: start/size = %08llx:%08llx\n",
	     bridge->ranges[kIOPCIRangeBridgeIO].start,
	     bridge->ranges[kIOPCIRangeBridgeIO].size);
	DLOG("  BUS: start/size = %08llx:%08llx\n",
	     bridge->ranges[kIOPCIRangeBridgeBusNumber].start,
	     bridge->ranges[kIOPCIRangeBridgeBusNumber].size);

	reg16 = pciDisableAccess(bridge);

	// Give children the correct bus

	FOREACH_CHILD(bridge, child)
	{
	    child->space.s.busNum = bridge->secBusNum;
	}

	// Program bridge BAR0 and BAR1

	for (int i = kIOPCIRangeBAR0; i <= kIOPCIRangeBAR1; i++)
	{
	    range = &bridge->ranges[i];
	    if (range->size == 0)
		continue;
	    assert(range->start);
	    configWrite32(bridge->space, 0x10 + i * 4, range->start);
	}

	// Program bridge bus numbers

	uint32_t reg32 = configRead32(bridge->space, kPCI2PCIPrimaryBus);
	reg32 &= ~0x00ffffff;
	reg32 |= bridge->space.s.busNum | (bridge->secBusNum << 8) | (bridge->subBusNum << 16);
	configWrite32(bridge->space, kPCI2PCIPrimaryBus, reg32);

	DLOG("  Regs:\n  BUS: prim/sec/sub = %02x:%02x:%02x\n",
	     configRead8(bridge->space, kPCI2PCIPrimaryBus),
	     configRead8(bridge->space, kPCI2PCISecondaryBus),
	     configRead8(bridge->space, kPCI2PCISubordinateBus));

	// That's it for yenta

	if (kPCIHeaderType2 == bridge->headerType)
	{
	    fYentaConfigCount++;
	    break;
	}

	// Program I/O base and limit

	baselim16 = 0x00f0; // closed range
	range = &bridge->ranges[kIOPCIRangeBridgeIO];
	if (range->size)
	{
	    assert(range->start);
	    assert((range->size  & (4096-1)) == 0);
	    assert((range->start & (4096-1)) == 0);
	    assert((range->start & 0xffff0000) == 0);

	    start = range->start;
	    end = start + range->size - 1;
	    baselim16 = ((start >> 8) & 0xf0) | (end & 0xf000);        
	}
	configWrite16(bridge->space, kPCI2PCIIORange, baselim16);
	configWrite32(bridge->space, kPCI2PCIUpperIORange, 0);

	// Program memory base and limit

	baselim32 = 0x0000FFF0; // closed range
	range = &bridge->ranges[kIOPCIRangeBridgeMemory];
	if (range->size)
	{
	    assert(range->start);
	    assert((range->size  & (0x100000-1)) == 0);
	    assert((range->start & (0x100000-1)) == 0);

	    start = range->start;
	    end = range->start + range->size - 1;
	    baselim32 = ((start >> 16) & 0xFFF0) | (end & 0xFFF00000);
	}
	configWrite32(bridge->space, kPCI2PCIMemoryRange, baselim32);

	// Program prefetchable memory base and limit

	baselim32 = 0x0000FFF0; // closed range
	range = &bridge->ranges[kIOPCIRangeBridgePFMemory];
	if (range->size)
	{
	    assert(range->start);
	    assert((range->size  & (0x100000-1)) == 0);
	    assert((range->start & (0x100000-1)) == 0);

	    start = range->start;
	    end = range->start + range->size - 1;
	    baselim32 = ((start >> 16) & 0xFFF0) | (end & 0xFFF00000);
	}
	configWrite32(bridge->space, kPCI2PCIPrefetchMemoryRange, baselim32);
	configWrite32(bridge->space, kPCI2PCIPrefetchUpperBase,  0);
	configWrite32(bridge->space, kPCI2PCIPrefetchUpperLimit, 0);

	DLOG("  MEM: base/limit   = %08x\n", (uint32_t) 
	     configRead32(bridge->space, kPCI2PCIMemoryRange));
	DLOG("  PFM: base/limit   = %08x\n", (uint32_t) 
	     configRead32(bridge->space, kPCI2PCIPrefetchMemoryRange));
	DLOG("  I/O: base/limit   = %04x\n",
	     configRead16(bridge->space, kPCI2PCIIORange));

	fBridgeConfigCount++;

    } // (bridge->deviceState == kPCIDeviceStateResourceAssigned)
    while (false);

    // Set IOSE, memory enable, Bus Master transaction forwarding

    if (bridge->deviceState == kPCIDeviceStateResourceAssigned)
    {
	DLOG("Enabling bridge serving bus %u\n", bridge->secBusNum);

	if (kPCIHeaderType2 == bridge->headerType)
	{
	    reg16 &= ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace |
		       kIOPCICommandBusMaster | kIOPCICommandMemWrInvalidate);
	}
	else
	{
	    uint16_t bridgeControl;

	    reg16 |= (kIOPCICommandIOSpace | kIOPCICommandMemorySpace |
		       kIOPCICommandBusMaster);

	    // Turn off ISA bit.
	    bridgeControl = configRead16(bridge->space, kPCI2PCIBridgeControl);
	    if (bridgeControl & 0x0004)
	    {
		bridgeControl &= ~0x0004;
		configWrite16(bridge->space, kPCI2PCIBridgeControl, bridgeControl);
	    }
	    DLOG("  Bridge Control    = %04x\n",
		 configRead16(bridge->space, kPCI2PCIBridgeControl));
	}

	pciRestoreAccess(bridge, reg16);

	DLOG("  Bridge Command    = %08x\n", (uint32_t) 
	     configRead32(bridge->space, kIOPCIConfigCommand));
    }
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeAddChild( pci_dev_t bridge, pci_dev_t child )
{
    pci_dev_t old_child = bridge->child;
    bridge->child = child;
    child->peer = old_child;
}

void CLASS::pciBridgeProbeChild( pci_dev_t bridge, IOPCIAddressSpace space )
{
    pci_dev_t   child;
    bool        ok = true;

    child = IONew(pci_dev, 1);
    if (!child) return;

    memset(child, 0, sizeof(*child));
    child->headerType = configRead8(space, kIOPCIConfigHeaderType) & 0x7f;
    child->classCode = configRead32(space, kIOPCIConfigRevisionID) >> 8;
    child->space = space;

    DLOG("Probing type %u device class-code %06x at %u:%u:%u [cpu %x]\n",
         child->headerType, child->classCode,
         PCI_ADDRESS_TUPLE(child),
         cpu_number());

    switch (child->headerType)
    {
        case kPCIHeaderType0:
            // skip devices aliased to host bridges
            if ((child->classCode & 0xFFFFFF) == 0x060000)
                break;

            pciDeviceProbeRanges(child);
            break;

        case kPCIHeaderType1:
	    child->isBridge = true;
            pciBridgeProbeRanges(child);
            break;

        case kPCIHeaderType2:
	    child->isBridge = true;
            pciYentaProbeRanges(child);
            break;

        default:
            DLOG("  bad PCI header type %x\n", child->headerType);
            ok = false;
            break;
    }

    pciCheckCacheLineSize(child);

    if (ok)
        pciBridgeAddChild(bridge, child);
    else
        IODelete(child, pci_dev, 1);
}

//---------------------------------------------------------------------------

struct BARProbeParam {
    CLASS *     target;
    pci_dev_t   device;
    UInt32      lastBarNum;
};

void CLASS::safeProbeCallback( void * refcon )
{
    BARProbeParam * param = (BARProbeParam *) refcon;
    assert(param);

    if (cpu_number() == 0)
    {
        param->target->pciProbeBaseAddressRegister(
            param->device, param->lastBarNum );
    }
    if (cpu_number() == 99)
    {
	IOLog("safeProbeCallback() gcc workaround\n");
    }
}

void CLASS::pciSafeProbeBaseAddressRegister( pci_dev_t device, UInt32 lastBarNum )
{
//    if (fFlags & kIOPCIConfiguratorAllocate)
    {
#if NO_RENDEZVOUS_KERNEL
        boolean_t       istate;
        istate = ml_set_interrupts_enabled(FALSE);
        pciProbeBaseAddressRegister(device, lastBarNum);
        ml_set_interrupts_enabled(istate);
#else
        BARProbeParam param;

        param.target     = this;
        param.device     = device;
        param.lastBarNum = lastBarNum;

        mp_rendezvous_no_intrs(&safeProbeCallback, &param);
#endif
    }
}

void CLASS::pciProbeBaseAddressRegister( pci_dev_t device, UInt32 lastBarNum )
{
    IOPCIRange *    range;
    UInt32          saved;
    UInt32          value;
    UInt32          barMask;
    UInt8           barOffset;

    if (kIOPCIRangeExpansionROM == lastBarNum) do
    {
	lastBarNum--;

	barOffset = kIOPCIConfigExpansionROMBase;
	barMask = 0x7FF;
        range = &device->ranges[kIOPCIRangeExpansionROM];

        saved = configRead32(device->space, barOffset);
        configWrite32(device->space, barOffset, 0xFFFFFFFF & ~barMask);
        value = configRead32(device->space, barOffset);
        configWrite32(device->space, barOffset, saved);

        // unimplemented BARs are hardwired to zero
        if (value == 0)
            continue;

	range->type = kIOPCIResourceTypeMemory;
        value &= ~barMask;
	if (!(fFlags & kIOPCIConfiguratorReset))
	    range->start = IOPhysical32( 0, saved & value );
        range->size  = IOPhysical32( 0, -value );
        range->alignment = range->size;

	if (!range->start)
	    device->deviceState = kPCIDeviceStateResourceWait;
    }
    while (false);

    for (UInt32 barNum = 0; barNum <= lastBarNum; barNum++)
    {
        barOffset = 0x10 + barNum * 4;
        range = &device->ranges[barNum];

        saved = configRead32(device->space, barOffset);
        configWrite32(device->space, barOffset, 0xFFFFFFFF);
        value = configRead32(device->space, barOffset);
        configWrite32(device->space, barOffset, saved);

        // unimplemented BARs are hardwired to zero
        if (value == 0)
            continue;

        if (value & 1)
        {
            barMask = 0x3;
            range->type = kIOPCIResourceTypeIO;

            // If the upper 16 bits for I/O space
            // are all 0, then we should ignore them.
            if ((value & 0xFFFF0000) == 0)
            {
                value |= 0xFFFF0000;
            }
        }
        else
        {
            barMask = 0xf;
            if (value & 0x8)
                range->type = kIOPCIResourceTypePrefetchMemory;
            else
                range->type = kIOPCIResourceTypeMemory;

            switch (value & 6)
            {
                case 2: /* below 1Mb */
                case 0: /* 32-bit mem */
                case 6: /* reserved  */
                    break;

                case 4: /* 64-bit mem */
                    barNum++;
                    break;
            }
        }

        value &= ~barMask;
	if (!(fFlags & kIOPCIConfiguratorReset))
	    range->start = IOPhysical32( 0, saved & value );
        range->size  = IOPhysical32( 0, -value );
        range->alignment = range->size;

	if (!range->start)
	    device->deviceState = kPCIDeviceStateResourceWait;
    }
}

//---------------------------------------------------------------------------

void CLASS::pciDeviceProbeRanges( pci_dev_t device )
{
    uint32_t idx;

    // Probe BAR 0 through 5 and ROM
    pciSafeProbeBaseAddressRegister(device, kIOPCIRangeExpansionROM);

    DLOG("  state %d\n", device->deviceState);
    for (idx = kIOPCIRangeBAR0; idx <= kIOPCIRangeExpansionROM; idx++)
    {
	if (!device->ranges[idx].size)
	    continue;
	DLOG("   [%x %s] %llx:%llx\n",
	     (idx == kIOPCIRangeExpansionROM) ? kIOPCIConfigExpansionROMBase : idx * 4 + 0x10,
	     gPCIResourceTypeName[device->ranges[idx].type],
	     device->ranges[idx].start, device->ranges[idx].size);
    }
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeProbeBusRange( pci_dev_t bridge )
{
    IOPCIRange *    range;

    // Record the bridge secondary and subordinate bus numbers

    bridge->secBusNum = configRead8(bridge->space, kPCI2PCISecondaryBus);
    bridge->subBusNum = configRead8(bridge->space, kPCI2PCISubordinateBus);

    range = &bridge->ranges[kIOPCIRangeBridgeBusNumber];
    range->start     = bridge->secBusNum;
    range->size      = bridge->subBusNum - bridge->secBusNum + 1;
    range->alignment = kPCIBridgeBusNumberAlignment;
    range->type      = kIOPCIResourceTypeBusNumber;
}

//---------------------------------------------------------------------------

void CLASS::pciBridgeProbeRanges( pci_dev_t bridge )
{
    IOPCIRange *    range;
    UInt32          end;
    UInt32          start;
    UInt32          bar0;
    UInt32          bar1;

    pciBridgeProbeBusRange(bridge);

    // Probe bridge BAR0 and BAR1 (is it ever implemented?)

    bar0 = configRead32(bridge->space, kIOPCIConfigBaseAddress0);
    bar1 = configRead32(bridge->space, kIOPCIConfigBaseAddress1);
    if (bar0 || bar1)
    {
        pciSafeProbeBaseAddressRegister(bridge, kIOPCIRangeBAR1);
        DLOG("  bridge BAR sizes %llx, %llx\n",
             bridge->ranges[0].size, bridge->ranges[1].size);
    }

    // Probe memory base and limit

    end = configRead32(bridge->space, kPCI2PCIMemoryRange);
    if (end)
    {
        start = (end & 0xfff0) << 16;
        end  |= 0x000fffff;
        if (end > start)
        {
            range = &bridge->ranges[kIOPCIRangeBridgeMemory];
            range->start = start;
            range->size  = end - start + 1;
            range->alignment = kPCIBridgeMemoryAlignment;
            range->type = kIOPCIResourceTypeMemory;
        }
    }

    // Probe prefetchable memory base and limit

    end = configRead32(bridge->space, kPCI2PCIPrefetchMemoryRange);
    if (end)
    {
        start = (end & 0xfff0) << 16;
        end  |= 0x000fffff;
        if (end > start)
        {
            range = &bridge->ranges[kIOPCIRangeBridgePFMemory];
            range->start = start;
            range->size  = end - start + 1;
            range->alignment = kPCIBridgeMemoryAlignment;
            range->type = kIOPCIResourceTypePrefetchMemory;
        }
    }

    // Probe I/O base and limit

    end = configRead32(bridge->space, kPCI2PCIIORange);
    if (end && ((end & (0x0e0e)) == 0))
    {
        // I/O Base and Limit register at dword 7 (0x1c).
        // If bridge does not implement an I/O address range, then both
        // the I/O Base and I/O Limit registers must be implemented as
        // read-only registers that return zero when read. The bottom
        // and the top of the I/O address range will always be aligned
        // to a 4KB boundary.
        //
        //  I/O Limit  |  I/O  Base
        // 7...4 3...0 | 7...4 3...0 
        //  ^     ^       ^     ^
        //  |     |       |     |
        //  |     |       |     +- 0 for 16bit decode, 1 for 32-bit decode
        //  |     |       |
        //  |     |       +-  Upper hex digit of 16-bit or 32-bit I/O range
        //  |     |           start address. Read-only field.
        //  |     |
        //  |     +- 0 for 16bit decode, 1 for 32-bit decode
        //  |
        //  +- Upper hex digit of 16-bit or 32-bit I/O range end address.
        //     Read-write field.
        
        start = (end & 0xf0) << 8;
        end   = (end & 0xffff) | 0xfff;

        // Limit may be less than the base, when there are no I/O addresses
        // on the secondary side of the bridge. Or when BIOS has failed to
        // assign I/O resources to devices behind the bridge.

        if (end > start)
        {
            range = &bridge->ranges[kIOPCIRangeBridgeIO];
            range->start = start;
            range->size  = end - start + 1;
            range->alignment = kPCIBridgeIOAlignment;
            range->type = kIOPCIResourceTypeIO;
        }
    }

    DLOG("    bridge bus %x:%x I/O %04llx:%04llx MEM %08llx:%08llx PFM %08llx:%08llx\n",
         bridge->secBusNum, bridge->subBusNum,
         bridge->ranges[kIOPCIRangeBridgeIO].start,
         bridge->ranges[kIOPCIRangeBridgeIO].size,
         bridge->ranges[kIOPCIRangeBridgeMemory].start,
         bridge->ranges[kIOPCIRangeBridgeMemory].size,
         bridge->ranges[kIOPCIRangeBridgePFMemory].start,
         bridge->ranges[kIOPCIRangeBridgePFMemory].size);
}

//---------------------------------------------------------------------------

void CLASS::pciYentaProbeRanges( pci_dev_t bridge )
{
    IOPCIRange * range;

    pciBridgeProbeBusRange(bridge);

    // Maximal bus range.

    range = &bridge->ranges[kIOPCIRangeBridgeBusNumber];
    range->flags    |= kIOPCIRangeFlagMaximizeSize;

    // 4K register space

    range = &bridge->ranges[kIOPCIRangeBAR0];
    range->start     = 0;
    range->size      = 4096;
    range->alignment = 4096;
    range->type      = kIOPCIResourceTypeMemory;
    range->flags     = 0;

    // Maximal memory and I/O range.

    range = &bridge->ranges[kIOPCIRangeBridgeIO];
    range->start     = 0;
    range->size      = kPCIBridgeIOAlignment;
    range->alignment = kPCIBridgeIOAlignment;
    range->type      = kIOPCIResourceTypeIO;
    range->flags     = kIOPCIRangeFlagMaximizeSize;

    range = &bridge->ranges[kIOPCIRangeBridgeMemory];
    range->start     = 0;
    range->size      = kPCIBridgeMemoryAlignment;
    range->alignment = kPCIBridgeMemoryAlignment;
    range->type      = kIOPCIResourceTypeMemory;
    range->flags     = kIOPCIRangeFlagMaximizeSize;

    bridge->deviceState = kPCIDeviceStateResourceWait;
}

//---------------------------------------------------------------------------

#ifndef ExtractLSB(x)
#define ExtractLSB(x) ((x) & (~((x) - 1)))
#endif

void CLASS::pciCheckCacheLineSize( pci_dev_t device )
{
    UInt8 cls, was;

    if (!(fFlags & kIOPCIConfiguratorAllocate))
	return;

    cls = configRead8(device->space, kIOPCIConfigCacheLineSize);
    was = cls;

    if (fCacheLineSize)
    {
        // BIOS config looks reasonable, keep original value
        if ((cls >= fCacheLineSize) && ((cls % fCacheLineSize) == 0))
            return;
    }
    else
    {
        // PCI cache line size must be a power of 2
        if (ExtractLSB(cls) == cls)
            return;
    }

    configWrite8(device->space, kIOPCIConfigCacheLineSize, fCacheLineSize);
    cls = configRead8(device->space, kIOPCIConfigCacheLineSize);
    if (cls != fCacheLineSize)
    {
        configWrite8(device->space, kIOPCIConfigCacheLineSize, 0);
    }
    else
    {
        DLOG("  changed CLS from %u to %u dwords\n", was, cls);
    }
}

//---------------------------------------------------------------------------

void CLASS::pciWriteLatencyTimer( pci_dev_t device )
{
    const UInt8 defaultLT = 0x40;

    if (device == fPCIBridgeList[0])
	return;

    // Nothing fancy here, just set the latency timer to 64 PCI clocks.

    configWrite8(device->space, kIOPCIConfigLatencyTimer, defaultLT);
    DLOG("  changed LT to %u PCI clocks\n",
         configRead8(device->space, kIOPCIConfigLatencyTimer));

    // Bridges can act as an initiator on either side of the bridge,
    // and there is a separate register for the latency timer on the
    // secondary side.

    if (device->isBridge)
    {
        configWrite8(device->space, kPCI2PCISecondaryLT, defaultLT);
        DLOG("  changed SEC-LT to %u PCI clocks\n",
             configRead8(device->space, kPCI2PCISecondaryLT));
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool CLASS::createRoot( IOService * provider )
{
    pci_dev_t bridge;

    bridge = IONew(pci_dev, 1);
    if (!bridge) return (false);

    memset(bridge, 0, sizeof(*bridge));
    bridge->classCode     = 0x060000;
    bridge->headerType   = kPCIHeaderType1;
    bridge->secBusNum    = fRootBridge->firstBusNum();
    bridge->subBusNum    = fRootBridge->lastBusNum();
    bridge->space	 = fRootBridge->getBridgeSpace();

    bridge->ranges[kIOPCIRangeBridgeBusNumber].start     = bridge->secBusNum;
    bridge->ranges[kIOPCIRangeBridgeBusNumber].size      = bridge->subBusNum - bridge->secBusNum + 1;
    bridge->ranges[kIOPCIRangeBridgeBusNumber].alignment = 1;
    bridge->ranges[kIOPCIRangeBridgeBusNumber].type      = kIOPCIResourceTypeBusNumber;
    bridge->ranges[kIOPCIRangeBridgeBusNumber].flags     = 0;

    bridge->dtNub           = fRootBridge->getProvider();
    bridge->acpiDevice      = IOPCICopyACPIDevice(bridge->dtNub);
    bridge->isHostBridge    = (!OSDynamicCast(IOPCIDevice, bridge->dtNub));
    bridge->isBridge        = true;
    bridge->supportsHotPlug = IOPCIIsHotplugPort(bridge->dtNub);

    fPCIBridgeList[0] = bridge;

    return true;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Configuration Space Access
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

UInt32 CLASS::configRead32( IOPCIAddressSpace space, UInt32 offset )
{
    return (fRootBridge->configRead32(space, offset));
}

void CLASS::configWrite32( IOPCIAddressSpace space, 
                           UInt32 offset, UInt32 data )
{
    fRootBridge->configWrite32(space, offset, data);
}

UInt16 CLASS::configRead16( IOPCIAddressSpace space, UInt32 offset )
{
    return (fRootBridge->configRead16(space, offset));
}

void CLASS::configWrite16( IOPCIAddressSpace space, 
                           UInt32 offset, UInt16 data )
{
    fRootBridge->configWrite16(space, offset, data);
}

UInt8 CLASS::configRead8( IOPCIAddressSpace space, UInt32 offset )
{
    return (fRootBridge->configRead8(space, offset));
}

void CLASS::configWrite8( IOPCIAddressSpace space, 
                          UInt32 offset, UInt8 data )
{
    fRootBridge->configWrite8(space, offset, data);
}
