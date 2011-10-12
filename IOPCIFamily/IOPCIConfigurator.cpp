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
#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/pci/IOPCIConfigurator.h>
#if ACPI_SUPPORT
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#endif
#include <libkern/sysctl.h>

__BEGIN_DECLS

#if defined(__i386__) || defined(__x86_64__)

#include <i386/cpuid.h>
#include <i386/cpu_number.h>

extern void mp_rendezvous_no_intrs(
               void (*action_func)(void *),
               void *arg);
#else

#define NO_RENDEZVOUS_KERNEL    1
#define cpu_number()    (0)

#endif

__END_DECLS

#define PFM64_SIZE    (0xF0000000ULL)

#define DLOGC(configurator, fmt, args...)                  \
    do {                                    \
        if ((configurator->fFlags & kIOPCIConfiguratorIOLog) && !ml_at_interrupt_context())   \
            IOLog(fmt, ## args);            \
        if (configurator->fFlags & kIOPCIConfiguratorKPrintf) \
            kprintf(fmt, ## args);          \
    } while(0)


#define DLOG(fmt, args...)      DLOGC(this, fmt, ## args);

#define DLOG_RANGE(name, range) if (range) {                                \
                DLOG(name": start/size   = 0x%08llx:0x%08llx(0x%08llx)\n",  \
                                        range->start,                       \
                                        range->size,                        \
                                        range->proposedSize) }

static const char * gPCIResourceTypeName[kIOPCIResourceTypeCount] =
{
    "MEM", "PFM", "I/O", "BUS"
};

#define BRN(type)      ((type) + kIOPCIRangeBridgeMemory)


static const IOPCIScalar minBridgeAlignments[kIOPCIResourceTypeCount] = {
    kPCIBridgeMemoryAlignment, kPCIBridgeMemoryAlignment, 
    kPCIBridgeIOAlignment, kPCIBridgeBusNumberAlignment
};

static const IOPCIScalar maxBridgeAddressDefault[kIOPCIResourceTypeCount] = {
    0xFFFFFFFF, 0xFFFFFFFF, 
    0xFFFF, 0xFF
};

// Never allocate ranges below 1K I/O and 1MB Memory to avoid
// stomping on legacy ISA, and VGA ranges.
static const IOPCIScalar minBARAddressDefault[kIOPCIResourceTypeCount] = {
    kPCIBridgeMemoryAlignment, kPCIBridgeMemoryAlignment, 
    0x400, 0
};

static const IOPCIScalar maxiRangeSizes[kIOPCIResourceTypeCount] = {
    1024*1024*1024, 1024*1024*1024, 
    0x8000, 0xC0
};

static UInt8 IOPCIIsHotplugPort(IORegistryEntry * bridgeDevice);
#if ACPI_SUPPORT
static IOACPIPlatformDevice * IOPCICopyACPIDevice(IORegistryEntry * device);
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define CLASS IOPCIConfigurator
#define super IOService

OSDefineMetaClassAndStructors( IOPCIConfigurator, IOService )

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool CLASS::init(IOWorkLoop * wl, uint32_t flags)
{
    if (!super::init()) return false;

    fWL    = wl;
    fFlags = flags;

    // Fetch global resources
    if (!createRoot())
        return false;

    return (true);
}

IOWorkLoop * CLASS::getWorkLoop() const
{
    return (fWL);
}

IOReturn CLASS::configOp(IOService * device, uintptr_t op, void * result)
{
    IOReturn ret = kIOReturnUnsupported;
    IOPCIConfigEntry * entry = NULL;
    IOPCIDevice *      pciDevice;

    if ((pciDevice = OSDynamicCast(IOPCIDevice, device)))
        entry = pciDevice->reserved->configEntry;

    switch (op)
    {
        case kConfigOpAddHostBridge:
            ret = addHostBridge(OSDynamicCast(IOPCIBridge, device));
			return (ret);
            break;

        case kConfigOpGetState:
			*((uint32_t *) result) = entry ? entry->deviceState : kPCIDeviceStateDead;
			ret = kIOReturnSuccess;
			return (ret);
            break;
    }

	if (!entry) return (kIOReturnBadArgument);

	switch (op)
    {

        case kConfigOpNeedsScan:
			entry->deviceState &= ~kPCIDeviceStateScanned;
			entry->rangeBaseChanges |= ((1 << kIOPCIRangeBridgeMemory)
									  |  (1 << kIOPCIRangeBridgePFMemory));
			ret = kIOReturnSuccess;
            break;

        case kConfigOpScan:
			if (kPCIDeviceStateScanned & entry->deviceState)
			{
				*((void **)result) = NULL;
			}
			else
        	{
				DLOG("[ PCI configuration begin ]\n");
		
				fChangedServices = OSSet::withCapacity(8);
		
				configure(0);
				DLOG("[ PCI configuration end, bridges %d devices %d]\n", fBridgeCount, fDeviceCount);
		
				*((void **)result) = fChangedServices;
				fChangedServices = 0;
			}
			ret = kIOReturnSuccess;
			return (ret);
            break;


        case kConfigOpEject:
			entry->deviceState |= kPCIDeviceStateEjected;
			ret = kIOReturnSuccess;
            break;

        case kConfigOpTerminated:
			DLOG("kConfigOpTerminated at %u:%u:%u vendor/product 0x%08x\n",
				 PCI_ADDRESS_TUPLE(entry), entry->vendorProduct);

//			if (0x590111c1 != entry->vendorProduct)
//          if (!(kPCIDeviceStateEjected & entry->deviceState))
			{
				bridgeRemoveChild(entry->parent, entry, 
									kIOPCIRangeAllBarsMask, kIOPCIRangeAllBarsMask, NULL);
			}
			ret = kIOReturnSuccess;
            break;

		case kConfigOpProtect:
			entry->deviceState = (entry->deviceState & ~(kPCIDeviceStateConfigRProtect|kPCIDeviceStateConfigWProtect))
								| (*((uint32_t *) result));
			DLOG("kConfigOpProtect at %u:%u:%u vendor/product 0x%08x prot %x\n",
				 PCI_ADDRESS_TUPLE(entry), entry->vendorProduct, *((uint32_t *) result));

			ret = kIOReturnSuccess;
            break;
    }

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if ACPI_SUPPORT
void CLASS::removeFixedRanges(IORegistryEntry * root)
{
	IORegistryIterator * iter;
	IOService *          service;
	OSOrderedSet *       all;
	OSArray *            array;
	IOMemoryDescriptor * mem;		
	uint32_t             idx, type;
    IOPCIRange *         range;
    uint64_t             start, size;
	bool                 ok;

	range = NULL;
	iter = IORegistryIterator::iterateOver(root, gIOPCIACPIPlane, kIORegistryIterateRecursively);
	if (iter)
	{
		all = iter->iterateAll();
		iter->release();
		if (all)
		{
			for ( ; (service = (IOService *) all->getFirstObject()); all->removeObject(service))
			{
				array = service->getDeviceMemory();
				if (!array)
					continue;
				for (idx = 0; (mem = (IOMemoryDescriptor *) array->getObject(idx)); idx++)
				{
					type  = mem->getTag();
					start = mem->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
					size  = mem->getLength();
					if (kIOACPIMemoryRange == type)
						type = kIOPCIResourceTypeMemory;
					else if (kIOACPIIORange == type)
					{
						if (start < 0x1000)
							continue;
						if ((start == 0xFFFF) && (size == 0x0001))
							continue;
						type = kIOPCIResourceTypeIO;
					}
					else
						continue;
					if (!range)
						range = IOPCIRangeAlloc();
					IOPCIRangeInit(range, type, start, size, 1);
					ok = IOPCIRangeListAllocateSubRange(fRoot->ranges[BRN(type)], range);
					DLOG("%s: %sfixed alloc type %d, 0x%llx len 0x%llx\n", 
							service->getName(), ok ? "" : "!", type, start, size);
					if (ok)
						range = NULL;
				}
			}
			all->release();
		}	
	}
	if (range)
		IOPCIRangeFree(range);
}
#endif

bool CLASS::createRoot(void)
{
    IOPCIConfigEntry * root;
    IOPCIRange *       range;
    uint64_t           start, size;
	uint32_t           cpuPhysBits;

    root = IONew(IOPCIConfigEntry, 1);
    if (!root) return (false);

    memset(root, 0, sizeof(*root));

    root->isHostBridge = true;
    root->isBridge     = true;

    root->secBusNum    = 0xff;

	cpuPhysBits = cpuid_info()->cpuid_address_bits_physical;
	start = (1ULL << cpuPhysBits) - PFM64_SIZE;
    size  = PFM64_SIZE;
	IOLog("PFM64 (%d cpu) 0x%llx, 0x%llx\n", cpuPhysBits, start, size);
	if (cpuPhysBits > 32)
	{
		range = IOPCIRangeAlloc();
		IOPCIRangeInit(range, kIOPCIResourceTypeMemory, start, size, 1);
		range->size = range->proposedSize;
		root->ranges[kIOPCIRangeBridgeMemory] = range;
	}
    root->deviceState |= kPCIDeviceStateScanned | kPCIDeviceStateConfigurationDone;

    fRoot = root;

    return true;
}

IOReturn CLASS::addHostBridge(IOPCIBridge * hostBridge)
{
    IOPCIConfigEntry * bridge;
	IOPCIAddressSpace  space;
    IOPCIRange *       range;
    uint64_t           start, size;

    bridge = IONew(IOPCIConfigEntry, 1);
    if (!bridge) return (kIOReturnNoMemory);

    memset(bridge, 0, sizeof(*bridge));
    bridge->classCode     = 0x060000;
    bridge->headerType   = kPCIHeaderType1;
    bridge->secBusNum    = hostBridge->firstBusNum();
    bridge->subBusNum    = hostBridge->lastBusNum();
    bridge->space        = hostBridge->getBridgeSpace();

    bridge->dtNub           = hostBridge->getProvider();
#if ACPI_SUPPORT
    bridge->acpiDevice      = IOPCICopyACPIDevice(bridge->dtNub);
#endif
    bridge->isHostBridge    = true;
    bridge->isBridge        = true;
    bridge->supportsHotPlug = false;

    if (OSDynamicCast(IOPCIDevice, bridge->dtNub)) panic("!host bridge");

    // fix - all config cycles use first host bridge
    if (!fHostBridge)
	{
        fHostBridge = hostBridge;
		space.bits = 0;
        fRootVendorProduct = configRead32(space, kIOPCIConfigVendorID);
		if((0x27A08086 == fRootVendorProduct)
		  || (0x27AC8086 == fRootVendorProduct)
		  || (0x25C08086 == fRootVendorProduct))
			fFlags &= ~kIOPCIConfiguratorPFM64;
		DLOG("root id 0x%x, flags 0x%x\n", fRootVendorProduct, (int) fFlags);
	}

    range = IOPCIRangeAlloc();
    start = bridge->secBusNum;
    size  = bridge->subBusNum - bridge->secBusNum + 1;

    DLOG("host bridge acpi bus range 0x%llx len 0x%llx\n", start, size);

    IOPCIRangeInit(range, kIOPCIResourceTypeBusNumber, start, size, kPCIBridgeBusNumberAlignment);
    range->size = range->proposedSize;
    bridge->ranges[kIOPCIRangeBridgeBusNumber] = range;

    DLOG("added(%s) host bridge resource %s 0x%llx len 0x%llx\n", 
        "ok", gPCIResourceTypeName[kIOPCIResourceTypeBusNumber], 
        range->start, range->size);

    for (int type = 0; type < kIOPCIResourceTypeCount; type++)
    {
        IOPCIRange * thisRange;
        for (thisRange = hostBridge->reserved->rangeLists[type];
			 thisRange;
			 thisRange = thisRange->next)
        {
            bool ok;
			start = thisRange->start;
			size  = thisRange->size;
            DLOG("reported host bridge resource %s 0x%llx len 0x%llx\n", 
                gPCIResourceTypeName[type], 
                start, size);
            ok = IOPCIRangeListAddRange(&fRoot->ranges[BRN(type)],
                                        type, start, size);
            DLOG("added(%s) host bridge resource %s 0x%llx len 0x%llx\n", 
                ok ? "ok" : "!", gPCIResourceTypeName[type], 
                start, size);
        }
    }

#if ACPI_SUPPORT
	if (bridge->acpiDevice)
		removeFixedRanges(bridge->acpiDevice);
#endif

    bridge->deviceState |= kPCIDeviceStateConfigurationDone;

    bridgeAddChild(fRoot, bridge);

    configure(kIOPCIConfiguratorBoot);

    return (kIOReturnSuccess);
}

void CLASS::free( void )
{
    super::free();
}

//---------------------------------------------------------------------------

void CLASS::constructAddressingProperties(IOPCIConfigEntry * device, OSDictionary * propTable)
{
    IOPCIRange *         range;
    IOPCIPhysicalAddress regData;
    OSString *           string;
    OSData *             prop;
    OSData *             regProp;
    OSData *             assignedProp;
    OSData *             rangeProp;
    char                 tuple[64];
    size_t               len;

    len = snprintf(tuple, sizeof(tuple), "%u:%u:%u", PCI_ADDRESS_TUPLE(device));
    if (device->isBridge && (len < sizeof(tuple)))
    {
        snprintf(&tuple[len], sizeof(tuple) - len, "(%u:%u)", device->secBusNum, device->subBusNum);
    }
    string = OSString::withCString(tuple);
    if (string)
    {
        propTable->setObject("pcidebug", string);
        string->release();
    }

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
        static const uint8_t barRegisters[kIOPCIRangeExpansionROM + 1] = {
            kIOPCIConfigBaseAddress0, kIOPCIConfigBaseAddress1, kIOPCIConfigBaseAddress2,
            kIOPCIConfigBaseAddress3, kIOPCIConfigBaseAddress4, kIOPCIConfigBaseAddress5,
            kIOPCIConfigExpansionROMBase
        };

        range = device->ranges[i];
        if (!range)
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
            if (range->start)
            {
				IOPCIPhysicalAddress assignedData;
				assignedData = regData;
                assignedData.physHi.s.reloc = 1;
                assignedData.physMid = (range->start >> 32ULL);
                assignedData.physLo  = range->start;
                assignedProp->appendBytes(&assignedData, sizeof(assignedData));
            }
			// reg gets requested length
			regData.lengthHi             = (range->proposedSize >> 32ULL);
			regData.lengthLo             = range->proposedSize;
            regProp->appendBytes(&regData, sizeof(regData));
        }
        else
        {
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
        prop = OSData::withBytes(&regData.lengthLo, sizeof(regData.lengthLo));
        if (prop)
        {
            propTable->setObject("#address-cells", prop );
            prop->release();
        }
        regData.lengthLo = 2;
        prop = OSData::withBytes(&regData.lengthLo, sizeof(regData.lengthLo));
        if (prop)
        {
            propTable->setObject("#size-cells", prop );
            prop->release();
        }
    }
    rangeProp->release();
}

OSDictionary * CLASS::constructProperties(IOPCIConfigEntry * device)
{
    IOPCIAddressSpace   space = device->space;
    OSDictionary *      propTable;
    uint32_t            value;
    uint32_t            vendor, product, classCode, revID;
    uint32_t            subVendor = 0, subProduct = 0;
    OSData *            prop;
    const char *        name;
    const OSSymbol *    nameProp;
    char                compatBuf[128];
    char *              out;

    struct IOPCIGenericNames
    {
        const char *    name;
        UInt32          mask;
        UInt32          classCode;
    };
    static const IOPCIGenericNames genericNames[] = {
                { "display",    0xffffff, 0x000100 },
                { "scsi",       0xffff00, 0x010000 },
                { "ethernet",   0xffff00, 0x020000 },
                { "display",    0xff0000, 0x030000 },
                { "pci-bridge", 0xffff00, 0x060400 },
                { 0, 0, 0 }
            };
    const IOPCIGenericNames *   nextName;

    propTable = OSDictionary::withCapacity( 8 );
    if (!propTable)
        return (NULL);

    constructAddressingProperties(device, propTable);

    value = configRead32( device, kIOPCIConfigVendorID );
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

    value = configRead32( device, kIOPCIConfigRevisionID );
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

    value = configRead32( device, kIOPCIConfigSubSystemVendorID );
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
        out += snprintf(out, sizeof("pcivvvv,pppp"), "pci%x,%x", subVendor, subProduct) + 1;

    if (0 == name)
        name = out;

    out += snprintf(out, sizeof("pcivvvv,pppp"), "pci%x,%x", vendor, product) + 1;
    out += snprintf(out, sizeof("pciclass,cccccc"), "pciclass,%06x", classCode) + 1;

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

    if (kPCIHotPlugRoot == device->supportsHotPlug)
        propTable->setObject(kIOPCIHotPlugKey, kOSBooleanTrue);
    else if (kPCILinkChange == device->supportsHotPlug)
        propTable->setObject(kIOPCILinkChangeKey, kOSBooleanTrue);

	if (kPCIHotPlugTunnel == device->parent->supportsHotPlug)
        propTable->setObject(kIOPCITunnelledKey, kOSBooleanTrue);

	if (device->linkInterrupts)
        propTable->setObject(kIOPCITunnelLinkChangeKey, kOSBooleanTrue);

	if (device->linkInterrupts || (kPCIHotPlugRoot == device->supportsHotPlug))
	{
		bool bootDefer = (kIOPCIConfiguratorBootDefer == (kIOPCIConfiguratorBootDefer & fFlags));
		if (bootDefer)
			propTable->setObject(kIOPCITunnelBootDeferKey, kOSBooleanTrue);
		else if (!(kPCIDeviceStateNoLink & device->deviceState))
			propTable->setObject(kIOPCIOnlineKey, kOSBooleanTrue);
	}

    return (propTable);
}

//---------------------------------------------------------------------------

#if ACPI_SUPPORT

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

#endif /* ACPI_SUPPORT */

//---------------------------------------------------------------------------

static UInt8 IOPCIIsHotplugPort(IORegistryEntry * bridgeDevice)
{
    UInt8                  type = kPCIStatic;

#if ACPI_SUPPORT

    IOACPIPlatformDevice * rp;
    IOACPIPlatformDevice * child;

    rp = IOPCICopyACPIDevice(bridgeDevice);
    if (rp && gIOPCIACPIPlane)
    {
        child = (IOACPIPlatformDevice *) rp->getChildEntry(gIOPCIACPIPlane);
        if (child)
        {
            IOReturn   ret;
            UInt32     result32 = 0;
            OSObject * obj;

            ret = child->evaluateInteger("_RMV", &result32);
            if (kIOReturnSuccess == ret)
            {
                if (result32)
                    type = kPCIHotPlugRoot;
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

#endif /* ACPI_SUPPORT */

    return (type);
}

//---------------------------------------------------------------------------

struct MatchDTEntryContext
{
    CLASS *     me;
    IOPCIConfigEntry *   bridge;
};

void CLASS::matchDTEntry( IORegistryEntry * dtEntry, void * _context )
{
    MatchDTEntryContext * context = (MatchDTEntryContext *) _context;
    IOPCIConfigEntry *    bridge = context->bridge;
    IOPCIConfigEntry *    match = 0;
    const OSSymbol *      location;     

    assert(bridge);
    assert(dtEntry);

    location = dtEntry->copyLocation();
    if (!location)
        return;

    uint32_t devfn = strtoul(location->getCStringNoCopy(), NULL, 16);
    uint32_t deviceNum   = ((devfn >> 16) & 0x1f);
    uint32_t functionNum = (devfn & 0x7);
    bool     functionAll = ((devfn & 0xffff) == 0xffff);

    FOREACH_CHILD(bridge, child)
    {
        if (kPCIDeviceStateDead & child->deviceState)
            continue;
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
        if (!match->dtNub)
        {
            match->dtNub = dtEntry;
            DLOGC(context->me, "Found PCI device for DT entry [%s] %u:%u:%u\n",
                 dtEntry->getName(), PCI_ADDRESS_TUPLE(match));
        }
    }
    else
    {
        DLOGC(context->me, "NOT FOUND: PCI device for DT entry [%s] %u:%u:%u\n", 
                dtEntry->getName(), bridge->secBusNum, deviceNum, functionNum);
    }

    if (location)
        location->release();
}

#if ACPI_SUPPORT

void CLASS::matchACPIEntry( IORegistryEntry * dtEntry, void * _context )
{
    MatchDTEntryContext * context = (MatchDTEntryContext *) _context;
    IOPCIConfigEntry *             bridge = context->bridge;
    IOPCIConfigEntry *             match = 0;
    OSNumber *            adr;

    assert(bridge);
    assert(dtEntry);

    adr = OSDynamicCast(OSNumber, dtEntry->getProperty("_ADR"));
    if (!adr)
        return;

    uint32_t devfn = adr->unsigned32BitValue();
    uint32_t deviceNum = ((devfn >> 16) & 0x1f);
    uint32_t functionNum = (devfn & 0x7);
    bool     functionAll = ((devfn & 0xffff) == 0xffff);

    FOREACH_CHILD( bridge, child )
    {
        if (kPCIDeviceStateDead & child->deviceState)
            continue;
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
        if (!match->acpiDevice)
        {
            match->acpiDevice = dtEntry;
            DLOGC(context->me, "Found PCI device for ACPI entry [%s] %u:%u:%u\n",
                 dtEntry->getName(), PCI_ADDRESS_TUPLE(match));
        }
    }
    else
    {
        DLOGC(context->me, "NOT FOUND: PCI device for ACPI entry [%s] %u:%u:%u\n",
                dtEntry->getName(), bridge->secBusNum, deviceNum, functionNum);
    }
}

#endif /* ACPI_SUPPORT */

//---------------------------------------------------------------------------

void CLASS::bridgeConnectDeviceTree(IOPCIConfigEntry * bridge)
{
    IORegistryEntry *   dtBridge = bridge->dtNub;
	OSObject *          obj;
    MatchDTEntryContext context;
	IOPCIRange *        childRange;
	bool				oneChild;

    context.me     = this;
    context.bridge = bridge;
    if (dtBridge)
    {
        dtBridge->applyToChildren(&matchDTEntry, &context, gIODTPlane);
    }
#if ACPI_SUPPORT
    if (gIOPCIACPIPlane && bridge->acpiDevice)
    {
        bridge->acpiDevice->applyToChildren(&matchACPIEntry, &context, gIOPCIACPIPlane);
    }
#endif /* ACPI_SUPPORT */

	oneChild = (bridge->child && !bridge->child->peer);

    FOREACH_CHILD(bridge, child)
    {
		if (oneChild
			&& ((child->classCode & 0xFFFFFF) == 0x088000)
			&& (kPCIHotPlugTunnel == bridge->supportsHotPlug)
			&& (0x60 == (0xf0 & bridge->expressCaps)))
		{
			// downstream port with one child 
			bridge->supportsHotPlug = kPCIStatic;
			DLOG("tunnel controller %u:%u:%u\n", PCI_ADDRESS_TUPLE(bridge));
		}

        if (!child->isBridge)
            continue;
		do
		{
			if ((kPCIHotPlugTunnelRoot == bridge->supportsHotPlug)
			  || (kPCIHotPlugTunnel == bridge->supportsHotPlug))
			{
				child->supportsHotPlug = kPCIHotPlugTunnel;
				continue;
			}
	
			if ((kPCIHotPlugRoot == bridge->supportsHotPlug)
			  || (kPCIHotPlug == bridge->supportsHotPlug))
			{
				child->supportsHotPlug = kPCIHotPlug;
				continue;
			}

			if (child->dtNub 
				&& ((obj = child->dtNub->copyProperty("PCI-Thunderbolt", gIODTPlane, 
													   kIORegistryIterateRecursively))))
			{
				obj->release();
				child->supportsHotPlug = kPCIHotPlugTunnelRoot;
				continue;
			}

			if (child->headerType == kPCIHeaderType2)
				child->supportsHotPlug = kPCIHotPlugRoot;
			else if (child->dtNub)
			{
				child->supportsHotPlug = IOPCIIsHotplugPort(child->dtNub);
			}
			else
				child->supportsHotPlug = kPCIStatic;
		}
		while (false);
		DLOG("bridge %u:%u:%u supportsHotPlug %d, linkInterrupts %d\n",
			  PCI_ADDRESS_TUPLE(child),
			  child->supportsHotPlug, child->linkInterrupts);

        for (int i = kIOPCIRangeBridgeMemory; i < kIOPCIRangeCount; i++)
        {
            childRange = child->ranges[i];
            if (!childRange)
                continue;

			if ((kPCIHotPlugTunnelRoot == child->supportsHotPlug)
			|| ((kPCIHotPlugTunnelRoot == bridge->supportsHotPlug) && oneChild))
			{
				childRange->flags |= kIOPCIRangeFlagMaximizeFlags;
				childRange->proposedSize = maxiRangeSizes[childRange->type];
				if ((kIOPCIRangeBridgeBusNumber == i) 
					&& (kPCIHotPlugTunnelRoot != child->supportsHotPlug))
					childRange->proposedSize--;
				child->rangeSizeChanges |= (1 << BRN(childRange->type));
			}
			if (child->linkInterrupts)
			{
				childRange->flags |= kIOPCIRangeFlagSplay;
			}
		}
    }
}

//---------------------------------------------------------------------------

bool CLASS::bridgeConstructDeviceTree(void * unused, IOPCIConfigEntry * bridge)
{
    IORegistryEntry *   dtBridge = bridge->dtNub;
    uint32_t            int32;
    bool                ok = true;
    IOPCIDevice *       nub;

//    DLOG("bridgeConstructDeviceTree(%p)\n", bridge);
    if (!dtBridge)
        return (ok);

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

        if (kPCIDeviceStatePropertiesDone & child->deviceState)
            continue;
        child->deviceState |= kPCIDeviceStatePropertiesDone;

        DLOG("bridgeConstructDeviceTree at %u:%u:%u vendor/product 0x%08x\n",
             PCI_ADDRESS_TUPLE(child), child->vendorProduct);

        propTable = constructProperties(child);
        if (!propTable)
            continue;

        if (!child->dtNub)
        {
#if ACPI_SUPPORT
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
#endif /* ACPI_SUPPORT */

            nub = OSTypeAlloc(IOPCIDevice);
			if (!nub)
				continue;
			ok = (nub->init(propTable) 
				&& nub->attachToParent(bridge->dtNub, gIODTPlane));
			nub->release();
			if (!ok)
				continue;
            nub->reserved->configEntry = child;
            child->dtNub = nub;
            if ((sym = OSDynamicCast(OSSymbol, propTable->getObject(gIONameKey))))
                child->dtNub->setName(sym);

#if ACPI_SUPPORT
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
#endif /* ACPI_SUPPORT */
        }
        else
        {
            OSCollectionIterator * propIter =
                OSCollectionIterator::withCollection(propTable);
            if (propIter)
            {
                const OSSymbol * propKey;

                child->dtNub->removeProperty("ranges");
                child->dtNub->removeProperty("reg");
                child->dtNub->removeProperty("assigned-addresses");
                child->dtNub->removeProperty("pcidebug");

                while ((propKey = (const OSSymbol *)propIter->getNextObject()))
                {
                    if (child->dtNub->getProperty(propKey))
                        continue;
                    obj = propTable->getObject(propKey);
                    child->dtNub->setProperty(propKey, obj);
                }
                propIter->release();
            }
//
            if (!OSDynamicCast(IOPCIDevice, child->dtNub))
            {
                nub = OSTypeAlloc(IOPCIDevice);
                nub->init(child->dtNub, gIODTPlane);
                nub->reserved->configEntry = child;
                child->dtNub = nub;
            }
//
        }
        propTable->release();
    }

//    if ((kIOPCIConfiguratorAllocate & fFlags)
//     || (kPCIHotPlug != bridge->supportsHotPlug))
    {
        dtBridge->setProperty(kIOPCIConfiguredKey, kOSBooleanTrue);
    }

    return (ok);
}

//---------------------------------------------------------------------------

void CLASS::bridgeScanBus(IOPCIConfigEntry * bridge, uint8_t busNum)
{
    IOPCIAddressSpace   space;
    IOPCIConfigEntry *  child;
    uint32_t		    noLink = 0;
    bool     			bootDefer = false;
    UInt8               scanDevice, scanFunction, lastFunction;
	uint32_t            linkStatus;
	enum
	{
		kLinkCapDataLinkLayerActiveReportingCapable = (1 << 20),
		kLinkStatusDataLinkLayerLinkActive 			= (1 << 13)
	};

	space.bits = 0;
	space.s.busNum = busNum;

	if (bridge->expressCapBlock) do
	{
		bootDefer = (bridge->linkInterrupts 
			&& (kIOPCIConfiguratorBootDefer == (kIOPCIConfiguratorBootDefer & fFlags)));
		if (bootDefer)
			break;

		linkStatus  = configRead16(bridge, bridge->expressCapBlock + 0x12);
		if ((kLinkCapDataLinkLayerActiveReportingCapable & bridge->linkCaps)
			&& !(kLinkStatusDataLinkLayerLinkActive & linkStatus))
		{
			noLink = kPCIDeviceStateNoLink;
		}

		if ((kPCIDeviceStateNoLink & bridge->deviceState) != noLink)
		{
			bridge->deviceState &= ~kPCIDeviceStateNoLink;
			bridge->deviceState |= noLink;
			bridge->rangeBaseChanges |= ((1 << kIOPCIRangeBridgeMemory)
									  |  (1 << kIOPCIRangeBridgePFMemory));
		}

		DLOG("bridge %u:%u:%u %s linkStatus 0x%04x, linkCaps 0x%04x\n",
			 PCI_ADDRESS_TUPLE(bridge), noLink ? "(nolink) " : "",
			 linkStatus, bridge->linkCaps);

		if (noLink)
		{
            configWrite32(bridge, kPCI2PCIMemoryRange,         0);
            configWrite32(bridge, kPCI2PCIPrefetchMemoryRange, 0);
            configWrite32(bridge, kPCI2PCIPrefetchUpperBase,   0);
            configWrite32(bridge, kPCI2PCIPrefetchUpperLimit,  0);

			IOPCIConfigEntry * next;
			for (child = bridge->child; child; child = next)
			{
				next = child->peer;
				bridgeDeadChild(bridge, child);
			}
		}
	}
	while (false);

    if (!bootDefer && !noLink)
    {
		// Scan all PCI devices and functions on the secondary bus.
		for (scanDevice = 0; scanDevice <= 31; scanDevice++)
		{
			lastFunction = 0;
			for (scanFunction = 0; scanFunction <= lastFunction; scanFunction++)
			{
				space.s.deviceNum   = scanDevice;
				space.s.functionNum = scanFunction;
	
				bridgeProbeChild(bridge, space);
	
				// look in function 0 for multi function flag
				if (0 == scanFunction)
				{
					uint32_t flags = configRead32(space, kIOPCIConfigCacheLineSize);
					if ((flags != 0xFFFFFFFF) && (0x00800000 & flags))
					{
						lastFunction = 7;
					}
				}
			}
		}
	}
}

//---------------------------------------------------------------------------

void CLASS::bridgeAddChild(IOPCIConfigEntry * bridge, IOPCIConfigEntry * child)
{
    IOPCIConfigEntry ** prev = &bridge->child;
    IOPCIConfigEntry *  next;

    // tailq
    while ((next = *prev))
    {
        prev = &next->peer;
    }
    *prev = child;
    child->parent = bridge;

    if (child->isBridge)
        fBridgeCount++;
    else
        fDeviceCount++;

    if (fChangedServices)
    {
        if (bridge->dtNub && bridge->dtNub->inPlane(gIOServicePlane))
            fChangedServices->setObject(bridge->dtNub);
    }
}

bool CLASS::bridgeDeallocateChildRanges(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead,
								        uint32_t deallocTypes, uint32_t freeTypes)
{
	IOPCIRange *        range;
	IOPCIRange *        childRange;
    bool				ok;
    bool				dispose;
    bool				didKeep = false;

    for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
	{
		childRange = dead->ranges[rangeIndex];
		if (!childRange)
			continue;
		if (!((1 << rangeIndex) & deallocTypes))
			continue;

		if ((kIOPCIRangeBridgeBusNumber == rangeIndex) && dead->busResv.nextSubRange)
		{
			ok = IOPCIRangeListDeallocateSubRange(childRange, &dead->busResv);
			if (!ok) panic("!IOPCIRangeListDeallocateSubRange busResv");
		}

		dispose = (0 != ((1 << rangeIndex) & freeTypes));

		if (childRange->nextSubRange)
		{
			range = bridgeGetRange(bridge, childRange->type);
			if (!range) panic("!range");
			childRange->proposedSize = childRange->size;
			ok = IOPCIRangeListDeallocateSubRange(range, childRange);
			if (!ok) panic("!IOPCIRangeListDeallocateSubRange");
			dead->rangeBaseChanges |= (1 << rangeIndex);
			if (!dispose)
			{
				didKeep = true;
			}
		}
		if (dispose)
		{
			IOPCIRangeFree(childRange);
			dead->ranges[rangeIndex] = NULL;
		}
	}

	return (didKeep);
}

void CLASS::bridgeRemoveChild(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead,
								uint32_t deallocTypes, uint32_t freeTypes,
								IOPCIConfigEntry ** childList)
{
    IOPCIConfigEntry ** prev;
    IOPCIConfigEntry *  child;
    IOPCIDevice *       pciDevice;
    bool				didKeep;

	dead->deviceState |= kPCIDeviceStateDead;

    while ((child = dead->child))
    {
        bridgeRemoveChild(dead, child, deallocTypes, freeTypes, childList);
    }

    DLOG("bridge %p removing child %p at %u:%u:%u vendor/product 0x%08x\n",
         bridge, dead, PCI_ADDRESS_TUPLE(dead), dead->vendorProduct);

	prev = &bridge->child;
	while ((child = *prev))
    {
        if (dead == child)
        {
            *prev = child->peer;
            break;
        }
        prev = &child->peer;
    }

	didKeep = bridgeDeallocateChildRanges(bridge, dead, deallocTypes, freeTypes);

    if (childList && didKeep)
	{
		dead->parent       = NULL;
		dead->peer         = *childList;
		*childList         = dead;
	}
	else
    {
		if ((pciDevice = OSDynamicCast(IOPCIDevice, dead->dtNub)))
		{
			pciDevice->reserved->configEntry = NULL;
		}
		if (dead->isBridge)
			fBridgeCount--;
		else
			fDeviceCount--;

//		memset(dead, 0xBB, sizeof(*dead));
		IODelete(dead, IOPCIConfigEntry, 1);
		DLOG("deleted %p, bridges %d devices %d\n", dead, fBridgeCount, fDeviceCount);
	}
}

void CLASS::bridgeMoveChildren(IOPCIConfigEntry * to, IOPCIConfigEntry * list)
{
	IOPCIConfigEntry * dead;
	IOPCIRange *       range;
	IOPCIRange *       childRange;
	bool		       ok;

	while (list)
	{
		dead = list;
		list = list->peer;
		for (int rangeIndex = 0; rangeIndex <= kIOPCIRangeExpansionROM; rangeIndex++)
		{
			childRange = dead->ranges[rangeIndex];
			if (!childRange)
				continue;
			if (!childRange->proposedSize)
				continue;
			if (!childRange->start)
				continue;

			range = bridgeGetRange(to, childRange->type);
			if (!range) panic("!range");

			// reserve the alloc until terminate
			if (!dead->parent)
			{
				// head Q
				dead->parent = to;
				dead->peer   = to->child;
				to->child  = dead;
				DLOG("Reserves on (%u:%u:%u) for (%u:%u:%u):\n",
						PCI_ADDRESS_TUPLE(to), PCI_ADDRESS_TUPLE(dead));
			}
			DLOG("  %s: 0x%llx reserve 0x%llx\n",
					gPCIResourceTypeName[childRange->type], 
					childRange->start, childRange->proposedSize);
			ok = IOPCIRangeListAllocateSubRange(range, childRange);
			if (!ok) panic("!IOPCIRangeListAllocateSubRange");
		}
	}
}

void CLASS::bridgeDeadChild(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead)
{
	IOPCIConfigEntry * pendingList = NULL;

    if (kPCIDeviceStateDead & dead->deviceState)
        return;

    DLOG("bridge %p dead child at %u:%u:%u vendor/product 0x%08x\n",
         bridge, PCI_ADDRESS_TUPLE(dead), dead->vendorProduct);

    if (fChangedServices)
    {
        if (dead->dtNub && dead->dtNub->inPlane(gIOServicePlane))
            fChangedServices->setObject(dead->dtNub);
    }

	bridgeRemoveChild(bridge, dead,
                        kIOPCIRangeAllMask, kIOPCIRangeAllBridgeMask,
						true ? &pendingList : NULL);

	if (pendingList)
    {
		bridgeMoveChildren(bridge, pendingList);
    }
}

void CLASS::bridgeProbeChild( IOPCIConfigEntry * bridge, IOPCIAddressSpace space )
{
    IOPCIConfigEntry * child = NULL;
    bool      ok = true;
    uint32_t  vendorProduct;
	uint8_t   reset;

	reset = (kPCIStatic != bridge->supportsHotPlug);

    vendorProduct = configRead32(space, kIOPCIConfigVendorID);

    if (reset
    	&& ((0 == (vendorProduct & 0xffff)) || (0xffff == (vendorProduct & 0xffff))))
	{
        configWrite32(space, kIOPCIConfigVendorID, 0);
		vendorProduct = configRead32(space, kIOPCIConfigVendorID);
	}

    for (child = bridge->child; child; child = child->peer)
    {
        if (kPCIDeviceStateDead & child->deviceState)
            continue;
        if (space.bits == child->space.bits)
        {
            DLOG("bridge %p scan existing child at %u:%u:%u vendor/product 0x%08x\n",
                 bridge, PCI_ADDRESS_TUPLE(child), child->vendorProduct);

            // check bars?

			if (!(kIOPCIConfiguratorNoTerminate & fFlags))
			{
				if ((vendorProduct != child->vendorProduct)
				|| (kPCIDeviceStateEjected & child->deviceState))
				{
					bridgeDeadChild(bridge, child);
					if (!(kPCIDeviceStateEjected & child->deviceState))
					{
						// create new if present
						child = NULL;
					}
				}
			}
            break;
        }
    }
    
    if (child)
        return;

    uint32_t retries = 1;
    while ((0 == (vendorProduct & 0xffff)) || (0xffff == (vendorProduct & 0xffff)))
    {
        if (!--retries)
            return;
//        vendorProduct = configRead32(space, kIOPCIConfigVendorID);
    }

    child = IONew(IOPCIConfigEntry, 1);
    if (!child) return;

    memset(child, 0, sizeof(*child));
    child->space         = space;
    child->headerType    = configRead8(child, kIOPCIConfigHeaderType) & 0x7f;
    child->classCode     = configRead32(child, kIOPCIConfigRevisionID) >> 8;
    child->vendorProduct = vendorProduct;

    DLOG("Probing type %u device class-code 0x%06x at %u:%u:%u [state 0x%x]\n",
         child->headerType, child->classCode,
         PCI_ADDRESS_TUPLE(child),
         child->deviceState);

    switch (child->headerType)
    {
        case kPCIHeaderType0:
            // skip devices aliased to host bridges
            if ((child->classCode & 0xFFFFFF) == 0x060000)
                break;

            deviceProbeRanges(child, reset);
            break;

        case kPCIHeaderType1:
            child->isBridge = true;
            bridgeProbeRanges(child, reset);
            break;

        case kPCIHeaderType2:
            child->isBridge = true;
            cardbusProbeRanges(child, reset);
            break;

        default:
            DLOG("  bad PCI header type 0x%x\n", child->headerType);
            ok = false;
            break;
    }

    if (!ok)
    {
        IODelete(child, IOPCIConfigEntry, 1);
		return;
    }

	if (findPCICapability(child, kIOPCIPCIExpressCapability, &child->expressCapBlock))
	{
		if (child->isBridge)
		{
			uint32_t expressCaps, linkCaps, linkControl;
			enum
			{
				kLinkCapDataLinkLayerActiveReportingCapable = (1 << 20),
				kLinkStatusDataLinkLayerLinkActive 			= (1 << 13)
			};
		
			expressCaps = configRead16(child, child->expressCapBlock + 0x02);
			linkCaps    = configRead32(child, child->expressCapBlock + 0x0c);
			linkControl = configRead16(child, child->expressCapBlock + 0x10);
	
			if ((kPCIHotPlugTunnel <= bridge->supportsHotPlug)
				&& (0x60 == (0xf0 & expressCaps))) // downstream port
			{
				if (kLinkCapDataLinkLayerActiveReportingCapable & linkCaps)
					child->linkInterrupts = true;
			}
	
			child->expressCaps        = expressCaps;
			child->linkCaps           = linkCaps;
			child->expressDeviceCaps1 = configRead32(child, child->expressCapBlock + 0x04);
	
			DLOG("  expressCaps 0x%04x, linkControl 0x%04x, linkCaps 0x%04x\n",
				 child->expressCaps, linkControl, child->linkCaps);
		}
		child->expressMaxPayload  = (child->expressDeviceCaps1 & 7);
		DLOG("  expressMaxPayload 0x%x\n", child->expressMaxPayload);
	}

	bridgeAddChild(bridge, child);
	checkCacheLineSize(child);
}

//---------------------------------------------------------------------------

uint32_t CLASS::findPCICapability(IOPCIConfigEntry * device,
                                  uint32_t capabilityID, uint32_t * found)
{
	return(findPCICapability(device->space, capabilityID, found));
}

uint32_t CLASS::findPCICapability(IOPCIAddressSpace space,
                                  uint32_t capabilityID, uint32_t * found)
{
    uint32_t data = 0;
    uint32_t offset, firstOffset = 0;

    if (found)
    {
        firstOffset = *found;
        *found = 0;
    }

    if (0 == ((kIOPCIStatusCapabilities << 16)
              & (configRead32(space, kIOPCIConfigCommand))))
        return (0);

    if (capabilityID >= 0x100)
    {
        capabilityID =- capabilityID;
        offset = 0x100;
        while (offset)
        {
            data = configRead32(space, offset);
            if ((offset > firstOffset) && (capabilityID == (data & 0xffff)))
            {
                if (found)
                    *found = offset;
                break;
            }
            offset = (data >> 20) & 0xfff;
            if ((offset < 0x100) || (offset & 3))
                offset = 0;
        }
    }
    else
    {
        offset = (0xff & configRead32(space, kIOPCIConfigCapabilitiesPtr));
        if (offset & 3)
            offset = 0;
        while (offset)
        {
            data = configRead32(space, offset);
            if ((offset > firstOffset) && (capabilityID == (data & 0xff)))
            {
                if (found)
                    *found = offset;
                break;
            }
            offset = (data >> 8) & 0xff;
            if (offset & 3)
                offset = 0;
        }
    }

    return (offset ? data : 0);
}

//---------------------------------------------------------------------------

struct BARProbeParam {
    CLASS *            target;
    IOPCIConfigEntry * device;
    uint32_t           lastBarNum;
    uint8_t            reset;
};

void CLASS::safeProbeCallback( void * refcon )
{
    BARProbeParam * param = (BARProbeParam *) refcon;
    assert(param);

    if (cpu_number() == 0)
    {
        param->target->probeBaseAddressRegister(
            param->device, param->lastBarNum, param->reset );
    }
    if (cpu_number() == 99)
    {
        IOLog("safeProbeCallback() gcc workaround\n");
    }
}

void CLASS::safeProbeBaseAddressRegister(IOPCIConfigEntry * device, 
										 uint32_t lastBarNum, uint8_t reset)
{
    uint32_t barNum;

    for (barNum = 0; barNum <= lastBarNum; barNum++)
    {
        device->ranges[barNum] = IOPCIRangeAlloc();
        IOPCIRangeInit(device->ranges[barNum], 0, 0, 0);
    }

    {
#if NO_RENDEZVOUS_KERNEL
        boolean_t       istate;
        istate = ml_set_interrupts_enabled(FALSE);
        pciProbeBaseAddressRegister(device, lastBarNum, reset);
        ml_set_interrupts_enabled(istate);
#else
        BARProbeParam param;

        param.target     = this;
        param.device     = device;
        param.lastBarNum = lastBarNum;
        param.reset      = reset;

        mp_rendezvous_no_intrs(&safeProbeCallback, &param);
#endif
    }

    for (barNum = 0; barNum <= lastBarNum; barNum++)
    {
        if (!device->ranges[barNum]->proposedSize)
        {
            IOPCIRangeFree(device->ranges[barNum]);
            device->ranges[barNum] = NULL;
        }
    }
}

void CLASS::probeBaseAddressRegister(IOPCIConfigEntry * device, uint32_t lastBarNum, uint8_t reset)
{
    IOPCIRange *    range;
    uint32_t        barNum, nextBarNum;
    uint32_t        value;
    uint64_t        saved, upper, barMask;
    uint64_t        start, size;
    uint32_t        type;
    uint16_t        command;
    uint8_t         barOffset;
    bool            clean64;

    command = disableAccess(device, true);

    if (kIOPCIRangeExpansionROM == lastBarNum) do
    {
        lastBarNum--;

        barOffset = kIOPCIConfigExpansionROMBase;
        barMask = 0x7FF;

        saved = configRead32(device, barOffset);
        configWrite32(device, barOffset, 0xFFFFFFFF & ~barMask);
        value = configRead32(device, barOffset);
        configWrite32(device, barOffset, saved);

        // unimplemented BARs are hardwired to zero
        if (value == 0)
            continue;

        range = device->ranges[kIOPCIRangeExpansionROM];
        start = reset ? 0 : (saved & ~barMask);
        value &= ~barMask;
        size  = -value;
        IOPCIRangeInit(range, kIOPCIResourceTypeMemory, start, size, size);
    }
    while (false);

    for (barNum = 0; barNum <= lastBarNum; barNum = nextBarNum)
    {
        barOffset = kIOPCIConfigBaseAddress0 + barNum * 4;
        nextBarNum = barNum + 1;

        saved = configRead32(device, barOffset);
        configWrite32(device, barOffset, 0xFFFFFFFF);
        value = configRead32(device, barOffset);
        configWrite32(device, barOffset, saved);

        // unimplemented BARs are hardwired to zero
        if (value == 0)
            continue;

        clean64 = false;
        if (value & 1)
        {
            barMask = 0x3;
            type = kIOPCIResourceTypeIO;

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
                type = kIOPCIResourceTypePrefetchMemory;
            else
                type = kIOPCIResourceTypeMemory;

            switch (value & 6)
            {
                case 2: /* below 1Mb */
                case 0: /* 32-bit mem */
                case 6: /* reserved  */
                    break;

                case 4: /* 64-bit mem */
                    clean64 = (kIOPCIResourceTypePrefetchMemory == type);
                    if (clean64)
                    {
                        upper = configRead32(device, barOffset + 4);
                        saved |= (upper << 32);
                    }
                    nextBarNum = barNum + 2;
                    break;
            }
        }

        start = reset ? 0 : (saved & ~barMask);
        value &= ~barMask;
        size  = -value;
        if (start == value) 
        {
            DLOG("  [0x%x] can't probe\n", barOffset);
            continue;
        }

        range = device->ranges[barNum];
#if 0
		if ((0x8760105a == configRead32(device->space, kIOPCIConfigVendorID))
		 && (0x20 == barOffset))
		{
			IOPCIRangeInit(range, type, start, 0xc000, size);
		}
		else
#endif

        IOPCIRangeInit(range, type, start, size, size);
        range->minAddress = minBARAddressDefault[type];
        if (clean64)
            range->maxAddress = 0xFFFFFFFFFFFFFFFFULL;
    }

    restoreAccess(device, command);
}

//---------------------------------------------------------------------------

void CLASS::deviceProbeRanges( IOPCIConfigEntry * device, uint8_t reset )
{
    uint32_t     idx;
    IOPCIRange * range;

    // Probe BAR 0 through 5 and ROM
    safeProbeBaseAddressRegister(device, kIOPCIRangeExpansionROM, reset);

    for (idx = kIOPCIRangeBAR0; idx <= kIOPCIRangeExpansionROM; idx++)
    {
        range = device->ranges[idx];
        if (!range)
            continue;
        DLOG("  [0x%x %s] 0x%llx:0x%llx\n",
             (idx == kIOPCIRangeExpansionROM) ? 
                kIOPCIConfigExpansionROMBase : idx * 4 + kIOPCIConfigBaseAddress0,
             gPCIResourceTypeName[range->type], range->start, range->proposedSize);
    }
}

//---------------------------------------------------------------------------

void CLASS::bridgeProbeBusRange(IOPCIConfigEntry * bridge, uint8_t reset)
{
    IOPCIRange *    range;
    uint64_t        start, size;

    // Record the bridge secondary and subordinate bus numbers
	if (!reset)
	{
		bridge->secBusNum = configRead8(bridge, kPCI2PCISecondaryBus);
		bridge->subBusNum = configRead8(bridge, kPCI2PCISubordinateBus);
	}
    range = IOPCIRangeAlloc();
    start = bridge->secBusNum;
    size  = bridge->subBusNum - bridge->secBusNum + 1;
	if (reset) start = 0;
    IOPCIRangeInit(range, kIOPCIResourceTypeBusNumber, start, size, kPCIBridgeBusNumberAlignment);
    bridge->ranges[kIOPCIRangeBridgeBusNumber] = range;
}

//---------------------------------------------------------------------------

void CLASS::bridgeProbeRanges( IOPCIConfigEntry * bridge, uint8_t reset )
{
    IOPCIRange *    range;
    IOPCIScalar     start, end, upper, size;
    uint32_t        bar0, bar1;

    bridgeProbeBusRange(bridge, reset);

    // Probe bridge BAR0 and BAR1 (is it ever implemented?)

    bar0 = configRead32(bridge, kIOPCIConfigBaseAddress0);
    bar1 = configRead32(bridge, kIOPCIConfigBaseAddress1);
    if (bar0 || bar1)
    {
        safeProbeBaseAddressRegister(bridge, kIOPCIRangeBAR1, reset);
        DLOG_RANGE("  bridge BAR0", bridge->ranges[kIOPCIRangeBAR0]);
        DLOG_RANGE("  bridge BAR1", bridge->ranges[kIOPCIRangeBAR1]);
    }

    // Probe memory base and limit

    end = configRead32(bridge, kPCI2PCIMemoryRange);

    start = (end & 0xfff0) << 16;
    end  |= 0x000fffff;
    if (start && (end > start))
        size = end - start + 1;
    else
        size = start = 0;
	if (reset) start = 0;

    range = IOPCIRangeAlloc();
    IOPCIRangeInit(range, kIOPCIResourceTypeMemory, start, size,
                    kPCIBridgeMemoryAlignment);
    bridge->ranges[kIOPCIRangeBridgeMemory] = range;

    // Probe prefetchable memory base and limit

    end = configRead32(bridge, kPCI2PCIPrefetchMemoryRange);

    if (true /* should check r/w */)
    {
        bridge->clean64 = (0x1 == (end & 0xf));
        if (bridge->clean64)
        {
            upper  = configRead32(bridge, kPCI2PCIPrefetchUpperBase);
            start |= (upper << 32);
            upper  = configRead32(bridge, kPCI2PCIPrefetchUpperLimit);
            end   |= (upper << 32);
        }

        start = (end & 0xfff0) << 16;
        end  |= 0x000fffff;
        if (start && (end > start))
            size = end - start + 1;
        else
            size = start = 0;
		if (reset) start = 0;

        range = IOPCIRangeAlloc();
        IOPCIRangeInit(range, kIOPCIResourceTypePrefetchMemory, start, size,
                        kPCIBridgeMemoryAlignment);
        if (bridge->clean64)
            range->maxAddress = 0xFFFFFFFFFFFFFFFFULL;
        bridge->ranges[BRN(kIOPCIResourceTypePrefetchMemory)] = range;
    }

    // Probe I/O base and limit

    end = configRead32(bridge, kPCI2PCIIORange);

    if ((end & (0x0e0e)) == 0)
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

        if (start && (end > start))
            size = end - start + 1;
        else
            size = start = 0;
		if (reset) start = 0;

        range = IOPCIRangeAlloc();
        IOPCIRangeInit(range, kIOPCIResourceTypeIO, start, size,
                        kPCIBridgeIOAlignment);
        bridge->ranges[kIOPCIRangeBridgeIO] = range;
    }

    DLOG_RANGE("  BUS", bridge->ranges[kIOPCIRangeBridgeBusNumber]);
    DLOG_RANGE("  I/O", bridge->ranges[kIOPCIRangeBridgeIO]);
    DLOG_RANGE("  MEM", bridge->ranges[kIOPCIRangeBridgeMemory]);
    DLOG_RANGE("  PFM", bridge->ranges[kIOPCIRangeBridgePFMemory]);
}

//---------------------------------------------------------------------------

void CLASS::cardbusProbeRanges(IOPCIConfigEntry * bridge, uint8_t reset)
{
    IOPCIRange * range;

    bridgeProbeBusRange(bridge, reset);

    // Maximal bus range.

    range = bridge->ranges[kIOPCIRangeBridgeBusNumber];
    range->flags    |= kIOPCIRangeFlagNoCollapse;

    // 4K register space

    range = IOPCIRangeAlloc();
    IOPCIRangeInit(range, kIOPCIResourceTypeMemory, 0, 4096, 4096);
    bridge->ranges[kIOPCIRangeBAR0] = range;

    // Maximal memory and I/O range.

    range = IOPCIRangeAlloc();
    IOPCIRangeInit(range, kIOPCIResourceTypeIO, 0, kPCIBridgeIOAlignment, kPCIBridgeIOAlignment);
    range->flags     = kIOPCIRangeFlagNoCollapse;
    bridge->ranges[kIOPCIRangeBridgeIO] = range;

    range = IOPCIRangeAlloc();
    IOPCIRangeInit(range, kIOPCIResourceTypeMemory, 0, kPCIBridgeMemoryAlignment, kPCIBridgeMemoryAlignment);
    range->flags     = kIOPCIRangeFlagNoCollapse;
    bridge->ranges[kIOPCIRangeBridgeMemory] = range;
}

//---------------------------------------------------------------------------

bool CLASS::scanProc(void * ref, IOPCIConfigEntry * bridge)
{
    bool ok = true;
    IOPCIConfigEntry * child;

    if (kPCIDeviceStateDead & bridge->deviceState)
        return (ok);

    if (!(kPCIDeviceStateScanned & bridge->deviceState))
    {
		if (bridge->ranges[kIOPCIRangeBridgeBusNumber]->size)
		{
			DLOG("scan (bus %u)\n", bridge->secBusNum);
			bridgeScanBus(bridge, bridge->secBusNum);
		}
        bridge->deviceState |= kPCIDeviceStateScanned;
        
        // Associate bootrom devices.
        bridgeConnectDeviceTree(bridge);
        
        for (child = bridge->child; child; child = child->peer)
        {
            child->deviceState &= ~kPCIDeviceStateScanned;
            child->deviceState &= ~kPCIDeviceStateAllocatedBus;
            child->deviceState &= ~kPCIDeviceStateAllocated;
        }          
    }
//    if (!(kPCIDeviceStateAllocatedBus & bridge->deviceState)
    {
        ok = bridgeTotalResources(bridge, (1 << kIOPCIResourceTypeBusNumber));
		if (ok)
		{
			ok = bridgeAllocateResources(bridge, (1 << kIOPCIResourceTypeBusNumber));
			DLOG("bus alloc done (bus %u, state 0x%x, %s)\n", 
					bridge->secBusNum, bridge->iterator, ok ? "ok" : "moretodo");
		}
        if (ok)
            bridge->deviceState |= kPCIDeviceStateAllocatedBus;
    }
    return (ok);
}

bool CLASS::totalProc(void * ref, IOPCIConfigEntry * bridge)
{
    bool ok = true;

    bridgeTotalResources(bridge, 
                          (1 << kIOPCIResourceTypeMemory)
                        | (1 << kIOPCIResourceTypePrefetchMemory)
                        | (1 << kIOPCIResourceTypeIO));
    return (ok);
}


bool CLASS::allocateProc(void * ref, IOPCIConfigEntry * bridge)
{
    bool ok = true;

//    if (!(kPCIDeviceStateAllocated & bridge->deviceState)
    {
        ok = bridgeAllocateResources(bridge, 
                          (1 << kIOPCIResourceTypeMemory)
                        | (1 << kIOPCIResourceTypePrefetchMemory)
                        | (1 << kIOPCIResourceTypeIO));
        DLOG("alloc done (bus %u, state 0x%x, %s)\n", 
                bridge->secBusNum, bridge->iterator, ok ? "ok" : "moretodo");
        if (ok)
            bridge->deviceState |= kPCIDeviceStateAllocated;
    }
    return (ok);
}

void CLASS::doConfigure(uint32_t options)
{
	fMaxPayload = 5;

    iterate(0, &CLASS::scanProc,             &CLASS::totalProc);
    iterate(1, &CLASS::allocateProc,         NULL);
    iterate(2, &CLASS::bridgeFinalizeConfig, NULL);
}

//---------------------------------------------------------------------------

enum 
{
    kIteratorNew       = 0,
    kIteratorCheck     = 1,
    kIteratorDidCheck  = 2,
    kIteratorDoneCheck = 3,
};

void CLASS::iterate(uint32_t options, IterateProc topProc, IterateProc bottomProc, void * ref)
{
    IOPCIConfigEntry * device;
    IOPCIConfigEntry * parent;
    bool      ok, didCheck;

    device = fRoot;
    device->iterator = kIteratorCheck;

    DLOG("iterate start(%d)\n", options);
    do
    {
        parent = device->parent;
//        DLOG("iterate(bus %u, state 0x%x, parent 0x%x)\n", device->secBusNum, device->iterator, parent ? parent->iterator : 0);
        ok = true;
        didCheck = false;

        if (device->iterator == kIteratorCheck)
        {
            didCheck = true;
            if (topProc)
            {
                ok = (this->*topProc)(ref, device);
            }
            device->iterator = kIteratorDidCheck;
        }

        if (parent && !ok)
        {
            parent->iterator = kIteratorCheck;
            device = parent;
        }
        else
        {
            IOPCIConfigEntry * child;
            IOPCIConfigEntry * next = NULL;
            for (child = device->child; child; child = child->peer)
            {
                if (!child->isBridge)
                    continue;
                if (didCheck)
                    child->iterator = kIteratorCheck;
                if (!next && (child->iterator < kIteratorDidCheck))
                    next = child;
            }
            if (next)
            {
                device = next;
            }
            else
            {
                device->iterator = kIteratorDoneCheck;
//                DLOG("bus done (bus %u, state 0x%x)\n", device->secBusNum, device->iterator);
                if (bottomProc)
                {
                    (void) (this->*bottomProc)(ref, device);
                }
                device = parent;
            }
        }
    }
    while (device);
    DLOG("iterate done(%d)\n", options);
}

//---------------------------------------------------------------------------

void CLASS::configure(uint32_t options)
{
    bool bootConfig = (kIOPCIConfiguratorBoot & options);

    IOLog("[ PCI configuration begin ]\n");

    PE_Video	       consoleInfo;

	fFlags |= options;

    if (bootConfig)
    {
        if (!fPFMConsole)
        {
            IOService::getPlatform()->getConsoleInfo(&consoleInfo);
            fPFMConsole  = consoleInfo.v_baseAddr;
#ifndef __LP64__
            fPFMConsole |= (((uint64_t) consoleInfo.v_baseAddrHigh) << 32);
#endif
            DLOG("console %ld x %ld @ 0x%qx\n", 
                consoleInfo.v_width, consoleInfo.v_height, fPFMConsole);
        }
        fConsoleRange = NULL;
        getPlatform()->setConsoleInfo(0, kPEDisableScreen);
    }

    doConfigure(options);
    if (bootConfig)
    {
        if (fConsoleRange)
        {
            if (fConsoleRange->start)
                fPFMConsole += fConsoleRange->start;
            else
                fPFMConsole = 0;
        }
        if (!fPFMConsole)
        {
            DLOG("!! lost console !!\n");
        }
        else
        {
            if (fConsoleRange)
            {
                consoleInfo.v_baseAddr = (fPFMConsole | 1);
#ifdef __LP64__
                DLOG("console setting @ %lx\n", 
                    consoleInfo.v_baseAddr);
#else
                consoleInfo.v_baseAddrHigh = (fPFMConsole >> 32);
                DLOG("console setting 0x%lx:%lx\n", 
                    consoleInfo.v_baseAddrHigh, consoleInfo.v_baseAddr);
#endif
                getPlatform()->setConsoleInfo(&consoleInfo, kPEBaseAddressChange);
            }
            getPlatform()->setConsoleInfo(NULL, kPEEnableScreen);
            if (fConsoleRange)
                IOLog("console relocated to 0x%llx\n", fPFMConsole);
        }
    }

	fFlags &= ~options;

    if (fBridgeConfigCount || fDeviceConfigCount || fCardBusConfigCount)
    {
        IOLog("PCI configuration changed (bridge=%d device=%d cardbus=%d)\n",
              fBridgeConfigCount, fDeviceConfigCount, fCardBusConfigCount);
    }
    IOLog("[ PCI configuration end, bridges %d devices %d ]\n", fBridgeCount, fDeviceCount);
}

//---------------------------------------------------------------------------

IOPCIRange * CLASS::bridgeGetRange(IOPCIConfigEntry * bridge, uint32_t type)
{
    IOPCIRange * range;

    if (bridge->isHostBridge && (type != kIOPCIResourceTypeBusNumber))
    {
        // use global host ranges
        bridge = fRoot;
    }
    switch (type)
    {
        case kIOPCIResourceTypePrefetchMemory:
            range = bridge->ranges[BRN(kIOPCIResourceTypePrefetchMemory)];
            if (range)
                break;
            /* fall thru */

        case kIOPCIResourceTypeMemory:
            range = bridge->ranges[BRN(kIOPCIResourceTypeMemory)];
            break;

        case kIOPCIResourceTypeIO:
        case kIOPCIResourceTypeBusNumber:
            range = bridge->ranges[BRN(type)];
            break;

        default:
            range = NULL;
            break;
    }
    return (range);
}

//---------------------------------------------------------------------------

bool CLASS::bridgeTotalResources(IOPCIConfigEntry * bridge, uint32_t typeMask)
{
	IOPCIConfigEntry * child;
	IOPCIRange *       range;
    IOPCIRange *       childRange;
    uint32_t           type;
	bool		       ok = true;

    IOPCIScalar		   totalSize[kIOPCIResourceTypeCount];
    IOPCIScalar        maxAlignment[kIOPCIResourceTypeCount];
    IOPCIScalar        minAddress[kIOPCIResourceTypeCount];
    IOPCIScalar        maxAddress[kIOPCIResourceTypeCount];

    if (bridge->isHostBridge)
        return (ok);
    if (kPCIDeviceStateDead & bridge->deviceState)
        return (ok);

    DLOG("bridgeTotalResources(bus %u, iter 0x%x, state 0x%x)\n", 
            bridge->secBusNum, bridge->iterator, bridge->deviceState);

    bzero(&totalSize[0], sizeof(totalSize));
    if (bridge != fRoot)
        totalSize[kIOPCIResourceTypeBusNumber] = 1;

    bcopy(&minBridgeAlignments[0], &maxAlignment[0], sizeof(maxAlignment));

    bzero(&minAddress[0], sizeof(minAddress));
    bcopy(&maxBridgeAddressDefault[0], &maxAddress[0], sizeof(maxAddress));
    if (bridge->clean64)
        maxAddress[kIOPCIResourceTypePrefetchMemory] = 0xFFFFFFFFFFFFFFFFULL;

	if ( ((1 << kIOPCIResourceTypeMemory)
		| (1 << kIOPCIResourceTypePrefetchMemory)
		| (1 << kIOPCIResourceTypeIO))
		& typeMask)
	do
	{
		if (!bridge->child)
			break;
		for (child = bridge->child; child; child = child->peer)
		{
			if (!(kPCIDeviceStateDead & child->deviceState))
				break;
		}
		if (child)
			break;
		// all children are dead, move reserved allocs to parent & free bridge ranges
		IOPCIConfigEntry * pendingList = NULL;
		IOPCIConfigEntry * next;
		for (child = bridge->child; child; child = next)
		{
			next = child->peer;
			bridgeRemoveChild(bridge, child, kIOPCIRangeAllBarsMask, 0, &pendingList);
		}
		bridgeDeallocateChildRanges(bridge->parent, bridge, 
								kIOPCIRangeAllBridgeMask & ~(1 << kIOPCIRangeBridgeBusNumber), 0);
		if (!pendingList) panic("!pendingList");
		bridgeMoveChildren(bridge->parent, pendingList);
	}
	while (false);

	for (child = bridge->child; child; child = child->peer)
    {
        for (int i = 0; i < kIOPCIRangeCount; i++)
        {
            childRange = child->ranges[i];
            if (!childRange)
                continue;
            if (!((1 << childRange->type) & typeMask))
                continue;
            if (!childRange->proposedSize)
                continue;
            range = bridgeGetRange(bridge, childRange->type);
            if (kIOPCIRangeExpansionROM == i)
            { /* Don't allocate for a ROM */ }
            else
            {
                totalSize[range->type] += childRange->proposedSize;
            }
            if (childRange->alignment > maxAlignment[range->type])
                maxAlignment[range->type] = childRange->alignment;
            if (childRange->minAddress < minAddress[range->type])
                minAddress[range->type] = childRange->minAddress;
            if (childRange->maxAddress < maxAddress[range->type])
            {
                DLOG("  %s: maxAddr change 0x%llx -> 0x%llx (at %u:%u:%u)\n",
                        gPCIResourceTypeName[range->type], 
                        maxAddress[range->type], childRange->maxAddress, PCI_ADDRESS_TUPLE(child));
                maxAddress[range->type] = childRange->maxAddress;
			}

			if (childRange->proposedSize
			  && child->expressCapBlock
			  && (kIOPCIResourceTypeMemory == childRange->type)
			    || (kIOPCIResourceTypePrefetchMemory == childRange->type))
			{
				if (child->expressMaxPayload < fMaxPayload)
				{
					fMaxPayload  = child->expressMaxPayload;
				}
			}
        }
    }

    for (type = 0; type < kIOPCIResourceTypeCount; type++)
    {
        if (!((1 << type) & typeMask))
            continue;

        totalSize[type] = IOPCIScalarAlign(totalSize[type], minBridgeAlignments[type]);
        range = bridgeGetRange(bridge, type);
        if (range)
        {
            if ((totalSize[type] != range->proposedSize)
             && ((!totalSize[type] && !(kIOPCIRangeFlagMaximizeSize & range->flags))
             	|| (totalSize[type] > range->proposedSize)))
            {
                DLOG("  %s: 0x%llx: size change 0x%llx -> 0x%llx\n",
                        gPCIResourceTypeName[type], 
                        range->start, range->proposedSize, totalSize[type]);
                range->proposedSize          = totalSize[type];
                bridge->rangeSizeChanges    |= (1 << BRN(type));
                bridge->rangeRequestChanges |= (1 << type);
				ok = false;
            }
            range->alignment  = maxAlignment[type];
            range->minAddress = minAddress[type];
            range->maxAddress = maxAddress[type];
        }

        DLOG("  %s: total child reqs 0x%llx of 0x%llx maxalign 0x%llx\n",
                gPCIResourceTypeName[type], 
                totalSize[type], range->proposedSize, range->alignment);
    }

	return (ok);
}

bool CLASS::bridgeAllocateResources(IOPCIConfigEntry * bridge, uint32_t typeMask)
{
    IOPCIRange * requests[kIOPCIResourceTypeCount];
    IOPCIScalar  shortage[kIOPCIResourceTypeCount];
	IOPCIScalar  shortageAlignments[kIOPCIResourceTypeCount];
    IOPCIRange * range;
    IOPCIRange * childRange;
    IOPCIScalar  shrink;
    uint32_t     type;
    int          phase;
    bool         ok;
	bool         canRelocate;
    bool         moretodo = false;

    DLOG("bridgeAllocateResources(bus %u, iter 0x%x, state 0x%x)\n", 
            bridge->secBusNum, bridge->iterator, bridge->deviceState);

    if (bridge == fRoot)
        return (true);
    if (kPCIDeviceStateDead & bridge->deviceState)
        return (true);

    bzero(requests, sizeof(requests));
    bzero(shortage, sizeof(shortage));
    bzero(shortageAlignments, sizeof(shortageAlignments));

	if ((1 << kIOPCIResourceTypeBusNumber) & typeMask)
	{
		if (!bridge->busResv.nextSubRange
			&& (range = bridgeGetRange(bridge, kIOPCIResourceTypeBusNumber)))
		{
			IOPCIRangeInit(&bridge->busResv, kIOPCIResourceTypeBusNumber,
							bridge->secBusNum, 1, kPCIBridgeBusNumberAlignment);
			ok = IOPCIRangeListAllocateSubRange(range, &bridge->busResv); 
			DLOG("  BUS: reserved(%sok) 0x%llx\n", ok ? "" : "!", bridge->busResv.start);
		}

		FOREACH_CHILD(bridge, child)
		{
			range = bridgeGetRange(child, kIOPCIResourceTypeBusNumber);
			if (!range)
				continue;
			if ((!child->ranges[kIOPCIRangeBridgeMemory]
					|| !child->ranges[kIOPCIRangeBridgeMemory]->nextSubRange)
				&& (!child->ranges[kIOPCIRangeBridgePFMemory]
					|| !child->ranges[kIOPCIRangeBridgePFMemory]->nextSubRange)
				&& (!child->ranges[kIOPCIRangeBridgeIO]
					|| !child->ranges[kIOPCIRangeBridgeIO]->nextSubRange))
			{
				range->flags |= kIOPCIRangeFlagRelocatable;
			}
			else
			{
				range->flags &= ~kIOPCIRangeFlagRelocatable;
			}
		}
	}

    // Try placed allocations - free relocations

    for (phase = 0; phase <= 2; phase++)
    {
        FOREACH_CHILD(bridge, child)
        {
            for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
            {
                int resize;
    
                childRange = child->ranges[rangeIndex];
                if (!childRange)
                    continue;
                if (!((1 << childRange->type) & typeMask))
                    continue;
   
                resize = (childRange->proposedSize && 
                		 (childRange->proposedSize != childRange->size));

				if ((((kIOPCIRangeFlagMaximizeSize | kIOPCIRangeFlagSplay) & childRange->flags)
						? 2 : resize) != phase)
                    continue;

                DLOG("  %s: 0x%llx new 0x%llx current 0x%llx (at %u:%u:%u) [p%d] %s%s%s\n",
                        gPCIResourceTypeName[childRange->type], 
                        childRange->start, childRange->proposedSize, childRange->size,
                        PCI_ADDRESS_TUPLE(child),
                        phase,
                        (kIOPCIRangeFlagRelocatable  & childRange->flags) ? "R" : "",
                        (kIOPCIRangeFlagSplay        & childRange->flags) ? "S" : "",
                        (kIOPCIRangeFlagMaximizeSize & childRange->flags) ? "M" : "");

                range = bridgeGetRange(bridge, childRange->type);
                if (!range)
                    continue;
				type = range->type;

                if (bridge->isHostBridge 
                    && !childRange->nextSubRange
                	&& (kIOPCIConfiguratorPFM64 & fFlags) 
                	&& (kIOPCIResourceTypeMemory == type)
            		&& (childRange->maxAddress > 0xFFFFFFFFULL))
				{
//					childRange->minAddress = (1ULL << 32);
					if (!fConsoleRange && fPFMConsole 
						&& (fPFMConsole >= childRange->start) 
						&& (fPFMConsole < childRange->end))
					{
						DLOG("  hit console\n");
						fPFMConsole -= childRange->start;
						fConsoleRange = childRange;
					}
					childRange->start = 0;
				}

                if (!childRange->proposedSize)
                {
                    if (childRange->nextSubRange)
                    {
                        child->rangeBaseChanges |= (1 << rangeIndex);
						ok = IOPCIRangeListDeallocateSubRange(range, childRange);
						if (!ok) panic("IOPCIRangeListDeallocateSubRange");
					}
                    childRange->start = 0;
                    childRange->size  = 0;
                    continue;
                }
				if (!childRange->start)
					continue;
				if (childRange->nextSubRange && !resize)
					continue;

				// try the placed alloc
				ok = IOPCIRangeListAllocateSubRange(range, childRange);
				if (ok)
					continue;

				canRelocate = (!((kIOPCIRangeFlagMaximizeSize | kIOPCIRangeFlagSplay) & childRange->flags)
							 && (kIOPCIConfiguratorBoot & fFlags));
				canRelocate |= (0 != (kIOPCIRangeFlagRelocatable & childRange->flags));
				canRelocate |= (!childRange->nextSubRange);

				enum { kBridgeMemIO = (1 << kIOPCIRangeBridgeMemory)
									| (1 << kIOPCIRangeBridgePFMemory)
									| (1 << kIOPCIRangeBridgeIO) };

				if (false && resize && ((1 << rangeIndex) & kBridgeMemIO))
				{
					// try to extend bridge ranges back
					if (childRange->proposedSize < childRange->size) panic("<proposedSize");
					ok = IOPCIRangeListAllocateSubRange(range, childRange, 
							childRange->start - (childRange->proposedSize - childRange->size));
					if (ok)
					{
						child->rangeBaseChanges |= (1 << rangeIndex);
						continue;
					}
				}

				DLOG("  %s: 0x%llx:0x%llx inplace alloc failed\n",
							gPCIResourceTypeName[type], 
							childRange->start, childRange->proposedSize);

				if (!canRelocate)
				{
					if (childRange->nextSubRange)
					{
						DLOG("  %s: resize shortage 0x%llx -> 0x%llx\n",
							   gPCIResourceTypeName[type], childRange->size, childRange->proposedSize);
						shortage[type] += childRange->proposedSize - childRange->size;
						if (childRange->alignment > shortageAlignments[type])
							shortageAlignments[type] = childRange->alignment;
					}
				}
				else
				{
					if (childRange->nextSubRange)
					{
						DLOG("  %s: 0x%llx:0x%llx freed\n",
									gPCIResourceTypeName[type], 
									childRange->start, childRange->size);
						ok = IOPCIRangeListDeallocateSubRange(range, childRange);
						if (!ok) panic("IOPCIRangeListDeallocateSubRange");
					}
					childRange->start = 0;
					childRange->size  = 0;
				}
            }
        }
    }

    // Find allocations to make

	FOREACH_CHILD(bridge, child)
	{
		for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
		{
			childRange = child->ranges[rangeIndex];
			if (!childRange)
				continue;
			if (!((1 << childRange->type) & typeMask))
				continue;

			if (childRange->proposedSize && !childRange->nextSubRange)
			{
                range = bridgeGetRange(bridge, childRange->type);
                if (!range)
                    continue;
				type = range->type;

				// Link child ranges to form dependency chain, and sort this
				// list based on descending alignment values. This will make
				// it easier to satisfy alignment requirements for all child
				// ranges by allocating the largest sub-range first.

				// Don't allocate for a ROM
				if (kIOPCIRangeExpansionROM == rangeIndex)
					continue;
	
				child->rangeBaseChanges |= (1 << rangeIndex);
				IOPCIRangeAppendSubRange(&requests[type], childRange);
			}
		}
	}

    // Make unplaced allocations

    for (type = 0; type < kIOPCIResourceTypeCount; type++)
    {
        if (!((1 << type) & typeMask))
            continue;
        range = bridgeGetRange(bridge, type);
        while ((childRange = requests[type]))
        {
            requests[type] = childRange->nextSubRange;
			childRange->nextSubRange = NULL;

            ok = IOPCIRangeListAllocateSubRange(range, childRange);
            DLOG("  %s: %s allocated block 0x%llx:0x%llx\n",
                 gPCIResourceTypeName[childRange->type],
                 ok ? "ok" : "NOT",
                 childRange->start, childRange->proposedSize);

            if (!ok)
            {
                shortage[type] += childRange->proposedSize - childRange->size;
				if (childRange->alignment > shortageAlignments[type])
					shortageAlignments[type] = childRange->alignment;
                if (bridge->isHostBridge)
                {
                    DLOG("  %s: new host req 0x%llx -> 0x%llx\n",
                           gPCIResourceTypeName[type], childRange->size, childRange->proposedSize);
                    IOPCIRangeDump(range);
                }
            }
        }
        if (!bridge->isHostBridge)
        {
            if (shortage[type])
            {
            	IOPCIScalar newSize, extendAvail;
				extendAvail = IOPCIRangeListLastFree(range, shortageAlignments[type]);
				if (shortage[type] <= extendAvail)
					newSize = range->proposedSize;
				else
					newSize = range->size + shortage[type] - extendAvail;
				if (newSize > range->proposedSize)
				{
					range->proposedSize = IOPCIScalarAlign(newSize, minBridgeAlignments[type]);;
					DLOG("  %s: shortage 0x%llx -> 0x%llx\n",
						   gPCIResourceTypeName[type], range->size, range->proposedSize);
					bridge->rangeSizeChanges |= (1 << BRN(type));
					bridge->rangeRequestChanges |= (1 << type);
				}
                moretodo = true;
            }
            else if (range && !(kIOPCIRangeFlagNoCollapse & range->flags))
            {
                shrink = IOPCIRangeListCollapse(range, minBridgeAlignments[type]);
                if (shrink)
                {
                    DLOG("  %s: shrunk 0x%llx -> 0x%llx\n",
                           gPCIResourceTypeName[type], range->size, range->proposedSize);
                    bridge->rangeSizeChanges    |= (1 << BRN(type));
                    bridge->rangeRequestChanges |= (1 << type);
                    moretodo = true;
                }
            }
        }
    }

    if (moretodo && !(bridge->rangeRequestChanges & typeMask))
    {
        DLOG("  exhausted\n");
        moretodo = false;
    }

    if (moretodo)
    {
        bridge->rangeRequestChanges &= ~typeMask;
    }
    else
    {
        // Apply configuration changes to all children.
        FOREACH_CHILD(bridge, child)
        {
            applyConfiguration(child, typeMask);
        }
    }

    return (!moretodo);
}

//---------------------------------------------------------------------------

uint16_t CLASS::disableAccess(IOPCIConfigEntry * device, bool disable)
{
    uint16_t  command;

    command = configRead16(device, kIOPCIConfigCommand);
    if (disable)
    {
        configWrite16(device, kIOPCIConfigCommand,
                        (command & ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace)));
    }
    return (command);
}

void CLASS::restoreAccess( IOPCIConfigEntry * device, UInt16 command )
{
    configWrite16(device, kIOPCIConfigCommand, command);
}

//---------------------------------------------------------------------------

void CLASS::applyConfiguration(IOPCIConfigEntry * device, uint32_t typeMask)
{
    if ((!device->isHostBridge) && !(kPCIDeviceStateDead & device->deviceState))
    {
        if (device->rangeBaseChanges || device->rangeSizeChanges) 
        {
            switch (device->headerType)
            {
                case kPCIHeaderType0:
                    deviceApplyConfiguration(device, typeMask);
                    break;
                case kPCIHeaderType1:
                case kPCIHeaderType2:
                    bridgeApplyConfiguration(device, typeMask);
                    break;
            }
            device->deviceState &= ~kPCIDeviceStatePropertiesDone;
        }
		if (!(kPCIDeviceStatePropertiesDone & device->deviceState))
			writeLatencyTimer(device);
    }

    device->deviceState |= kPCIDeviceStateConfigurationDone;
}

void CLASS::deviceApplyConfiguration(IOPCIConfigEntry * device, uint32_t typeMask)
{
    IOPCIScalar start;
    IOPCIRange * range;
    uint16_t     reg16;

    DLOG("Applying config (bm 0x%x, sm 0x%x) for device %u:%u:%u\n", 
            device->rangeBaseChanges, device->rangeSizeChanges,
            PCI_ADDRESS_TUPLE(device));
    fDeviceConfigCount++;

    reg16 = disableAccess(device, true);

    for (int rangeIndex = kIOPCIRangeBAR0; rangeIndex <= kIOPCIRangeExpansionROM; rangeIndex++)
    {
        uint32_t bar;
        bool     change;
        range = device->ranges[rangeIndex];
        if (!range)
            continue;
        if (!((1 << range->type) & typeMask))
            continue;
        change = (0 != ((1 << rangeIndex) & device->rangeBaseChanges));
        device->rangeBaseChanges &= ~(1 << rangeIndex);
        device->rangeSizeChanges &= ~(1 << rangeIndex);
        if (change)
        {
            start = range->start;
            if (!start)
                continue;
            if (rangeIndex <= kIOPCIRangeBAR5)
                bar = kIOPCIConfigBaseAddress0 + (rangeIndex * 4);
            else
                bar = kIOPCIConfigExpansionROMBase;
            configWrite32(device, bar, start);
            DLOG("  [0x%x %s] 0x%llx, read 0x%x\n", 
                bar, gPCIResourceTypeName[range->type],
                start & 0xFFFFFFFF, configRead32(device, bar));
            if (kIOPCIConfigExpansionROMBase != bar)
            {
                start >>= 32;
                if (start)
                {
                    rangeIndex++;
                    bar += 4;
                    configWrite32(device, bar, start);
                    DLOG("  [0x%x %s] 0x%llx, read 0x%x\n", 
                        bar, gPCIResourceTypeName[range->type],
                        start, configRead32(device, bar));
                }
            }
        }
    }

//    reg16 &= ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace |
//               kIOPCICommandBusMaster | kIOPCICommandMemWrInvalidate);
    restoreAccess(device, reg16);

    DLOG("  Device Command = 0x%08x\n", (uint32_t) 
         configRead32(device, kIOPCIConfigCommand));
}

void CLASS::bridgeApplyConfiguration(IOPCIConfigEntry * bridge, uint32_t typeMask)
{
    IOPCIScalar  start;
    IOPCIScalar  end;
    IOPCIRange * range;
    uint16_t     commandReg = 0;
    uint32_t     baselim32;
    uint16_t     baselim16;
    bool         accessDisabled;

    enum { 
        kBridgeCommand = (kIOPCICommandIOSpace | kIOPCICommandMemorySpace | kIOPCICommandBusMaster) 
    };

    do
    {
		if (bridge->ranges[kIOPCIRangeBridgeBusNumber]->start
			&& bridge->ranges[kIOPCIRangeBridgeBusNumber]->size)
		{
			bridge->secBusNum = bridge->ranges[kIOPCIRangeBridgeBusNumber]->start;
			bridge->subBusNum = bridge->secBusNum
							  + bridge->ranges[kIOPCIRangeBridgeBusNumber]->size - 1;
		}
		else
		{
			bridge->secBusNum = bridge->subBusNum = 0;
		}

        accessDisabled = (0 != bridge->rangeBaseChanges);
accessDisabled = false;
        DLOG("Applying config for bridge serving bus %u (disabled %d) %s\n",
                bridge->secBusNum, accessDisabled,
                (!bridge->secBusNum) ? "(dead)" : "");

        commandReg = disableAccess(bridge, accessDisabled);

        // Program bridge BAR0 and BAR1

        for (int rangeIndex = kIOPCIRangeBAR0; rangeIndex <= kIOPCIRangeBAR1; rangeIndex++)
        {
            int thisIndex = rangeIndex;
            range = bridge->ranges[rangeIndex];
            if (!range)
                continue;
            if (!((1 << range->type) & typeMask))
                continue;
            if ((1 << rangeIndex) & bridge->rangeBaseChanges)
            {
                start = range->start;
                configWrite32(bridge, kIOPCIConfigBaseAddress0 + rangeIndex * 4, start);
                start >>= 32;
                if (start)
                {
                    rangeIndex++;
                    configWrite32(bridge, kIOPCIConfigBaseAddress0 + rangeIndex * 4, start);
                }
            }
            bridge->rangeBaseChanges &= ~(1 << thisIndex);
            bridge->rangeSizeChanges &= ~(1 << thisIndex);
        }

        if (((1 << kIOPCIResourceTypeBusNumber) & typeMask)
          && ((1 << kIOPCIRangeBridgeBusNumber) & (bridge->rangeBaseChanges | bridge->rangeSizeChanges)))
        {
            DLOG_RANGE("  BUS", bridge->ranges[kIOPCIRangeBridgeBusNumber]);

            // Give children the correct bus
    
            FOREACH_CHILD(bridge, child)
            {
                child->space.s.busNum = bridge->secBusNum;
            }
    
            // Program bridge bus numbers
    
            uint32_t reg32 = configRead32(bridge, kPCI2PCIPrimaryBus);
            reg32 &= ~0x00ffffff;
            reg32 |= bridge->space.s.busNum | (bridge->secBusNum << 8) | (bridge->subBusNum << 16);
            configWrite32(bridge, kPCI2PCIPrimaryBus, reg32);
    
            DLOG("  BUS: prim/sec/sub = 0x%02x:0x%02x:0x%02x\n",
                 configRead8(bridge, kPCI2PCIPrimaryBus),
                 configRead8(bridge, kPCI2PCISecondaryBus),
                 configRead8(bridge, kPCI2PCISubordinateBus));

            bridge->rangeBaseChanges &= ~(1 << kIOPCIRangeBridgeBusNumber);
            bridge->rangeSizeChanges &= ~(1 << kIOPCIRangeBridgeBusNumber);
        }

        // That's it for cardbus

        if (kPCIHeaderType2 == bridge->headerType)
        {
            fCardBusConfigCount++;
            break;
        }

        if (((1 << kIOPCIResourceTypeIO) & typeMask)
          && ((1 << kIOPCIRangeBridgeIO) & (bridge->rangeBaseChanges | bridge->rangeSizeChanges)))
        {
            // Program I/O base and limit

            DLOG_RANGE("  I/O", bridge->ranges[kIOPCIRangeBridgeIO]);
    
            baselim16 = 0x00f0; // closed range
            range = bridge->ranges[kIOPCIRangeBridgeIO];
            if (range && range->size)
            {
                assert(range->start);
                assert((range->size  & (4096-1)) == 0);
                assert((range->start & (4096-1)) == 0);
                assert((range->start & 0xffff0000) == 0);
    
                start = range->start;
                end = start + range->size - 1;
                baselim16 = ((start >> 8) & 0xf0) | (end & 0xf000);        
            }
            configWrite16(bridge, kPCI2PCIIORange, baselim16);
            configWrite32(bridge, kPCI2PCIUpperIORange, 0);

            DLOG("  I/O: base/limit   = 0x%04x\n",
                 configRead16(bridge, kPCI2PCIIORange));

            bridge->rangeBaseChanges &= ~(1 << kIOPCIRangeBridgeIO);
            bridge->rangeSizeChanges &= ~(1 << kIOPCIRangeBridgeIO);
        }

        if (((1 << kIOPCIResourceTypeMemory) & typeMask)
          && ((1 << kIOPCIRangeBridgeMemory) & (bridge->rangeBaseChanges | bridge->rangeSizeChanges)))
        {
            // Program memory base and limit
    
            DLOG_RANGE("  MEM", bridge->ranges[kIOPCIRangeBridgeMemory]);

            baselim32 = 0x0000FFF0; // closed range
            range = bridge->ranges[kIOPCIRangeBridgeMemory];
            if (range && range->size && !(kPCIDeviceStateNoLink & bridge->deviceState))
            {
                assert(range->start);
                assert((range->size  & (0x100000-1)) == 0);
                assert((range->start & (0x100000-1)) == 0);
    
                start = range->start;
                end   = range->start + range->size - 1;
                baselim32 = ((start >> 16) & 0xFFF0) | (end & 0xFFF00000);
            }
            configWrite32(bridge, kPCI2PCIMemoryRange, baselim32);

            DLOG("  MEM: base/limit   = 0x%08x\n", (uint32_t) 
                 configRead32(bridge, kPCI2PCIMemoryRange));

            bridge->rangeBaseChanges &= ~(1 << kIOPCIRangeBridgeMemory);
            bridge->rangeSizeChanges &= ~(1 << kIOPCIRangeBridgeMemory);
        }

        if (((1 << kIOPCIResourceTypePrefetchMemory) & typeMask)
          && ((1 << kIOPCIRangeBridgePFMemory) & (bridge->rangeBaseChanges | bridge->rangeSizeChanges)))
        {
            // Program prefetchable memory base and limit

            DLOG_RANGE("  PFM", bridge->ranges[kIOPCIRangeBridgePFMemory]);
    
            baselim32 = 0x0000FFF0; // closed range
            start     = 0;
            end       = 0;
            range = bridge->ranges[kIOPCIRangeBridgePFMemory];
            if (range && range->size && !(kPCIDeviceStateNoLink & bridge->deviceState))
            {
                assert(range->start);
                assert((range->size  & (0x100000-1)) == 0);
                assert((range->start & (0x100000-1)) == 0);
    
                start = range->start;
                end = range->start + range->size - 1;
                baselim32 = ((start >> 16) & 0xFFF0) | (end & 0xFFF00000);
            }
            configWrite32(bridge, kPCI2PCIPrefetchMemoryRange, baselim32);
            configWrite32(bridge, kPCI2PCIPrefetchUpperBase,  (start >> 32));
            configWrite32(bridge, kPCI2PCIPrefetchUpperLimit, (end   >> 32));

            DLOG("  PFM: base/limit   = 0x%08x, 0x%08x, 0x%08x\n",  
                 (uint32_t)configRead32(bridge, kPCI2PCIPrefetchMemoryRange),
                 (uint32_t)configRead32(bridge, kPCI2PCIPrefetchUpperBase),
                 (uint32_t)configRead32(bridge, kPCI2PCIPrefetchUpperLimit));

            bridge->rangeBaseChanges &= ~(1 << kIOPCIRangeBridgePFMemory);
            bridge->rangeSizeChanges &= ~(1 << kIOPCIRangeBridgePFMemory);
        }
        fBridgeConfigCount++;
    }
    while (false);

    // Set IOSE, memory enable, Bus Master transaction forwarding

    DLOG("Enabling bridge serving bus %u\n", bridge->secBusNum);

    if (kPCIHeaderType2 == bridge->headerType)
    {
        commandReg &= ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace |
                   kIOPCICommandBusMaster | kIOPCICommandMemWrInvalidate);
    }
    else
    {
        uint16_t bridgeControl;

        commandReg |= (kIOPCICommandIOSpace | kIOPCICommandMemorySpace |
                       kIOPCICommandBusMaster);

        // Turn off ISA bit.
        bridgeControl = configRead16(bridge, kPCI2PCIBridgeControl);
        if (bridgeControl & 0x0004)
        {
            bridgeControl &= ~0x0004;
            configWrite16(bridge, kPCI2PCIBridgeControl, bridgeControl);
            DLOG("  Bridge Control    = 0x%04x\n",
                 configRead16(bridge, kPCI2PCIBridgeControl));
        }
    }

    restoreAccess(bridge, commandReg);

    DLOG("  Bridge Command    = 0x%08x\n", 
         configRead32(bridge, kIOPCIConfigCommand));
}

//---------------------------------------------------------------------------

#ifndef ExtractLSB(x)
#define ExtractLSB(x) ((x) & (~((x) - 1)))
#endif

void CLASS::checkCacheLineSize(IOPCIConfigEntry * device)
{
    uint8_t cacheLineSize, cls, was;

    if (device->isHostBridge)
        return;

    if (kPCIStatic == device->parent->supportsHotPlug)
        return;

    if ((kPCIHotPlugTunnelRoot == device->parent->supportsHotPlug)
       || (kPCIHotPlugTunnel == device->parent->supportsHotPlug))
		cacheLineSize = 0x20;
	else
		cacheLineSize = 0x40;

    cls = configRead8(device, kIOPCIConfigCacheLineSize);
    was = cls;

    // config looks reasonable, keep original value
    if ((cls >= cacheLineSize) && ((cls % cacheLineSize) == 0))
        return;

    configWrite8(device, kIOPCIConfigCacheLineSize, cacheLineSize);
    cls = configRead8(device, kIOPCIConfigCacheLineSize);
    if (cls != cacheLineSize)
    {
        DLOG("  could not set CLS from %u to %u dwords\n", was, cls);
        configWrite8(device, kIOPCIConfigCacheLineSize, 0);
    }
    else
    {
        DLOG("  changed CLS from %u to %u dwords\n", was, cls);
    }
}

//---------------------------------------------------------------------------

void CLASS::writeLatencyTimer(IOPCIConfigEntry * device)
{
    const uint8_t defaultLT = 0x40;
    uint8_t was, now;

    if (device == fRoot)
        return;

    // Nothing fancy here, just set the latency timer to 64 PCI clocks.

    was = configRead8(device, kIOPCIConfigLatencyTimer);
    configWrite8(device, kIOPCIConfigLatencyTimer, defaultLT);
    now = configRead8(device, kIOPCIConfigLatencyTimer);
    if (was != now)
    {
        DLOG("  changed LT %u->%u PCI clocks\n", was, now);
    }

    // Bridges can act as an initiator on either side of the bridge,
    // and there is a separate register for the latency timer on the
    // secondary side.

    if (device->isBridge)
    {
        was = configRead8(device, kPCI2PCISecondaryLT);
        configWrite8(device, kPCI2PCISecondaryLT, defaultLT);
        now = configRead8(device, kPCI2PCISecondaryLT);
        if (was != now)
        {
            DLOG("  changed SEC-LT %u->%u PCI clocks\n", was, now);
        }
    }
}

//---------------------------------------------------------------------------

bool CLASS::bridgeFinalizeConfig(void * unused, IOPCIConfigEntry * bridge)
{
	uint32_t deviceControl, newControl;

	if (bridge->supportsHotPlug >= kPCIHotPlug)
	{
		FOREACH_CHILD(bridge, child)
		{
			if (kPCIDeviceStateDead & child->deviceState)
				continue;
			if (kPCIDeviceStatePropertiesDone & child->deviceState)
				continue;
			if (!child->expressCapBlock)
				continue;
			deviceControl = configRead16(child, child->expressCapBlock + 0x08);
			newControl    = deviceControl & ~((7 << 5) | (7 << 12));
			newControl    |= (fMaxPayload << 5) | (fMaxPayload << 12);
			if (newControl != deviceControl)
			{
				configWrite16(child, child->expressCapBlock + 0x08, deviceControl);
				DLOG("payload set 0x%08x -> 0x%08x (at %u:%u:%u), fMaxPayload 0x%x\n",
					  deviceControl, newControl,
					  PCI_ADDRESS_TUPLE(child), fMaxPayload);
			}
		}
	}

	return (bridgeConstructDeviceTree(unused, bridge));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Configuration Space Access
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool CLASS::configAccess(IOPCIConfigEntry * device, bool write)
{
	bool ok = 
	(0 == ((write ? kPCIDeviceStateConfigWProtect : kPCIDeviceStateConfigRProtect) 
			& device->deviceState));
	if (!ok)
	{
		DLOG("config protect fail(1) for device %u:%u:%u\n", PCI_ADDRESS_TUPLE(device));
		OSReportWithBacktrace("config protect fail(1) for device %u:%u:%u\n", 
								PCI_ADDRESS_TUPLE(device));
	}
	return (ok);
}

uint32_t CLASS::configRead32( IOPCIAddressSpace space, uint32_t offset )
{
	space.es.registerNumExtended = (offset >> 8);
    return (fHostBridge->configRead32(space, offset));
}

void CLASS::configWrite32( IOPCIAddressSpace space, uint32_t offset, uint32_t data )
{
	space.es.registerNumExtended = (offset >> 8);
    fHostBridge->configWrite32(space, offset, data);
}

uint32_t CLASS::configRead32( IOPCIConfigEntry * device, uint32_t offset )
{
	if (!configAccess(device, false)) return (0xFFFFFFFF);

	IOPCIAddressSpace space      = device->space;
	space.es.registerNumExtended = (offset >> 8);
    return (fHostBridge->configRead32(space, offset));
}

void CLASS::configWrite32( IOPCIConfigEntry * device, 
                           uint32_t offset, uint32_t data )
{
	if (!configAccess(device, true)) return;

	IOPCIAddressSpace space      = device->space;
	space.es.registerNumExtended = (offset >> 8);
    fHostBridge->configWrite32(space, offset, data);
}

uint16_t CLASS::configRead16( IOPCIConfigEntry * device, uint32_t offset )
{
	if (!configAccess(device, false)) return (0xFFFF);

	IOPCIAddressSpace space      = device->space;
	space.es.registerNumExtended = (offset >> 8);
    return (fHostBridge->configRead16(space, offset));
}

void CLASS::configWrite16( IOPCIConfigEntry * device, 
                           uint32_t offset, uint16_t data )
{
	if (!configAccess(device, true)) return;

	IOPCIAddressSpace space      = device->space;
	space.es.registerNumExtended = (offset >> 8);
    fHostBridge->configWrite16(space, offset, data);
}

uint8_t CLASS::configRead8( IOPCIConfigEntry * device, uint32_t offset )
{
	if (!configAccess(device, false)) return (0xFF);

	IOPCIAddressSpace space      = device->space;
	space.es.registerNumExtended = (offset >> 8);
    return (fHostBridge->configRead8(space, offset));
}

void CLASS::configWrite8( IOPCIConfigEntry * device, 
                          uint32_t offset, uint8_t data )
{
	if (!configAccess(device, true)) return;

	IOPCIAddressSpace space      = device->space;
	space.es.registerNumExtended = (offset >> 8);
    fHostBridge->configWrite8(space, offset, data);
}
/* -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: t -*- */
