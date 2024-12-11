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
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/pci/IOPCIConfigurator.h>
#if ACPI_SUPPORT
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#endif

__BEGIN_DECLS

#if defined(__i386__) || defined(__x86_64__)

#include <i386/cpuid.h>
#include <i386/cpu_number.h>

extern void mp_rendezvous_no_intrs(
               void (*action_func)(void *),
               void *arg);
extern void mp_rendezvous(
               void (*setup_func)(void *),
               void (*action_func)(void *),
               void (*teardown_func)(void *),
               void *arg);

#define PROBE_DISABLE_INTERRUPTS true

#else /* defined(__i386__) || defined(__x86_64__) */

#define NO_RENDEZVOUS_KERNEL    1
#define cpu_number()    (0)
#define PROBE_DISABLE_INTERRUPTS false

#define KB		(1024ULL)		
#define MB		(1024ULL*KB)
#define GB		(1024ULL*MB)

#endif /* ! defined(__i386__) || defined(__x86_64__) */

__END_DECLS

#define PFM64_SIZE     (2ULL*GB)
#define PFM64_MIN_SIZE (512*MB)
#define PFM64_MAX_SIZE (16ULL*GB)
#define MAX_BAR_SIZE   (1ULL*GB)
// NPHYSMAP; unfortunately this is too low to decode to PCI on some machines
// #define PFM64_MAX  (512LL*GB)
// cap to 32b page number max address
#define PFM64_MAX     (1ULL<<44)


#define DLOGC(configurator, fmt, args...)                  \
    do {                                    \
        if ((configurator->fFlags & kIOPCIConfiguratorIOLog) && !ml_at_interrupt_context())   \
            IOLog(fmt, ## args);            \
        if (configurator->fFlags & kIOPCIConfiguratorKPrintf) \
            kprintf(fmt, ## args);          \
    } while(0)

#define DLOG(fmt, args...)      DLOGC(this, fmt, ## args);
#define DLOGI(fmt, args...)     do { if (dolog) DLOG(fmt, ## args); } while (0);

#define DLOG_RANGE(name, range) if (range) {                                \
                DLOG(name": start/size   = 0x%08llx:0x%08llx,0x%08llx\n",  \
                                        range->start,                       \
                                        range->size,                        \
                                        range->proposedSize) }

#define DLOGI_RANGE(name, range)  do { if (dolog) DLOG_RANGE(name, range); } while (0);

static const char * gPCIResourceTypeName[kIOPCIResourceTypeCount] =
{
    "MEM", "PFM", "I/O", "BUS"
};

#define BRN(type)      ((type) + kIOPCIRangeBridgeMemory)


#define D() "[i%x]%u:%u:%u(0x%x:0x%x)"
#define DEVICE_IDENT(device)				\
		device->id, 						\
		PCI_ADDRESS_TUPLE(device),			\
		(device->vendorProduct & 0xffff), 	\
		(device->vendorProduct >> 16)

#define B() "[i%x]%u:%u:%u(%u:%u)"
#define BRIDGE_IDENT(bridge)	\
		bridge->id, PCI_ADDRESS_TUPLE(bridge), bridge->secBusNum, bridge->subBusNum


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
    uint64_t pfmSize;

    if (!super::init()) return false;

    fWL        = wl;
    fFlags     = flags;
    fPFM64Size = PFM64_SIZE;
    fResetStartTime = 0;
    fResetWaitTime = 0;

    if (PE_parse_boot_argn("pci64", &pfmSize, sizeof(pfmSize)))
    {
        pfmSize *= MB;
        if ((pfmSize <= PFM64_MAX_SIZE) && (pfmSize >= PFM64_MIN_SIZE)) fPFM64Size = pfmSize;
    }

    // Fetch global resources
    if (!createRoot()) return false;

    return (true);
}

IOWorkLoop * CLASS::getWorkLoop() const
{
    return (fWL);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOPCIConfigEntry * CLASS::findEntry(IORegistryEntry * from, IOPCIAddressSpace space)
{
    IOPCIConfigEntry * child;
    IOPCIConfigEntry * entry;
    IOPCIConfigEntry * next;
    uint8_t            bus, dev, func;

    bus   = space.s.busNum;
    dev   = space.s.deviceNum;
    func  = space.s.functionNum;
    next  = fRoot;
    child = NULL;

    // find the IORegistryEntry root bridge as a point of reference for the BDF search
    while(from != NULL)
    {
        IORegistryEntry* parent = from->getParentEntry(gIOServicePlane);
        if (   (OSDynamicCast(IOPCIDevice, parent) == NULL)
            && (OSDynamicCast(IOPCIBridge, parent) == NULL))
        {
            break;
        }
        from = parent;
    }

    while ((entry = next))
    {
        next = NULL;
        for (child = entry->child; child; child = child->peer)
        {
            if (child->hostBridge != from) continue;
            if (bus == entry->secBusNum)
            {
                if (dev  != child->space.s.deviceNum)   continue;
                if (func != child->space.s.functionNum) continue;
                break;
            }
            if (!child->isBridge)            continue;
            else if (bus < child->secBusNum) continue;
            else if (bus > child->subBusNum) continue;
            next = child;
            break;
        }
    }

    return (child);
}

bool CLASS::rangeListContains(IOPCIRange *range, IOPCIScalar address)
{
	while (range)
	{
		DLOG("Comparing address %lx to range [%lx,%lx]\n", address, range->start, range->end);
		if (range->start <= address && address < range->end) return true;
		range = range->next;
	}

    return false;
}

IOPCIConfigEntry * CLASS::findEntryByAddress(IORegistryEntry * from, IOPCIScalar address)
{
    IOPCIConfigEntry * child;
    IOPCIConfigEntry * entry;
    IOPCIConfigEntry * next;

    next  = fRoot;
    child = NULL;

    // find the IORegistryEntry root bridge as a point of reference for the search
    while(from != NULL)
    {
        IORegistryEntry* parent = from->getParentEntry(gIOServicePlane);
        if (   (OSDynamicCast(IOPCIDevice, parent) == NULL)
            && (OSDynamicCast(IOPCIBridge, parent) == NULL))
        {
            break;
        }
        from = parent;
    }

    while ((entry = next))
    {
        next = NULL;
        for (child = entry->child; child; child = child->peer)
        {
            if (child->hostBridge != from) continue;
			DLOG("Checking device " D() "\n", DEVICE_IDENT(child));
            if (!child->isBridge)
            {
                for (int idx = kIOPCIRangeBAR0; idx <= kIOPCIRangeExpansionROM; idx++)
                {
					if (child->ranges[idx])
						DLOG("Comparing address %lx to BAR%u\n", address, idx);
                    if (rangeListContains(child->ranges[idx], address))
                    {
						DLOG("[%s()] Found a match with " D() "\n", __func__, DEVICE_IDENT(child));
                        return child;
                    }
                }
            }
            else
            {
                if (   rangeListContains(child->ranges[kIOPCIRangeBridgeMemory], address)
                    || rangeListContains(child->ranges[kIOPCIRangeBridgePFMemory], address))
                {
					DLOG("[%s()] Found a match with bridge " B() "\n", __func__, BRIDGE_IDENT(child));
                    next = child;
                    break;
                }
            }
        }
    }

    return (child);
}
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn CLASS::configOp(IOService * device, uintptr_t op, void * arg, void * arg2)
{
    IOReturn           ret = kIOReturnUnsupported;
    IOPCIConfigEntry * entry = NULL;
    IOPCIDevice *      pciDevice;

    if ((pciDevice = OSDynamicCast(IOPCIDevice, device)))
        entry = pciDevice->reserved->configEntry;

    switch (op)
    {
        case kConfigOpAddHostBridge:
            ret = addHostBridge(OSDynamicCast(IOPCIHostBridge, device));
			return (ret);
            break;

        case kConfigOpGetState:
			if (!entry) *((uint32_t *) arg) = kPCIDeviceStateDead;
			else
			{
				*((uint32_t *) arg) = entry->deviceState;
			}
			ret = kIOReturnSuccess;
			return (ret);
            break;


		case kConfigOpFindEntry:
            entry = findEntry(device, *((IOPCIAddressSpace *) arg2));
            *((void **) arg) = NULL;
            if (!entry || !entry->dtNub) ret = kIOReturnNotFound;
            else
            {
                entry->dtNub->retain();
                *((void **) arg) = entry->dtNub;
                ret = kIOReturnSuccess;
            }
			return (ret);
            break;

		case kConfigOpFindEntryByAddress:
            entry = findEntryByAddress(device, *((IOPCIScalar *) arg2));
            *((void **) arg) = NULL;
            if (!entry || !entry->dtNub) ret = kIOReturnNotFound;
            else
            {
                entry->dtNub->retain();
                *((void **) arg) = entry->dtNub;
                ret = kIOReturnSuccess;
            }
			return (ret);
            break;

		case kConfigOpTerminated:
			if (entry)
			{
				DLOG("kConfigOpTerminated at " D() "\n", DEVICE_IDENT(entry));
				bridgeRemoveChild(entry->parent, entry);
			}
			ret = kIOReturnSuccess;
			return (ret);
			break;
    }

	if (!entry) return (kIOReturnBadArgument);

	switch (op)
    {
        case kConfigOpNeedsScan:
			entry->deviceState &= ~kPCIDeviceStateScanned;
			ret = kIOReturnSuccess;
            break;

        case kConfigOpLinkInt:
			entry->deviceState |= kPCIDeviceStateLinkInt;
			ret = kIOReturnSuccess;
            break;

        case kConfigOpShadowed:
			entry->configShadow = (uint8_t *) arg;
			ret = kIOReturnSuccess;
			if (!arg) break;
			/* fall thru */
        case kConfigOpPaused:
			DLOG("kConfigOpPaused%s at " D() "\n",
				op == kConfigOpShadowed ? "(shadowed)" : "", DEVICE_IDENT(entry));

			if (kPCIDeviceStateRequestPause & entry->deviceState)
			{
				entry->deviceState &= ~kPCIDeviceStateRequestPause;
				entry->deviceState &= ~kPCIDeviceStateAllocated;
				fWaitingPause--;
			}
			if (!(kPCIDeviceStatePaused & entry->deviceState))
			{
#if 0
				uint32_t reg32;
				reg32 = entry->pausedCommand = configRead16(entry, kIOPCIConfigCommand);
				reg32 &= ~(kIOPCICommandIOSpace
					     | kIOPCICommandMemorySpace
					     | kIOPCICommandBusLead);
				reg32  |= kIOPCICommandInterruptDisable;
				configWrite16(entry, kIOPCIConfigCommand, reg32);

				if (entry->isBridge)
				{
					entry->rangeBaseChanges |= (1 << kIOPCIRangeBridgeBusNumber);
					reg32 = configRead32(entry, kPCI2PCIPrimaryBus);
					reg32 &= ~0x00ffffff;
					configWrite32(entry, kPCI2PCIPrimaryBus, reg32);
				}
#endif
				entry->deviceState |= kPCIDeviceStatePaused;
				if (fStates & kIOPCIConfiguratorMPSOverride)
				{
					fStates |= kIOPCIConfiguratorMPSOverridePause;
				}
			}
			ret = kIOReturnSuccess;
            break;

        case kConfigOpUnpaused:
			if (kPCIDeviceStatePaused & entry->deviceState)
			{
				DLOG("kConfigOpUnpaused at " D() "\n", DEVICE_IDENT(entry));
//				configWrite16(entry, kIOPCIConfigCommand, entry->pausedCommand);
				entry->deviceState &= ~kPCIDeviceStatePaused;
				if (fStates & kIOPCIConfiguratorMPSOverridePause)
				{
					fStates &= ~kIOPCIConfiguratorMPSOverridePause;
				}
			}
			ret = kIOReturnSuccess;
            break;

        case kConfigOpTestPause:
			ret = kIOReturnSuccess;
            break;

        case kConfigOpScan:
			if (kPCIDeviceStateScanned & entry->deviceState)
			{
				*((void **)arg) = NULL;
				ret = kIOReturnSuccess;
				return (ret);
				break;
			}

			// always reset regs due to hotp interrupt clearing memory ranges
			entry->rangeBaseChanges |= ((1 << kIOPCIRangeBridgeMemory)
									  |  (1 << kIOPCIRangeBridgePFMemory));

			// fall thru

        case kConfigOpRealloc:

			DLOG("[ PCI configuration begin ]\n");
			fChangedServices = OSSet::withCapacity(8);
			configure(0);
			DLOG("[ PCI configuration end, bridges %d, devices %d, changed %d, waiting %d ]\n", 
				  fBridgeCount, fDeviceCount, fChangedServices->getCount(), fWaitingPause);

			if (entry)
			{
				entry->deviceState &= ~kPCIDeviceStateLinkInt;
			}

			if (arg)
			{
				*((void **)arg) = fChangedServices;
			}
			else
			{
				fChangedServices->release();
			}
			fChangedServices = 0;
			ret = kIOReturnSuccess;
			return (ret);
            break;


        case kConfigOpEject:
			entry->deviceState |= kPCIDeviceStateEjected;
			ret = kIOReturnSuccess;
            break;

        case kConfigOpKill:
			entry->deviceState |= kPCIDeviceStateToKill;
			ret = kIOReturnSuccess;
            break;

		case kConfigOpProtect:
			entry->deviceState = (entry->deviceState & ~(kPCIDeviceStateConfigRProtect|kPCIDeviceStateConfigWProtect))
								| (*((uint32_t *) arg));
			DLOG("kConfigOpProtect at " D() " prot %x\n",
				 DEVICE_IDENT(entry), *((uint32_t *) arg));

			ret = kIOReturnSuccess;
            break;
    }

    return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if ACPI_SUPPORT
void CLASS::removeFixedRanges(IOPCIConfigEntry * bridge)
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
	iter = IORegistryIterator::iterateOver(bridge->acpiDevice, gIOPCIACPIPlane, kIORegistryIterateRecursively);
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
					ok = IOPCIRangeListAllocateSubRange(bridge->ranges[BRN(type)], range);
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

    root = IOMallocType(IOPCIConfigEntry);
    if (!root) return (false);

    root->id           = ++fNextID;
    root->isHostBridge = true;
    root->isBridge     = true;

    root->secBusNum    = 0xff;
    root->subDeviceNum = 0;
    root->endDeviceNum = 31;


	IOLog  ("pci (build %s %s), flags 0x%x\n", __TIME__, __DATE__, gIOPCIFlags);
	kprintf("pci (build %s %s), flags 0x%x\n", __TIME__, __DATE__, gIOPCIFlags);

    root->deviceState |= kPCIDeviceStateScanned | kPCIDeviceStateConfigurationDone;
    fRoot = root;

    return (true);
}

IOReturn CLASS::addHostBridge(IOPCIHostBridge * hostBridge)
{
    IOPCIConfigEntry * bridge;
	IOPCIAddressSpace  space;
    IOPCIRange *       range;
    uint64_t           start, size;
    bool               ok, hasHostMemory, hasHost64;

    bridge = IOMallocType(IOPCIConfigEntry);
    if (!bridge) return (kIOReturnNoMemory);

    bridge->id           = ++fNextID;
    bridge->classCode    = 0x060000;
    bridge->headerType   = kPCIHeaderType1;
    bridge->secBusNum    = hostBridge->firstBusNum();
    bridge->subBusNum    = hostBridge->lastBusNum();
    bridge->subDeviceNum = 0;
    bridge->endDeviceNum = 31;
//#warning bus52
//    if (bridge->subBusNum > 250) bridge->subBusNum = 52;
    bridge->space        = hostBridge->getBridgeSpace();

    bridge->dtNub           = hostBridge->getProvider();
    bridge->dtEntry         = bridge->dtNub;
#if ACPI_SUPPORT
    bridge->acpiDevice      = IOPCICopyACPIDevice(bridge->dtNub);
#endif
    bridge->hostBridge      = hostBridge;
    bridge->isHostBridge    = true;
    bridge->isBridge        = true;
    bridge->supportsHotPlug = kPCIStatic;
    bridge->hostBridgeEntry = bridge;

    if (OSDynamicCast(IOPCIDevice, bridge->dtNub)) panic("!host bridge");

	// expressEndpointMaxReadRequestSize override only applies to endpoint devices
	bridge->expressEndpointMaxReadRequestSize = -1;
    if (bridge->dtNub->getProperty(kIOPCIExpressEndpointMaxReadRequestSize))
    {
        OSData* maxReadRequestSizeOverride = NULL;
        maxReadRequestSizeOverride = OSDynamicCast(OSData, bridge->dtNub->getProperty(kIOPCIExpressEndpointMaxReadRequestSize));
        if(maxReadRequestSizeOverride != NULL)
        {
			assert(maxReadRequestSizeOverride->getLength() == sizeof(uint32_t));
            bridge->expressEndpointMaxReadRequestSize = *reinterpret_cast<const uint32_t*>(maxReadRequestSizeOverride->getBytesNoCopy());
            DLOG("MRRS override = %u\n", bridge->expressEndpointMaxReadRequestSize)
        }
    }

    space.bits = 0;
    fRootVendorProduct = configRead32(bridge, kIOPCIConfigVendorID, &space);
#if defined(__i386__) || defined(__x86_64__)
    if ( (0x27A08086 == fRootVendorProduct)
      || (0x27AC8086 == fRootVendorProduct)
      || (0x25C08086 == fRootVendorProduct)
      || (CPUID_FEATURE_VMM & cpuid_features()))
#endif
        fFlags &= ~kIOPCIConfiguratorPFM64;
    DLOG("root id 0x%x, flags 0x%x\n", fRootVendorProduct, (int) fFlags);

    range = IOPCIRangeAlloc();
    start = bridge->secBusNum;
    size  = bridge->subBusNum - bridge->secBusNum + 1;

    DLOG("host bridge acpi bus range 0x%llx len 0x%llx\n", start, size);

    IOPCIRangeInitAlloc(range, kIOPCIResourceTypeBusNumber, start, size, kPCIBridgeBusNumberAlignment);
    bridge->ranges[kIOPCIRangeBridgeBusNumber] = range;

    DLOG("added(%s) host bridge resource %s 0x%llx len 0x%llx\n", 
        "ok", gPCIResourceTypeName[kIOPCIResourceTypeBusNumber], 
        range->start, range->size);

    for (int type = hasHostMemory = hasHost64 = 0; type < kIOPCIResourceTypeCount; type++)
    {
        IOPCIRange * thisRange;
        for (thisRange = hostBridge->reserved->rangeLists[type];
			 thisRange;
			 thisRange = thisRange->next)
        {
			start = thisRange->start;
			size  = thisRange->size;
            DLOG("reported host bridge resource %s 0x%llx len 0x%llx\n", 
                gPCIResourceTypeName[type], 
                start, size);
            ok = IOPCIRangeListAddRange(&bridge->ranges[BRN(type)],
                                        type, start, size);
            DLOG("added(%s) host bridge resource %s 0x%llx len 0x%llx\n", 
                ok ? "ok" : "!", gPCIResourceTypeName[type], 
                start, size);
            if (!ok) continue;
            if ((kIOPCIResourceTypeMemory == type) || (kIOPCIResourceTypePrefetchMemory == type))
            {
				hasHostMemory = true;
				hasHost64 |= (0 != (start >> 32));
            }
        }
    }

#if ACPI_SUPPORT
	if (bridge->acpiDevice)
		removeFixedRanges(bridge);

	if (hasHostMemory && !hasHost64 && !fAddedHost64)
	{
		uint64_t  start, size;
		uint32_t  cpuPhysBits;

		cpuPhysBits = cpuid_info()->cpuid_address_bits_physical;
		size  = fPFM64Size;
		start = (1ULL << cpuPhysBits);
		if (start > PFM64_MAX) start = PFM64_MAX;
		start -= size;
		ok = IOPCIRangeListAddRange(&bridge->ranges[kIOPCIRangeBridgeMemory],
									kIOPCIResourceTypeMemory, start, size);
		DLOG("added(%s) host bridge pfm64 (%d cpu) 0x%llx, 0x%llx\n",
			ok ? "ok" : "!", cpuPhysBits, start, size);
		fAddedHost64 = ok;
	}
#endif /* ACPI_SUPPORT */

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
        regData.lengthLo = static_cast<UInt32>(range->size);
        if (i <= kIOPCIRangeExpansionROM)
        {
            regData.physHi.s.registerNum = barRegisters[i];
            if (range->start)
            {
				IOPCIPhysicalAddress assignedData;
				assignedData = regData;
                assignedData.physHi.s.reloc = 1;
                assignedData.physMid = (range->start >> 32ULL);
                assignedData.physLo  = static_cast<UInt32>(range->start);
                assignedProp->appendBytes(&assignedData, sizeof(assignedData));
            }
			// reg gets requested length
			regData.lengthHi             = (range->proposedSize >> 32ULL);
			regData.lengthLo             = static_cast<UInt32>(range->proposedSize);
            regProp->appendBytes(&regData, sizeof(regData));
        }
        else
        {
            regData.physHi.s.reloc       = 1;
            regData.physMid              = (range->start >> 32ULL);
            regData.physLo               = static_cast<UInt32>(range->start);
            regData.physHi.s.busNum      = 0;
            regData.physHi.s.deviceNum   = 0;
            regData.physHi.s.functionNum = 0;
            regData.physHi.s.registerNum = 0;
            rangeProp->appendBytes(&regData, sizeof(regData.physHi) + sizeof(regData.physMid) + sizeof(regData.physLo));
            rangeProp->appendBytes(&regData, sizeof(regData));
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
    OSDictionary *      propTable;
    uint32_t            value;
    uint32_t            vendor, product, classCode, revID;
    uint32_t            subVendor = 0, subProduct = 0;
    OSData *            prop;
    const char *        name;
    const OSSymbol *    nameProp;
    OSNumber *          num;
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

    if (device->dtEntry && ((nameProp = device->dtEntry->copyName())))
	{
		out += strlcpy(out, nameProp->getCStringNoCopy(), 31) + 1;
		nameProp->release();
	}

    // out - compatBuf has a max value of 128
    prop = OSData::withBytes( compatBuf, static_cast<unsigned int>(out - compatBuf) );
    if (prop)
    {
        propTable->setObject("compatible", prop );
        prop->release();
    }

    nameProp = OSSymbol::withCString( name );
    if (nameProp)
    {
        propTable->setObject( gIONameKey, (OSSymbol *) nameProp);
        nameProp->release();
    }

    prop = OSData::withBytes( name, static_cast<unsigned int>(strlen(name) + 1));
    if (prop)
    {
        propTable->setObject("name", prop );
        prop->release();
    }

    if (device->supportsHotPlug
      && (num = OSNumber::withNumber(device->supportsHotPlug, 8)))
	{
		propTable->setObject(gIOPCIHPTypeKey, num);
		num->release();
	}
    if (kPCIHotPlugRoot == device->supportsHotPlug)
        propTable->setObject(kIOPCIHotPlugKey, kOSBooleanTrue);
    else if (kPCILinkChange == device->supportsHotPlug)
        propTable->setObject(kIOPCILinkChangeKey, kOSBooleanTrue);

	if (kPCIHotPlugTunnel == device->parent->supportsHotPlug)
        propTable->setObject(gIOPCITunnelledKey, kOSBooleanTrue);

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

	if (device->commandCompleted)
        propTable->setObject(kIOPCISlotCommandCompleted, kOSBooleanTrue);
	if (device->powerController)
        propTable->setObject(kIOPCISlotPowerController, kOSBooleanTrue);

    if ((num = OSNumber::withNumber(device->linkCaps, 32)))
	{
		propTable->setObject(kIOPCIExpressLinkCapabilitiesKey, num);
		num->release();
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

uint8_t CLASS::IOPCIIsHotplugPort(IOPCIConfigEntry * bridge)
{
    uint8_t type = kPCIStatic;

#if ACPI_SUPPORT

    IORegistryEntry *bridgeDevice = bridge->dtEntry;
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

    // Look for the PCIe native hotplug capability 
    if (type == kPCIStatic)
    {
        uint16_t expressCaps = configRead16(bridge, bridge->expressCapBlock + 0x02);
        uint32_t slotCaps = configRead32(bridge, bridge->expressCapBlock + 0x14);

        // Device/port type == root port, the slot is implemented, and the slot is hot-plug capable
        if (((expressCaps & kPCIECapDevicePortType) == kPCIEPortTypePCIERootPort)
            && (expressCaps & kPCIECapSlotConnected)
            && (slotCaps & kSlotCapHotplug))
        {
            type = kPCIHotPlugRoot;
        }
    }

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

    if (dtEntry->inPlane(gIOServicePlane)) return;

    location = dtEntry->copyLocation();
    if (!location)                         return;

    uint64_t devfn = strtoul(location->getCStringNoCopy(), NULL, 16);
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
        if (!match->dtEntry)
        {
            match->dtEntry = dtEntry;
            DLOGC(context->me, "Found PCI device for DT entry [%s] " D() "\n",
                 dtEntry->getName(), DEVICE_IDENT(match));
        }
    }
    else
    {
        DLOGC(context->me, "NOT FOUND: PCI device for DT entry [%s] " D() "\n",
                dtEntry->getName(), 0, bridge->secBusNum, deviceNum, functionNum, 0, 0);
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
        if ((child->space.s.deviceNum - bridge->subDeviceNum) == deviceNum &&
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
            DLOGC(context->me, "Found PCI device for ACPI entry [%s] " D() "\n",
                 dtEntry->getName(), DEVICE_IDENT(match));
        }
    }
    else
    {
        DLOGC(context->me, "NOT FOUND: PCI device for ACPI entry [%s] " D() "\n",
                dtEntry->getName(), 0, bridge->secBusNum, deviceNum, functionNum, 0, 0);
    }
}

#endif /* ACPI_SUPPORT */

//---------------------------------------------------------------------------

void CLASS::bridgeConnectDeviceTree(IOPCIConfigEntry * bridge)
{
    IORegistryEntry *   dtBridge = bridge->dtEntry;
    MatchDTEntryContext context;

    if ((kPCIHotPlugTunnel != bridge->supportsHotPlug)
     || (kPCIHotPlugTunnelRoot == bridge->parent->supportsHotPlug))
    {
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
    }

    FOREACH_CHILD(bridge, child)
    {
        if (child->dtEntry && child->dtEntry->getProperty(gIOPCIDeviceHiddenKey))
        {
            child->deviceState |= kPCIDeviceStateHidden;
        }
    }
}

//---------------------------------------------------------------------------
void CLASS::topologyMPSOverride(IOPCIConfigEntry * node)
{
	// preorderTraversal
	if (node != NULL)
	{
		node->expressMaxPayload = MIN_MPS;
		topologyMPSOverride(node->peer);
		topologyMPSOverride(node->child);
	}
}

//---------------------------------------------------------------------------

void CLASS::bridgeMPSOverride(IOPCIConfigEntry * bridge)
{
    IOPCIConfigEntry * child;
    for (child = bridge->child; child; child = child->peer)
    {
        if (kPCIDeviceStateHidden & child->deviceState)     continue;
        if ((child->expressCapBlock == 0) || (child->dtEntry == NULL))     continue;
        OSData* maxPayloadOverride = OSDynamicCast(OSData, child->dtEntry->getProperty(kIOPCIExpressMaxPayloadSize));
        if (maxPayloadOverride == NULL)     continue;
        uint8_t expressCaps = child->expressCaps & kPCIECapDevicePortType;
        // MPS override should only be set at Root Port, Switch USP, or EP
        if ((expressCaps == kPCIEPortTypeUpstreamPCIESwitch) || (expressCaps == kPCIEPortTypePCIERootPort) || (expressCaps == kPCIEPortTypePCIEEndpoint))
        {
            uint8_t overrideMaxPayload = *reinterpret_cast<const uint32_t*>(maxPayloadOverride->getBytesNoCopy());
            if (overrideMaxPayload > child->expressMaxPayload)
            {
                IOLog("Cannot override %s's MPS to %u which is higher than its MPS Capability %u\n", child->dtEntry->getName(), overrideMaxPayload, child->expressMaxPayload);
            }
            else if (overrideMaxPayload < child->expressMaxPayload)
            {
                child->expressMaxPayload = overrideMaxPayload;
                DLOG("MPS override for" D() " =  %u\n", DEVICE_IDENT(child), child->expressMaxPayload);
                // If the MPS override is for the root port's link partner, then also change the root port's MPS
                if (child->parent == child->rootPortEntry)
                {
                    child->rootPortEntry->expressMaxPayload = child->expressMaxPayload;
                }
            }
        }
    }
}

//---------------------------------------------------------------------------

void CLASS::bridgeFinishProbe(IOPCIConfigEntry * bridge)
{
    IOPCIConfigEntry *  parent;
	OSObject *          obj;
    OSNumber *          num;
	IOPCIRange *        childRange;
	bool				oneChild;
	bool				tbok = (0 == (kIOPCIConfiguratorNoTB & gIOPCIFlags));

	oneChild = (bridge->child && !bridge->child->peer);

    FOREACH_CHILD(bridge, child)
    {
        if (!child->isBridge)
        {
			child->supportsHotPlug = bridge->supportsHotPlug;
			continue;
		}
		if (kPCIDeviceStateTreeConnected & child->deviceState) continue;
//        child->deviceState |= kPCIDeviceStateTreeConnected;
		do
		{
			if (kPCIHotPlugTunnelRoot == bridge->supportsHotPlug)
			{
				if (bridge->space.s.busNum && (child == bridge->child))
				{
					// assume the first DSB of the TB root is for NHI
					child->supportsHotPlug = kPCIStaticTunnel;
					child->linkInterrupts  = false;
					DLOG("tunnel controller " D() "\n", DEVICE_IDENT(child));
					continue;
				}
				if (child->dtEntry
					 && (num = OSDynamicCast(OSNumber, child->dtEntry->getProperty(gIOPCIHotplugCapableKey)))
					 && !num->unsigned32BitValue())
				{
					// non tunneled but shared HW with root
					child->supportsHotPlug = kPCIStaticShared;
					child->linkInterrupts  = false;
					DLOG("shared " D() "\n", DEVICE_IDENT(child));
					continue;
				}
			}

			if (tbok && child->dtEntry)
			{
			    if (child->dtEntry->getProperty(gIOPCIThunderboltKey))
			    {
					child->supportsHotPlug = kPCIHotPlugTunnelRoot;
					continue;
				}
				if ((obj = child->dtEntry->copyProperty(gIOPCIThunderboltKey, gIODTPlane, 
													   kIORegistryIterateRecursively)))
				{
					obj->release();
					child->supportsHotPlug = kPCIHotPlugTunnelRootParent;
					continue;
				}
			}

			if ((kPCIHotPlugTunnelRoot == bridge->supportsHotPlug)
			  || (kPCIHotPlugTunnel == bridge->supportsHotPlug))
			{
				child->supportsHotPlug = kPCIHotPlugTunnel;
				continue;
			}

			if (kPCIStaticShared == bridge->supportsHotPlug)
			{
				child->supportsHotPlug = kPCIStaticShared;
				continue;
			}
	
			if ((kPCIHotPlugRoot == bridge->supportsHotPlug)
			  || (kPCIHotPlug == bridge->supportsHotPlug))
			{
				child->supportsHotPlug = kPCIHotPlug;
				continue;
			}

			if (child->headerType == kPCIHeaderType2)
				child->supportsHotPlug = kPCIHotPlugRoot;
			else if (child->dtEntry)
			{
				child->supportsHotPlug = IOPCIIsHotplugPort(child);
			}
			else
				child->supportsHotPlug = kPCIStatic;
		}
		while (false);
		if ((kPCIHotPlugTunnel != (kPCIHPTypeMask & child->supportsHotPlug))
		  || (kPCIHotPlugTunnelRootParent == child->supportsHotPlug))
		{
			child->linkInterrupts  = false;
		}
    }

	bridge->countMaximize = 0;
	uint32_t flags = kIOPCIRangeFlagMaximizeRoot | kIOPCIRangeFlagNoCollapse;

    FOREACH_CHILD(bridge, child)
    {
		if ((kPCIHotPlugTunnelRootParent == child->supportsHotPlug)
		 || (kPCIHotPlugTunnelRoot == child->supportsHotPlug)) bridge->countMaximize++;
	}

	for (parent = bridge; parent; parent = parent->parent)
	{
		if (parent->countMaximize > 1)
		{
			flags = kIOPCIRangeFlagMaximizeSize | kIOPCIRangeFlagNoCollapse;
			break;
		}
	}
	
    FOREACH_CHILD(bridge, child)
    {
        if (!child->isBridge) continue;
		if (kPCIDeviceStateTreeConnected & child->deviceState) continue;
        child->deviceState |= kPCIDeviceStateTreeConnected;

		DLOG("bridge " D() " supportsHotPlug %d, linkInterrupts %d\n",
			  DEVICE_IDENT(child),
			  child->supportsHotPlug, child->linkInterrupts);

        for (int i = kIOPCIRangeBridgeMemory; i < kIOPCIRangeCount; i++)
        {
            childRange = child->ranges[i];
            if (!childRange)
                continue;

			if (kPCIHotPlugTunnelRootParent == child->supportsHotPlug)
			{
				childRange->flags |= flags;
				child->rangeSizeChanges |= (1 << BRN(childRange->type));
			}
			else if (kPCIHotPlugTunnelRoot == child->supportsHotPlug)
			{
				childRange->flags |= flags;
				childRange->start = 0;
				child->rangeSizeChanges |= (1 << BRN(childRange->type));
			}
#if !ACPI_SUPPORT
			else if (oneChild
					&& (kPCIHotPlugTunnel == child->supportsHotPlug))
			{
				// Use maximum possible ranges if this is the only child bridge.
				childRange->flags |= kIOPCIRangeFlagMaximizeSize;
			}
#endif
			else if (child->linkInterrupts 
				&& (kPCIStatic != (kPCIHPTypeMask & child->supportsHotPlug)))
			{
				childRange->flags |= kIOPCIRangeFlagSplay;
			}
			else if (child->dtEntry && child->dtEntry->getProperty(kIOPCIBridgeSplayMask))
			{
				OSData* splayMaskData = OSDynamicCast(OSData, child->dtEntry->getProperty(kIOPCIBridgeSplayMask));
				if(splayMaskData != NULL)
				{
					assert(splayMaskData->getLength() == sizeof(uint32_t));
					uint32_t splayMask = *reinterpret_cast<const uint32_t*>(splayMaskData->getBytesNoCopy());
					childRange->flags |= (splayMask & (1 << i)) ? kIOPCIRangeFlagSplay : 0;
				}
			}
			// Don't collapse static, built-in bridge allocations (e.g. during crash recovery termination and re-probe).
			if (child->dtEntry && child->dtEntry->getProperty("built-in")
				&& (kPCIStatic == (kPCIHPTypeMask & child->supportsHotPlug)))
			{
				childRange->flags |= kIOPCIRangeFlagNoCollapse;
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
    IOPCIDevice       * nub;
    OSDictionary      * initFrom;

//    DLOG("bridgeConstructDeviceTree(%p)\n", bridge);
    if (!dtBridge) return (ok);

    int32 = 3;
    dtBridge->setProperty("#address-cells", &int32, sizeof(int32));
    int32 = 2;
    dtBridge->setProperty("#size-cells", &int32, sizeof(int32));

    // Create missing device-tree entries for any child devices.

    FOREACH_CHILD( bridge, child )
    {
        IOPCIDevice     * pciDevice;
		IORegistryEntry * copyReg;
        OSDictionary    * propTable;
        OSObject        * obj;
        const OSSymbol  * sym;
		bool              addProps;
		bool              initDT;

        if (kPCIDeviceStatePropertiesDone & child->deviceState) continue;
        child->deviceState |= kPCIDeviceStatePropertiesDone;

        DLOG("bridgeConstructDeviceTree at " D() "\n", DEVICE_IDENT(child));

        propTable = constructProperties(child);
        if (!propTable) continue;

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

        pciDevice = OSDynamicCast(IOPCIDevice, child->dtNub);
        addProps = true;
        if (!pciDevice)
        {
            nub = OSTypeAlloc(IOPCIDevice);
            if (!nub) continue;
            initFrom = 0;
            initDT = false;
            ok = false;
			if (child->dtEntry) 
            {
#if ACPI_SUPPORT
                ok = nub->init(child->dtEntry, gIODTPlane);
				if (ok) child->deviceState |= kPCIDeviceStateAttached;
                child->dtNub = child->dtEntry = 0;
                initDT = true;
#else  /* !ACPI_SUPPORT */
			    initFrom = child->dtEntry->dictionaryWithProperties();
#endif /* !ACPI_SUPPORT */
            }
            if (!initDT)
            {
                if (!initFrom) 
                {
                    initFrom = propTable;
                    initFrom->retain();
                    addProps = false;
                }
                ok = (nub->init(initFrom) && nub->attachToParent(dtBridge, gIODTPlane));
				if (ok) child->deviceState |= kPCIDeviceStateAttached;
                initFrom->release();
            }
            nub->release();
            if (!ok) continue;
            nub->reserved->configEntry = child;
            nub->reserved->hostBridge = child->hostBridge;
            child->dtNub = nub;
#if ACPI_SUPPORT
            child->dtEntry = nub;
            copyReg = child->acpiDevice;
#else /* !ACPI_SUPPORT */
            copyReg = child->dtEntry;
#endif /* !ACPI_SUPPORT */

            if (child->dtEntry && (child->dtEntry != nub)) nub->setProperty(kIOPCIDeviceDeviceTreeEntryKey, child->dtEntry);
            if (!initDT)
            {
                if (copyReg && !copyReg->propertyExists("pci-generate-name"))
                {
                    if ((sym = copyReg->copyName()))
                    {
                        nub->setName(sym);
                        sym->release();
                    }
                }
                else if ((sym = OSDynamicCast(OSSymbol, propTable->getObject(gIONameKey))))
                    nub->setName(sym);
            }
        }

        if (addProps)
        {
            // update existing device
            OSCollectionIterator * propIter =
                OSCollectionIterator::withCollection(propTable);
            if (propIter)
            {
                const OSSymbol * propKey;

                while ((propKey = (const OSSymbol *)propIter->getNextObject()))
                {
                    if (   !child->dtNub->getProperty(propKey)
                        || (propKey->isEqualTo("name") && child->dtNub->propertyExists("pci-generate-name"))
                        || (propKey->isEqualTo("ranges"))
                        || (propKey->isEqualTo("reg"))
                        || (propKey->isEqualTo("assigned-addresses"))
                        || (propKey->isEqualTo("pcidebug")))
                    {
                        obj = propTable->getObject(propKey);
                        child->dtNub->setProperty(propKey, obj);
                    }
                }
                propIter->release();
            }
        }

        if (pciDevice
          && (kPCIDeviceStatePaused & child->deviceState)
            && pciDevice->getProperty(kIOPCIResourcedKey))
        {
            // relocate existing device
            DLOG("IOPCIDevice::relocate at " D() "\n", DEVICE_IDENT(child));
            for (uint32_t idx = 0; idx < kIOPCIRangeCount; idx++)
            {
                IOPCIRange * range;
                range = child->ranges[idx];
                if (!range)	continue;
                if (range->size < range->proposedSize)
                {
                    DLOG("  %s ", gPCIResourceTypeName[range->type]);
                    DLOG_RANGE("short range", range);
                }
            }
            pciDevice->relocate(false);
        }
        propTable->release();
    }
	dtBridge->setProperty(kIOPCIConfiguredKey, kOSBooleanTrue);

    return (ok);
}

//---------------------------------------------------------------------------

void CLASS::maskUR(IOPCIConfigEntry *entry, bool mask)
{
	uint16_t deviceControl = 0;

	if (!entry->expressCapBlock)
	{
		return;
	}

	if (mask)
	{
		deviceControl = configRead16(entry, entry->expressCapBlock + 0x08);
		configWrite16(entry, entry->expressCapBlock + 0x08, deviceControl & ~0x8);
		entry->expressErrorReporting = deviceControl & 0xF;
		if (entry->aerCapBlock)
		{
			// Set entry's header log overflow mask, if unset
			entry->aerCorMask = configRead16(entry, entry->aerCapBlock + 0x14);
			configWrite16(entry, entry->aerCapBlock + 0x14, entry->aerCorMask | (1 << 15));
		}
	}
	else
	{
		// Clear entry's error status bits associated with the bus scan UR
		if (entry->aerCapBlock)
		{
			configWrite32(entry, entry->aerCapBlock + 0x04, 1 << 20);
			configWrite16(entry, entry->aerCapBlock + 0x10, (1 << 13) | (1 << 15));
			configWrite16(entry, entry->aerCapBlock + 0x14, entry->aerCorMask);
		}
		configWrite16(entry, entry->expressCapBlock + 0x0A, (1 << 0) | (1 << 3));

		deviceControl = configRead16(entry, entry->expressCapBlock + 0x08);
		deviceControl |= entry->expressErrorReporting;
		configWrite16(entry, entry->expressCapBlock + 0x08, deviceControl);
	}
}

bool CLASS::endpointPresent(IOPCIConfigEntry * bridge)
{
	if (bridge->dtEntry && bridge->dtEntry->propertyExists(kIOPCIEndpointPrsnt))
	{
		return true;
	}
	return false;
}

uint16_t CLASS::waitForLinkUp(IOPCIConfigEntry * bridge)
{
	// Following conventional reset, wait up to 1s for the link to
	// train (PCIe base spec section 6.6.1).
	uint16_t linkStatus = configRead16(bridge, bridge->expressCapBlock + 0x12);
	uint64_t startTime = fResetStartTime;
	uint64_t timeout = 0;
	uint64_t timeElapsed = 0;
	OSData *waitData = NULL;

	// rdar://103579149 shows that there is a card that is out of spec. It takes longer than 1s to link train
	// if property exists, overwrite the wait time with the value from EDT
	if (bridge->dtEntry && (waitData = OSDynamicCast(OSData, bridge->dtEntry->getProperty(kIOPCIWaitForLinkUpKey))))
	{
		uint32_t waitTime = *((uint32_t *) waitData->getBytesNoCopy());;
		fResetWaitTime = (uint64_t) waitTime;
	}
	clock_interval_to_absolutetime_interval(fResetWaitTime, kMillisecondScale, &timeout);
	while (!(kLinkStatusDataLinkLayerLinkActive & linkStatus) || (kLinkStatusLinkTraining & linkStatus))
	{
		IOSleepWithLeeway(1, 1);
		if ((timeElapsed = (mach_absolute_time() - startTime)) >= timeout)
		{
			IOLog("Endpoint failed to respond successfully after spec mandated wait time\n");
			break;
		}
		linkStatus = configRead16(bridge, bridge->expressCapBlock + 0x12);
	}

	if ((kLinkStatusDataLinkLayerLinkActive & linkStatus) && (timeElapsed < timeout))
	{
		uint64_t nanoTime = 0;
		absolutetime_to_nanoseconds(timeElapsed, &nanoTime);
		DLOG("IOPCIConfigurator::waitForLinkUp waited for %llu ms for " B() " to link up\n", nanoTime/1000, BRIDGE_IDENT(bridge));
		// Wait 100 ms after link up before making any configuration requests (PCI Express Base 5.0 - 6.6.1)
		// Following a Conventional Reset of a device, within 1.0 s the device must be able to receive a Configuration Request and return a Successful Completion if the Request is valid (PCI Express Base 5.0 - 6.6.1)
		// We only wait if we have not reached the upper bound of the wait time
		IOSleep(100);
	}

	return linkStatus;
}

uint32_t CLASS::retrainLink(IOPCIConfigEntry * bridge)
{
	uint16_t linkStatus = 0;

	// Per the PCIe base spec's implementation note "Avoiding Race Conditions
	// When Using the Retrain Link Bit", wait for the Link Training bit to go
	// to 0 before initiating a retrain.
	AbsoluteTime deadline, now = 0;
	clock_interval_to_deadline(1, kSecondScale, &deadline);
	do
	{
		linkStatus = configRead16(bridge, bridge->expressCapBlock + 0x12);
		if ((linkStatus & (1 << 11)) == 0)
		{
			break;
		}
		IOSleep(1);
		clock_get_uptime(&now);
	}
	while (AbsoluteTime_to_scalar(&now) < AbsoluteTime_to_scalar(&deadline));

	if (AbsoluteTime_to_scalar(&now) >= AbsoluteTime_to_scalar(&deadline))
	{
		IOLog("Link Training bit didn't reach 0 within 1s.");
		return kIOReturnError;
	}

	// Set the Link Retrain bit
	uint16_t linkControl = configRead16(bridge, bridge->expressCapBlock + 0x10);
	linkControl |= (1 << 5);
	configWrite16(bridge, bridge->expressCapBlock + 0x10, linkControl);

	return kIOReturnSuccess;
}

void CLASS::bridgeScanBus(IOPCIConfigEntry * bridge, uint8_t busNum)
{
    IOPCIAddressSpace   space;
    IOPCIConfigEntry *  child;
    uint32_t		    noLink = 0;
    bool     			bootDefer = false;
    UInt8               scanDevice, scanFunction, lastFunction;
    bool                ignoreNoLink = false;
	uint16_t            linkStatus;

	space.bits = 0;
	space.s.busNum = busNum;

	if (bridge->expressCapBlock) do
	{
		bootDefer = (bridge->linkInterrupts 
			&& (kIOPCIConfiguratorBootDefer == (kIOPCIConfiguratorBootDefer & fFlags)));
		if (bootDefer)
			break;

		if (bridge->dtEntry && bridge->dtEntry->getProperty(kIOPCIIgnoreLinkStatusKey))
		{
			DLOG("bridge " D() " ignore link status\n", DEVICE_IDENT(bridge));
			ignoreNoLink = true;
		}

		linkStatus  = configRead16(bridge, bridge->expressCapBlock + 0x12);
		if ((kLinkCapDataLinkLayerActiveReportingCapable & bridge->linkCaps)
			&& !(kLinkStatusDataLinkLayerLinkActive & linkStatus)
			&& !ignoreNoLink)
		{
			noLink = kPCIDeviceStateNoLink;
			if (endpointPresent(bridge))
			{
				// some endpoint take a long time to train so we wait rdar://103579149
				linkStatus = waitForLinkUp(bridge);
				if (kLinkStatusDataLinkLayerLinkActive & linkStatus)
				{
					noLink = 0;
				}
				else if (bridge->dtEntry && bridge->dtEntry->getProperty(kIOPCIRetrainLinkKey))
				{
					// if retrain did not result in a link up, remove the retrain property so we don't retrain on wake
					bridge->dtEntry->removeProperty(kIOPCIRetrainLinkKey);
				}
			}
		}

		if (linkStatus == 0xFFFF)
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

		DLOG("bridge " D() " %s linkStatus 0x%04x, linkCaps 0x%04x\n",
			 DEVICE_IDENT(bridge), 
			 noLink    ? "nolink " : "",
			 linkStatus, bridge->linkCaps);

		if (noLink)
		{
			if (kPCIHotPlugTunnel == (kPCIHPTypeMask & bridge->supportsHotPlug))
			{
				// disable mmio, bus leading, and I/O space before making changes to the memory ranges
				uint16_t commandRegister = configRead16(bridge, kIOPCIConfigurationOffsetCommand);
				configWrite16(bridge, kIOPCIConfigurationOffsetCommand, commandRegister & ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace | kIOPCICommandBusLead));
				configWrite32(bridge, kPCI2PCIMemoryRange,         0);
				configWrite32(bridge, kPCI2PCIPrefetchMemoryRange, 0);
				configWrite32(bridge, kPCI2PCIPrefetchUpperBase,   0);
				configWrite32(bridge, kPCI2PCIPrefetchUpperLimit,  0);

				// rdar://87701618: re-enable bus leading to ensure the bridge can generate MSI writes for hotplug interrupts
				configWrite16(bridge, kIOPCIConfigurationOffsetCommand, commandRegister);
			}
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
#if 0
		static const uint8_t deviceMap[32] = {
//		  0,   1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		  0,   1,  3,  2,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		  16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };
#endif
		// Scan all PCI devices and functions on the secondary bus. Disable UR reporting,
		// as these errors are benign during this sequence.
		maskUR(bridge, true);

		for (scanDevice = bridge->subDeviceNum; scanDevice <= bridge->endDeviceNum; scanDevice++)
		{
			bool isMFD = false;
			lastFunction = 0;
			for (scanFunction = 0; scanFunction <= lastFunction; scanFunction++)
			{
				space.s.deviceNum   = scanDevice; // deviceMap[scanDevice];
				space.s.functionNum = scanFunction;
	
				child = bridgeProbeChild(bridge, space);

				// look in function 0 for multi function flag
				if (child && 0 == scanFunction)
				{
					uint32_t flags = configRead32(bridge, kIOPCIConfigCacheLineSize, &space);
					if ((flags != 0xFFFFFFFF) && (0x00800000 & flags))
					{
						lastFunction = 7;
					}
				}

				if (child && 0 != scanFunction)
				{
                    isMFD = true;
				}

				if (lastFunction > 0 && child)
				{
					// Disable the MFD's UR reporting while scanning the bus, since it will
					// respond with UR status if software attempts to scan a non-existent function.
					maskUR(child, true);
				}
			}

			if (lastFunction > 0)
			{
				FOREACH_CHILD(bridge, child)
				{
					if (child->space.s.deviceNum == scanDevice)
					{
						maskUR(child, false);
					}

                    child->isMFD = isMFD;
				}
			}
		}

		maskUR(bridge, false);
	}
	if (fResetWaitTime > kIOPCIEnumerationWaitTime)
	{
		// if fResetWaitTime was set to > kIOPCIEnumerationWaitTime, reset it here to default value for the next bus scan
		fResetWaitTime = kIOPCIEnumerationWaitTime;
	}
}

//---------------------------------------------------------------------------

void CLASS::markChanged(IOPCIConfigEntry * entry)
{
    if (fChangedServices)
    {
        if (entry->dtNub && entry->dtNub->inPlane(gIOServicePlane))
            fChangedServices->setObject(entry->dtNub);
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

	bridge->deviceState |= kPCIDeviceStateChildChanged | kPCIDeviceStateChildAdded;
    if (child->rootPortEntry)
        child->rootPortEntry->deviceState |= kPCIDeviceStateDomainChanged;
}

//---------------------------------------------------------------------------

void CLASS::bridgeDeallocateChildRanges(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead)
{
	IOPCIRange *        range;
	IOPCIRange *        childRange;
    bool				ok;

    for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
	{
		childRange = dead->ranges[rangeIndex];
		if (!childRange) continue;

		// Free the secondary bus back to itself
		if ((kIOPCIRangeBridgeBusNumber == rangeIndex) && dead->busResv.nextSubRange)
		{
			ok = IOPCIRangeListDeallocateSubRange(childRange, &dead->busResv);
			if (!ok) panic("!IOPCIRangeListDeallocateSubRange busResv");
		}

		if (childRange->nextSubRange)
		{
			IOPCIScalar size  = childRange->size;
			IOPCIScalar start = childRange->start;

			range = bridgeGetRange(bridge, childRange->type);
			if (!range) panic("!range");
			ok = IOPCIRangeListDeallocateSubRange(range, childRange);
			if (!ok) panic("!IOPCIRangeListDeallocateSubRange");
			// Preserve start and size so BAR and bus number ranges can be reallocated
			if (rangeIndex <= kIOPCIRangeExpansionROM || rangeIndex == kIOPCIRangeBridgeBusNumber)
            {
                childRange->start = start;
                childRange->proposedSize = size;
            }
            else
            {
                childRange->proposedSize = 0;
            }
            bridge->haveAllocs |= (1 << childRange->type);
			dead->rangeBaseChanges |= (1 << rangeIndex);
		}

		IOPCIRangeFree(childRange);
		dead->ranges[rangeIndex] = NULL;
	}
}

//---------------------------------------------------------------------------

void CLASS::deleteConfigEntry(IOPCIConfigEntry *entry)
{
		IOPCIDevice *pciDevice;

		for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
		{
			if (entry->ranges[rangeIndex])
			{
				DLOG("Warning: leaking device " D()"'s range #%d\n", DEVICE_IDENT(entry), rangeIndex);
			}
		}

		if ((pciDevice = OSDynamicCast(IOPCIDevice, entry->dtNub)))
		{
			pciDevice->reserved->configEntry = NULL;
		}
		if (entry->isBridge)
			fBridgeCount--;
		else
			fDeviceCount--;

		if (   (entry->dtNub != NULL)
			&& (entry->dtNub->inPlane(gIOServicePlane) == false))
		{
			// detach from device tree plane before deleting the node, since
			// termination will never happen.
        	entry->dtNub->detachAbove(gIODTPlane);
		}

		DLOG("deleted %p, bridges %d devices %d\n", entry, fBridgeCount, fDeviceCount);
		IOFreeType(entry, IOPCIConfigEntry);
}

//---------------------------------------------------------------------------

void CLASS::bridgeMarkChildDead(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead)
{
    IOPCIConfigEntry *child;
    IOPCIConfigEntry *next;
    bool			  didKeep;

	dead->deviceState |= kPCIDeviceStateDead;

	FOREACH_CHILD_SAFE(dead, child, next)
	{
		bridgeMarkChildDead(dead, child);
	}

    DLOG("bridge " B() " marking child %p " D() " dead\n",
         BRIDGE_IDENT(bridge), dead, DEVICE_IDENT(dead));

	if (kPCIDeviceStateRequestPause & dead->deviceState)
	{
		dead->deviceState &= ~kPCIDeviceStateRequestPause;
		fWaitingPause--;
	}

	if (   (dead->dtNub != NULL)
		&& (dead->dtNub->inPlane(gIOServicePlane) == false))
	{
		// If dead isn't in the service plane, it won't terminate; remove it immediately.
		DLOG("bridge %p dead child at " D() " never entered service plane\n", bridge, DEVICE_IDENT(dead));

		bridgeRemoveChild(bridge, dead);
	}
}

void CLASS::bridgeRemoveChild(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead)
{
    IOPCIConfigEntry ** prev;
    IOPCIConfigEntry *  child;

	dead->deviceState |= kPCIDeviceStateDead;

    while ((child = dead->child))
    {
        bridgeRemoveChild(dead, child);
    }

    DLOG("bridge " B() " removing child %p " D() "\n",
         BRIDGE_IDENT(bridge), dead, DEVICE_IDENT(dead));

	if ((bridge == bridge->rootPortEntry) && !bridge->dtEntry->getProperty(kIOPCIExpressMaxPayloadSize))
	{
		// Root Port no longer has a connection. Reset Root Port MPS to its capability if there's no MPS override.
		bridge->expressMaxPayload = (configRead32(bridge, bridge->expressCapBlock + 0x04) & 7);
	}
	dead->rootPortEntry->deviceState |= kPCIDeviceStateDomainChanged;

	if (kPCIDeviceStateRequestPause & dead->deviceState)
	{
		dead->deviceState &= ~kPCIDeviceStateRequestPause;
		fWaitingPause--;
	}

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

    if (!child) panic("bridgeRemoveChild");

	bridge->deviceState |= kPCIDeviceStateChildChanged;
	bridge->deviceState &= ~(kPCIDeviceStateTotalled | kPCIDeviceStateAllocated);

	bridgeDeallocateChildRanges(bridge, dead);

	deleteConfigEntry(dead);
}

//---------------------------------------------------------------------------

void CLASS::bridgeMoveChildren(IOPCIConfigEntry * to, IOPCIConfigEntry * list, uint32_t moveTypes)
{
	IOPCIConfigEntry * dead;
	IOPCIRange *       range;
	IOPCIRange *       childRange;
	bool		       ok;

	while (list)
	{
		dead = list;
		list = list->peer;
		for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
		{
			if (!((1 << rangeIndex) & moveTypes))       continue;
			childRange = dead->ranges[rangeIndex];
			if (!childRange)							continue;
			if (!childRange->proposedSize)				continue;
			if (!childRange->start)						continue;

			range = bridgeGetRange(to, childRange->type);
			if (!range) panic("!range");

			// reserve the alloc until terminate
			if (!dead->parent)
			{
				// head Q
				dead->parent = to;
				dead->peer   = to->child;
				to->child    = dead;
				DLOG("Reserves on (" D() ") for (" D() "):\n",
						DEVICE_IDENT(to), DEVICE_IDENT(dead));
			}
			DLOG("  %s: 0x%llx reserve 0x%llx, 0x%llx\n",
					gPCIResourceTypeName[childRange->type], 
					childRange->start, childRange->proposedSize, childRange->totalSize);
            if (childRange->nextSubRange) panic("bridgeMoveChildren");
//			childRange->proposedSize = childRange->size;
			childRange->flags |= kIOPCIRangeFlagReserve;
			ok = IOPCIRangeListAllocateSubRange(range, childRange);
			if (!ok) panic("!IOPCIRangeListAllocateSubRange");
		}
	}
}

//---------------------------------------------------------------------------

void CLASS::bridgeDeadChild(IOPCIConfigEntry * bridge, IOPCIConfigEntry * dead)
{
	IOPCIConfigEntry * pendingList = NULL;

    if (kPCIDeviceStateDead & dead->deviceState)
        return;

	// Mark the hierarchy rooted at dead as, well, dead. This hierarchy retains
	// its allocations -- BAR, MEM, PFM, IO, and bus numbers -- until the
	// devices terminate. As the terminations proceed in bottom-up order,
	// allocations will be deallocated to their parent until they reach dead's
	// parent, bridge. If a device is not present in the service plane, free it
	// immediately.

    DLOG("bridge %p dead child at " D() "\n", bridge, DEVICE_IDENT(dead));
	markChanged(dead);

	bridgeMarkChildDead(bridge, dead);
}

//---------------------------------------------------------------------------

IOPCIConfigEntry* CLASS::bridgeProbeChild( IOPCIConfigEntry * bridge, IOPCIAddressSpace space )
{
    IOPCIConfigEntry * child = NULL;
    bool      ok = true;
    uint32_t  vendorProduct;

    vendorProduct = configRead32(bridge, kIOPCIConfigVendorID, &space);

    if ((kPCIStatic != (kPCIHPTypeMask & bridge->supportsHotPlug))
        && ((0 == (vendorProduct & 0xffff)) || (0xffff == (vendorProduct & 0xffff))))
    {
        configWrite32(bridge, kIOPCIConfigVendorID, 0, &space);
        vendorProduct = configRead32(bridge, kIOPCIConfigVendorID, &space);
    }

    for (child = bridge->child; child; child = child->peer)
    {
        if (kPCIDeviceStateDead & child->deviceState)
            continue;
        if (space.bits == child->space.bits)
        {
            DLOG("bridge %p scan existing child at " D() " state 0x%x\n",
                 bridge, DEVICE_IDENT(child), child->deviceState);

            // check bars?

			if (!(kIOPCIConfiguratorNoTerminate & fFlags))
			{
				if ((vendorProduct != child->vendorProduct)
				|| (kPCIDeviceStateEjected & child->deviceState)
				|| (kPCIDeviceStateToKill  & child->deviceState))
				{
					IOPCIConfigEntry * dead = child;
					if (!(kPCIDeviceStateEjected & child->deviceState))
					{
						// create new if present
						child = NULL;
					}
					// may free child
					bridgeDeadChild(bridge, dead);
				}
			}
            break;
        }
    }
    
    if (child)
        return child;

    uint32_t retries = 1;
    while ((0 == (vendorProduct & 0xffff)) || (0xffff == (vendorProduct & 0xffff)))
    {
        if (!--retries)
            return NULL;
        vendorProduct = configRead32(bridge, kIOPCIConfigVendorID, &space);
    }

    // Checking for VID/DID 0x0001/0xFFFF in case the RC implements CRS (sec 2.3.2)
    if (0xffff0001 == vendorProduct)
    {
       // Following conventional reset, wait up to 1s for the device to properly respond to
       // configuration requests (sec 6.6.1). If device fails to respond successfully, return NULL.
       uint64_t startTime = fResetStartTime;
       uint64_t timeout = 0;
       clock_interval_to_absolutetime_interval(fResetWaitTime, kMillisecondScale, &timeout);
       while (0xffff0001 == vendorProduct)
       {
           IOSleepWithLeeway(1, 1);
           if ((mach_absolute_time() - startTime) > timeout)
           {
               DLOG("Endpoint failed to respond successfully after spec mandated wait time\n");
               return NULL;
           }
           vendorProduct = configRead32(bridge, kIOPCIConfigVendorID, &space);
       }
    }

    // check if root MPS > 128B -> This means we have a CIO80 SoC connected to a USB4v2 capable switch
    if (!bridge->isHostBridge && (bridge->supportsHotPlug & kPCIHotPlugTunnel) && (bridge->rootPortEntry->expressMaxPayload > MIN_MPS)
		    && !(fStates & kIOPCIConfiguratorMPSOverride) && !(bridge->deviceState & (kPCIDeviceStateRequestPause | kPCIDeviceStatePaused)))
    {
	    // Check for ASM SATA controller devices. WA for rdar://134837798
	    switch (vendorProduct) {
		    case 0x06111b21:
		    case 0x06121b21:
		    case 0x06221b21:
		    case 0x06231b21:
		    case 0x06241b21:
		    case 0x06251b21:
			    IOLog("Work around to support ASM SATA controller 0x%08x\n", vendorProduct);
			    fStates |= kIOPCIConfiguratorMPSOverride;
			    FOREACH_CHILD(bridge->rootPortEntry, child)
			    {
				    child->deviceState |= kPCIDeviceStateRequestPause;
				    markChanged(child);
				    fWaitingPause++;
			    }
			    break;
		    default:
			    break;
	    }
    }

    child = IOMallocType(IOPCIConfigEntry);
    if (!child) return NULL;

    child->id              = ++fNextID;
    child->space           = space;
    child->hostBridge      = bridge->hostBridge;
    child->hostBridgeEntry = bridge->hostBridgeEntry;
    child->headerType      = configRead8(child, kIOPCIConfigHeaderType) & 0x7f;
    child->classCode       = configRead32(child, kIOPCIConfigRevisionID) >> 8;
    child->vendorProduct   = vendorProduct;

    if (bridge->isHostBridge)
    {
        child->rootPortEntry = child;
    }
    else
    {
        child->rootPortEntry = bridge->rootPortEntry;
        DLOG("Device " D() " has root port " D() "\n", DEVICE_IDENT(child), DEVICE_IDENT(child->rootPortEntry));
    }

    DLOG("Found type %u device class-code 0x%06x cmd 0x%04x at " D() " [state 0x%x]\n",
         child->headerType, child->classCode, configRead16(child, kIOPCIConfigCommand),
         DEVICE_IDENT(child),
         child->deviceState);

    switch (child->headerType)
    {
        case kPCIHeaderType0:
            break;

        case kPCIHeaderType1:
        case kPCIHeaderType2:
            child->isBridge     = true;
			// If PCI Device is a PCIe, then this value will get updated based on the Express Capablities Device/Port Type
			child->subDeviceNum = 0;
			child->endDeviceNum = 31;
            break;

        default:
            DLOG("  bad PCI header type 0x%x\n", child->headerType);
            ok = false;
            break;
    }

    if (!ok)
    {
        IOFreeType(child, IOPCIConfigEntry);
		return NULL;
    }

	if (findPCICapability(child, kIOPCIPCIExpressCapability, &child->expressCapBlock))
	{
		child->linkCaps = configRead32(child, child->expressCapBlock + 0x0c);
		if (child->isBridge)
		{
			uint32_t expressCaps, linkControl, slotCaps = kSlotCapHotplug;
		
			expressCaps = configRead16(child, child->expressCapBlock + 0x02);
			linkControl = configRead16(child, child->expressCapBlock + 0x10);

			if ((kPCIEPortTypeUpstreamPCIESwitch != (kPCIECapDevicePortType & expressCaps))
					&& (kPCIEPortTypePCIEtoPCIBridge != (kPCIECapDevicePortType & expressCaps)))
			{
				// Only Uptream Port and Root Complex require scanning all 32 device number
				// Root Complex entry is populated in addHostBridge()
				// All other Port Types only have device 0
				// PCI devices do not follow this particular PCIE requirement so exclude PCIE to PCI bridges as well
				child->subDeviceNum = child->endDeviceNum = 0;
			}

			if (kPCIECapSlotConnected & expressCaps)
			{
				slotCaps = configRead32(child, child->expressCapBlock + 0x14);
				child->commandCompleted = (kSlotCapNoCommandCompleted & slotCaps) == 0;
				child->powerController = (kSlotCapPowerController & slotCaps) != 0;
			}

			if ((0x60 == (0xf0 & expressCaps))      // downstream port
				 || (0x40 == (0xf0 & expressCaps))) // or root port
			{
				if ((kLinkCapDataLinkLayerActiveReportingCapable & child->linkCaps)
				 && (kSlotCapHotplug & slotCaps))
				{
					child->linkInterrupts = true;
				}
			}
	
			child->expressCaps        = expressCaps;
			DLOG("  expressCaps 0x%x, linkControl 0x%x, linkCaps 0x%x, slotCaps 0x%x\n",
				 child->expressCaps, linkControl, child->linkCaps, slotCaps);

			if ((kIOPCIConfiguratorFPBEnable & gIOPCIFlags)
			 && findPCICapability(child, kIOPCIFPBCapability, &child->fpbCapBlock))
			{
				child->fpbCaps = configRead32(child, child->fpbCapBlock + 0x04);
				if (1 & child->fpbCaps)
				{
					child->fpbUp   = (0x50 == (0xf0 & expressCaps));
					child->fpbDown = (bridge->fpbUp && (0x60 == (0xf0 & expressCaps)));
				}
				if (child->fpbDown)
				{
					child->subDeviceNum = child->endDeviceNum
					= child->space.s.deviceNum + (1 + (31 & (bridge->fpbCaps >> 3)));

					child->rangeBaseChanges = (1 << kIOPCIRangeBridgeBusNumber);
				}
				DLOG("  fpbCaps 0x%x, fpbUp %d, fpbDown %d subDevice %d\n",
					 child->fpbCaps, child->fpbUp, child->fpbDown, child->subDeviceNum);
			}
		}
		child->expressDeviceCaps1 = configRead32(child, child->expressCapBlock + 0x04);
		child->expressDeviceCaps2 = configRead32(child, child->expressCapBlock + 0x24);
		child->expressMaxPayload  = (child->expressDeviceCaps1 & 7);
		DLOG("  expressMaxPayload Capability 0x%x\n", child->expressMaxPayload);
		if (child != child->rootPortEntry)
		{
			if (bridge == child->rootPortEntry)
			{
				if (child->expressMaxPayload < child->rootPortEntry->expressMaxPayload)
				{
					DLOG("  root port's expressMaxPayload 0x%x -> 0x%x\n", child->rootPortEntry->expressMaxPayload, child->expressMaxPayload);
					child->rootPortEntry->expressMaxPayload = child->expressMaxPayload;
				}
			}
			if (child->expressMaxPayload > bridge->expressMaxPayload)
			{
				child->expressMaxPayload = bridge->expressMaxPayload;
			}
			DLOG("  expressMaxPayload Setting 0x%x\n", child->expressMaxPayload)
		}
	}

	if (kIOPCIConfiguratorAER & gIOPCIFlags)
	{
		findPCICapability(child, kIOPCIExpressErrorReportingCapability, &child->aerCapBlock);
	}

	bridgeAddChild(bridge, child);
	checkCacheLineSize(child);

	return child;
}

//---------------------------------------------------------------------------

void CLASS::bridgeProbeChildRanges( IOPCIConfigEntry * bridge, uint32_t resetMask )
{
	DLOG("bridge " D() " %s probe child ranges\n",
		 DEVICE_IDENT(bridge), resetMask ? "reset " : "");

	FOREACH_CHILD(bridge, child)
	{
        if (kPCIDeviceStateDeadOrHidden & child->deviceState) continue;

        DLOG("Probing type %u device class-code 0x%06x cmd 0x%04x at " D() " [state 0x%x]\n",
             child->headerType, child->classCode, configRead16(child, kIOPCIConfigCommand),
             DEVICE_IDENT(child),
             child->deviceState);

        if (kPCIDeviceStateRangesProbed & child->deviceState) continue;
        child->deviceState |= kPCIDeviceStateRangesProbed;

	    switch (child->headerType)
	    {
	        case kPCIHeaderType0:
	            // skip devices aliased to host bridges
	            if ((child->classCode & 0xFFFFFF) == 0x060000)
	                break;

	            deviceProbeRanges(child, resetMask);
	            break;

	        case kPCIHeaderType1:
	            bridgeProbeRanges(child, resetMask);
	            break;

	        case kPCIHeaderType2:
	            cardbusProbeRanges(child, resetMask);
	            break;
	    }
	}
}

//---------------------------------------------------------------------------
uint32_t CLASS::findPCICapability(IORegistryEntry* from, IOPCIAddressSpace space,
                           uint32_t capabilityID, uint32_t * found)
{
    return(findPCICapability(findEntry(from, space), capabilityID, found));
}

uint32_t CLASS::findPCICapability(IOPCIConfigEntry * device,
                                  uint32_t capabilityID, uint32_t * found)
{
    uint32_t data;
    uint32_t offset, expressCap, firstOffset;

    data = expressCap = firstOffset = 0;
    if (found)
    {
        firstOffset = *found;
        *found = 0;
    }

    if (0 == ((kIOPCIStatusCapabilities << 16)
              & (configRead32(device, kIOPCIConfigCommand, &device->space))))
        return (0);

    if (capabilityID >= 0x100)
    {
        if (!findPCICapability(device, kIOPCIPCIExpressCapability, &expressCap)) return (0);

        capabilityID = -capabilityID;
        offset = 0x100;
        while (offset)
        {
            data = configRead32(device, offset, &device->space);
            if (capabilityID == (data & 0xffff))
            {
                if (!firstOffset)
                {
                    if (found)
                        *found = offset;
                    break;
                }
                if (offset == firstOffset) firstOffset = 0;
            }
            offset = (data >> 20) & 0xfff;
            if ((offset < 0x100) || (offset & 3))
                offset = 0;
        }
    }
    else
    {
        offset = (0xff & configRead32(device, kIOPCIConfigCapabilitiesPtr, &device->space));
        if (offset & 3)
            offset = 0;
        while (offset)
        {
            data = configRead32(device, offset, &device->space);
            if (capabilityID == (data & 0xff))
            {
                if (!firstOffset)
                {
                    if (found)
                        *found = offset;
                    break;
                }
                if (offset == firstOffset) firstOffset = 0;
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
    uint32_t           resetMask;
};

void CLASS::safeProbeCallback( void * refcon )
{
    BARProbeParam * param = (BARProbeParam *) refcon;
    assert(param);

    if (cpu_number() == 0)
    {
        param->target->probeBaseAddressRegister(
            param->device, param->lastBarNum, param->resetMask );
    }
    if (cpu_number() == 99)
    {
        IOLog("safeProbeCallback() gcc workaround\n");
    }
}

void CLASS::safeProbeBaseAddressRegister(IOPCIConfigEntry * device, 
										 uint32_t lastBarNum, uint32_t resetMask, bool disableInterrupts)
{
    uint32_t barNum;
    lastBarNum = min(lastBarNum, kIOPCIRangeExpansionROM);

    for (barNum = 0; barNum <= lastBarNum; barNum++)
    {
        device->ranges[barNum] = IOPCIRangeAlloc();
        IOPCIRangeInit(device->ranges[barNum], 0, 0, 0);
    }

    {
#if NO_RENDEZVOUS_KERNEL
        if (disableInterrupts)
        {
            boolean_t       istate;
            istate = ml_set_interrupts_enabled(FALSE);
            probeBaseAddressRegister(device, lastBarNum, resetMask);
            ml_set_interrupts_enabled(istate);
        }
        else
        {
            probeBaseAddressRegister(device, lastBarNum, resetMask);
        }
#else
        BARProbeParam param;

        param.target     = this;
        param.device     = device;
        param.lastBarNum = lastBarNum;
        param.resetMask  = resetMask;

        if (disableInterrupts)
        {
            mp_rendezvous_no_intrs(&safeProbeCallback, &param);
        }
        else
        {
            mp_rendezvous(NULL, &safeProbeCallback, NULL, &param);
        }
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

//---------------------------------------------------------------------------

void CLASS::probeBaseAddressRegister(IOPCIConfigEntry * device, uint32_t lastBarNum, uint32_t resetMask)
{
    IOPCIRange *    range;
    uint32_t        barNum, nextBarNum;
    uint32_t        value;
    uint64_t        value64, saved, upper, barMask;
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
        configWrite32(device, barOffset, static_cast<uint32_t>(saved));

        // unimplemented BARs are hardwired to zero
        if (value == 0) continue;
		if (3 != (device->classCode >> 16)) continue;

        range = device->ranges[kIOPCIRangeExpansionROM];
        start = (resetMask & (1 << kIOPCIResourceTypeMemory)) ? 0 : (saved & ~barMask);
        value &= ~barMask;
        size  = -value;
        IOPCIRangeInit(range, kIOPCIResourceTypeMemory, start, size, size);
    }
    while (false);

    for (barNum = 0; barNum <= lastBarNum; barNum = nextBarNum)
    {
        barOffset  = kIOPCIConfigBaseAddress0 + barNum * 4;
        nextBarNum = barNum + 1;
        value64    = (-1ULL << 32);

        saved = configRead32(device, barOffset);
        configWrite32(device, barOffset, 0xFFFFFFFF);
        value = configRead32(device, barOffset);
        configWrite32(device, barOffset, static_cast<uint32_t>(saved));

        // unimplemented BARs are hardwired to zero
        if (value == 0) continue;

        clean64 = false;
        if (value & 1)
        {
            barMask = 0x3;
            type = kIOPCIResourceTypeIO;

            // If the upper 16 bits for I/O space
            // are all 0, then we should ignore them.
            if ((value & 0xFFFF0000) == 0) value |= 0xFFFF0000;
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
					type = kIOPCIResourceTypeMemory;
                    break;

                case 4: /* 64-bit mem */
                    clean64 = ((kIOPCIResourceTypePrefetchMemory == type) || (0 == device->space.s.busNum));
                    if (!clean64) configWrite32(device, barOffset + 4, 0);
                    else
                    {
                        upper = configRead32(device, barOffset + 4);
                        saved |= (upper << 32);
                        configWrite32(device, barOffset + 4, 0xFFFFFFFF);
                        value64 = configRead32(device, barOffset + 4);
						value64 <<= 32;
                        configWrite32(device, barOffset + 4, static_cast<uint32_t>(upper));
                    }
                    nextBarNum = barNum + 2;
                    break;
            }
        }

        start    = (resetMask & (1 << type)) ? 0 : (saved & ~barMask);
        value   &= ~barMask;
		value64 |= value;
        size     = -value64;

        if (size > MAX_BAR_SIZE)
        {
            size &= 0xFFFFFFFF;
            clean64 = false;
            nextBarNum--;
        }
        if (start == value64) 
        {
            DLOG("  [0x%x] can't probe\n", barOffset);
            continue;
        }

        range = device->ranges[barNum];
        IOPCIRangeInit(range, type, start, size, size);
        range->minAddress = minBARAddressDefault[type];
        if (clean64)
        {
            range->flags |= kIOPCIRangeFlagBar64;
            if (kIOPCIConfiguratorPFM64 & fFlags) range->maxAddress = 0xFFFFFFFFFFFFFFFFULL;
        }
#if 0
		if ((0x91821b4b == configRead32(device->space, kIOPCIConfigVendorID))
		 && (0x18 == barOffset))
		{
			range->proposedSize = range->totalSize = 0x4000;
		}
#endif
    }

    restoreAccess(device, command);
}

//---------------------------------------------------------------------------

void CLASS::deviceProbeRanges( IOPCIConfigEntry * device, uint32_t resetMask )
{
    uint32_t     idx;
    IOPCIRange * range;

    // Probe BAR 0 through 5 and ROM
    safeProbeBaseAddressRegister(device, kIOPCIRangeExpansionROM, resetMask, PROBE_DISABLE_INTERRUPTS);

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

void CLASS::bridgeProbeBusRange(IOPCIConfigEntry * bridge, uint32_t resetMask)
{
    IOPCIRange *    range;
    uint64_t        start, size;

    // Record the bridge secondary and subordinate bus numbers
	if (resetMask & (1 << kIOPCIResourceTypeBusNumber))
	{
		bridge->secBusNum = 0;
		bridge->subBusNum = 0;
	}
	else
	{
		bridge->secBusNum = configRead8(bridge, kPCI2PCISecondaryBus);
		bridge->subBusNum = configRead8(bridge, kPCI2PCISubordinateBus);
	}

    range = IOPCIRangeAlloc();
    start = bridge->secBusNum;
    size  = bridge->subBusNum - bridge->secBusNum + (bridge->fpbDown ? 0 : 1);
    IOPCIRangeInit(range, kIOPCIResourceTypeBusNumber, start, size, kPCIBridgeBusNumberAlignment);
    bridge->ranges[kIOPCIRangeBridgeBusNumber] = range;
}

//---------------------------------------------------------------------------

void CLASS::bridgeProbeRanges( IOPCIConfigEntry * bridge, uint32_t resetMask )
{
    IOPCIRange *    range;
    IOPCIScalar     start, end, upper, size;

    bridgeProbeBusRange(bridge, resetMask);

    // Probe bridge BAR0 and BAR1
	safeProbeBaseAddressRegister(bridge, kIOPCIRangeBAR1, resetMask, PROBE_DISABLE_INTERRUPTS);

#if 0
	// test bridge BARs
	if ((5 == bridge->space.s.busNum) && (0 == bridge->space.s.deviceNum) && (0 == bridge->space.s.functionNum))
	{
		for (int barNum = 0; barNum <= kIOPCIRangeBAR1; barNum++)
		{
			bridge->ranges[barNum] = IOPCIRangeAlloc();
			IOPCIRangeInit(bridge->ranges[barNum], kIOPCIResourceTypeMemory, 0, 0x40000, 0x40000);
		}
	}
#endif

	DLOG_RANGE("  bridge BAR0", bridge->ranges[kIOPCIRangeBAR0]);
	DLOG_RANGE("  bridge BAR1", bridge->ranges[kIOPCIRangeBAR1]);

    // Probe memory base and limit

    end = configRead32(bridge, kPCI2PCIMemoryRange);

    start = (end & 0xfff0) << 16;
    end  |= 0x000fffff;
    if (start && (end > start))
        size = end - start + 1;
    else
        size = start = 0;
	if (resetMask & (1 << kIOPCIResourceTypeMemory)) start = 0;

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
		if (resetMask & (1 << kIOPCIResourceTypePrefetchMemory)) start = 0;

        range = IOPCIRangeAlloc();
        IOPCIRangeInit(range, kIOPCIResourceTypePrefetchMemory, start, size,
                        kPCIBridgeMemoryAlignment);
		if (bridge->clean64 && (kIOPCIConfiguratorPFM64 & fFlags))
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
		if (resetMask & (1 << kIOPCIResourceTypeIO)) start = 0;

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

void CLASS::cardbusProbeRanges(IOPCIConfigEntry * bridge, uint32_t resetMask)
{
    IOPCIRange * range;

    bridgeProbeBusRange(bridge, resetMask);

    // Maximal bus range.

    range = bridge->ranges[kIOPCIRangeBridgeBusNumber];
    range->flags    |= kIOPCIRangeFlagNoCollapse | kIOPCIRangeFlagPermanent;

    // 4K register space

    range = IOPCIRangeAlloc();
    IOPCIRangeInit(range, kIOPCIResourceTypeMemory, 0, 4096, 4096);
    bridge->ranges[kIOPCIRangeBAR0] = range;

    // Maximal memory and I/O range.

    range = IOPCIRangeAlloc();
    IOPCIRangeInit(range, kIOPCIResourceTypeIO, 0, kPCIBridgeIOAlignment, kPCIBridgeIOAlignment);
    range->flags     = kIOPCIRangeFlagNoCollapse | kIOPCIRangeFlagPermanent;
    bridge->ranges[kIOPCIRangeBridgeIO] = range;

    range = IOPCIRangeAlloc();
    IOPCIRangeInit(range, kIOPCIResourceTypeMemory, 0, kPCIBridgeMemoryAlignment, kPCIBridgeMemoryAlignment);
    range->flags     = kIOPCIRangeFlagNoCollapse | kIOPCIRangeFlagPermanent;
    bridge->ranges[kIOPCIRangeBridgeMemory] = range;
}

//---------------------------------------------------------------------------
// ASM workaround rdar://93129225 or rdar://102254456

void CLASS::bridgeRetrainMask(IOPCIConfigEntry * bridge)
{
	OSNumber*          retrainNumber;
	if (((bridge->expressCaps & 0xf0) == 0x50) && bridge->dtEntry
			&& (retrainNumber = OSDynamicCast(OSNumber, bridge->dtEntry->getProperty(kIOPCIRetrainLinkMaskKey))))
	{
		// retrainMask is bitmask for the device number below the bridge
		uint32_t retrainMask = retrainNumber->unsigned32BitValue();
		uint16_t linkStatus, linkControl2 = 0;
		bool retrained = false;

		FOREACH_CHILD(bridge, child)
		{
			// Check if device requires retrain and only retrain if there's an endpoint present on the child bridge
			if ((retrainMask & (1 << child->space.s.deviceNum)) && (endpointPresent(child) == true))
			{
				// this workaround is only applicable if the current link speed != target link speed
				linkStatus = configRead16(child, child->expressCapBlock + 0x12);
				linkControl2 = configRead16(child, child->expressCapBlock + 0x30);
				if ((linkStatus & 0x0f) == (linkControl2 & 0x0f))
				{
					continue;
				}
				if (retrainLink(child) == kIOReturnSuccess)
				{
					retrained = true;
					// the design of this retrain workaround requires the bridge to be on the device tree.
					// if this becomes a workaround needed for downstream devices whose bridge is not on the device tree,
					// we'll have to rethink this
					if (child->dtEntry)
					{
						child->dtEntry->setProperty(kIOPCIRetrainLinkKey, kOSBooleanTrue);
					}
				}
			}
		}

		if (retrained)
		{
			// these variables are used for all devices during scanning (i.e., wait for link up, CRS wait).
			// We decided that for the sake of simple implementation, it is okay for other devices
			// to use this timestamp instead of keeping 2 sets of timestamp for very little benefits.
			fResetStartTime = mach_absolute_time();
			// we reset wait time to 1000ms (1s) for link retrain
			fResetWaitTime = kIOPCIWaitForLinkUpTime;
		}
	}
}

//---------------------------------------------------------------------------

int32_t CLASS::scanProc(void * ref, IOPCIConfigEntry * bridge)
{
    int32_t            ok = true;
    bool			   bootScan = (NULL != ref);
    bool 			   haveBus;
	uint32_t           resetMask = 0;

    if (kPCIDeviceStateDeadOrHidden & bridge->deviceState) return (ok);

    if (!(kPCIDeviceStateScanned & bridge->deviceState))
    {
		haveBus = (bridge->fpbDown
			|| (((bridge->secBusNum || bridge->isHostBridge)
			    && ((bootScan && bridge->ranges[kIOPCIRangeBridgeBusNumber]->proposedSize)
				   || ((!bootScan) && bridge->ranges[kIOPCIRangeBridgeBusNumber]->size)))));
		if (haveBus)
		{
			DLOG("scan %s" B() "\n", bootScan ? "(boot) " : "", BRIDGE_IDENT(bridge));
			if (kPCIStatic != (kPCIHPTypeMask & bridge->supportsHotPlug))
			{
				resetMask = ((1 << kIOPCIResourceTypeMemory)
						   | (1 << kIOPCIResourceTypePrefetchMemory)
						   | (1 << kIOPCIResourceTypeIO)
						   | (1 << kIOPCIResourceTypeBusNumber));
			}
			bridgeScanBus(bridge, (bridge->fpbDown ? bridge->space.s.busNum : bridge->secBusNum));
			bridge->deviceState |= kPCIDeviceStateScanned;
			if (kPCIDeviceStateChildChanged & bridge->deviceState) 
			{
				DLOG("bridge " B() " child change\n", BRIDGE_IDENT(bridge));
				bridge->deviceState &= ~(kPCIDeviceStateTotalled | kPCIDeviceStateAllocated 
										| kPCIDeviceStateAllocatedBus);
			}
		}

        // associate bootrom devices
        bridgeConnectDeviceTree(bridge);
        bridgeMPSOverride(bridge);
        // scan ranges
        if (haveBus) bridgeProbeChildRanges(bridge, resetMask);
        bridgeFinishProbe(bridge);
        // link retrain workaround check
        bridgeRetrainMask(bridge);
    }
    if (!bootScan)
	{
		if (!(kPCIDeviceStateAllocatedBus & bridge->deviceState))
		{
			FOREACH_CHILD(bridge, child) child->deviceState &= ~kPCIDeviceStateAllocatedBus;
			bridge->deviceState &= ~kPCIDeviceStateTotalled;
			ok = bridgeTotalResources(bridge, (1 << kIOPCIResourceTypeBusNumber));
			if (ok)
			{
				ok = bridgeAllocateResources(bridge, (1 << kIOPCIResourceTypeBusNumber));
				DLOG("bus alloc done (bridge " B() ", state 0x%x, ok %d)\n",
						BRIDGE_IDENT(bridge), bridge->deviceState, ok);
			}
			if (ok > 0)	bridge->deviceState |= kPCIDeviceStateAllocatedBus;
			else        bridge->parent->deviceState &= ~kPCIDeviceStateAllocatedBus;
		}
	}
    // return -1 if we need MPSOverride, ignore if we're in paused state
    if ((fStates & kIOPCIConfiguratorMPSOverride) && !(fStates & kIOPCIConfiguratorMPSOverridePause))
    {
	    ok = -1;
    }
    return (ok);
}

//---------------------------------------------------------------------------

int32_t CLASS::bootResetProc(void * ref, IOPCIConfigEntry * bridge)
{
    bool     ok = true;
    uint32_t reg32, reserveSize;

	if ((kPCIStatic != (kPCIHPTypeMask & bridge->supportsHotPlug))
	 && bridge->ranges[kIOPCIRangeBridgeBusNumber]
	 && !bridge->ranges[kIOPCIRangeBridgeBusNumber]->nextSubRange)
	{
		reserveSize = (bridge->fpbDown ? 0 : 1);
        bridge->ranges[kIOPCIRangeBridgeBusNumber]->start        = 0;
        bridge->ranges[kIOPCIRangeBridgeBusNumber]->size         = 0;
        bridge->ranges[kIOPCIRangeBridgeBusNumber]->totalSize    = reserveSize;
        bridge->ranges[kIOPCIRangeBridgeBusNumber]->proposedSize = reserveSize;
		if (kPCIHotPlugTunnelRootParent != bridge->supportsHotPlug)
		{
			DLOG("boot reset " B() "\n", BRIDGE_IDENT(bridge));
			reg32 = configRead32(bridge, kPCI2PCIPrimaryBus);
			reg32 &= ~0x00ffffff;
			configWrite32(bridge, kPCI2PCIPrimaryBus, reg32);
	    }
    }
    return (ok);
}

//---------------------------------------------------------------------------

int32_t CLASS::totalProc(void * ref, IOPCIConfigEntry * bridge)
{
    if (   (kPCIDeviceStateAllocatedBus & bridge->deviceState)
        && !(kPCIDeviceStateTotalled & bridge->deviceState))
    {
        bool ok = bridgeTotalResources(bridge,
                                  (1 << kIOPCIResourceTypeMemory)
                                | (1 << kIOPCIResourceTypePrefetchMemory)
                                | (1 << kIOPCIResourceTypeIO));

        if (!ok) bridge->parent->deviceState &= ~kPCIDeviceStateAllocated;
        bridge->deviceState |= kPCIDeviceStateTotalled;
    }
    return (true);
}

//---------------------------------------------------------------------------

int32_t CLASS::allocateProc(void * ref, IOPCIConfigEntry * bridge)
{
    int32_t ok = true;

    if (!(kPCIDeviceStateAllocatedBus & bridge->deviceState)) return (ok);
    if (kPCIDeviceStateAllocated & bridge->deviceState)       return (ok);

	FOREACH_CHILD(bridge, child) child->deviceState &= ~kPCIDeviceStateAllocated;

	ok = bridgeAllocateResources(bridge, 
					  (1 << kIOPCIResourceTypeMemory)
					| (1 << kIOPCIResourceTypePrefetchMemory)
					| (1 << kIOPCIResourceTypeIO));
	DLOG("alloc done (bridge " B() ", state 0x%x, ok %d)\n",
			BRIDGE_IDENT(bridge), bridge->deviceState, ok);

	if (ok > 0) bridge->deviceState |= kPCIDeviceStateAllocated;
	else        bridge->parent->deviceState &= ~kPCIDeviceStateAllocated;

    return (ok);
}

//---------------------------------------------------------------------------

void CLASS::doConfigure(uint32_t options)
{
    bool bootConfig = (kIOPCIConfiguratorBoot & options);

    if (bootConfig) iterate("boot reset", &CLASS::scanProc,                 &CLASS::bootResetProc,            this);
					iterate("scan total", &CLASS::scanProc,                 &CLASS::totalProc,                NULL);
					iterate("allocate",   &CLASS::allocateProc,             NULL,                             NULL);
					iterate("finalize",   &CLASS::bridgeFinalizeConfigProc, &CLASS::domainFinalizeConfigProc, NULL);
}

//---------------------------------------------------------------------------

enum 
{
    kIteratorNew       = 0,
    kIteratorCheck     = 1,
    kIteratorDidCheck  = 2,
    kIteratorDoneCheck = 3,
};

void CLASS::iterate(const char * what, IterateProc topProc, IterateProc bottomProc, void * ref)
{
    IOPCIConfigEntry * device;
    IOPCIConfigEntry * parent;
    int32_t			   ok;
	uint32_t           revisits;
    bool               didCheck;

    device = fRoot;
    device->iterator = kIteratorCheck;
    revisits = 0;

    DLOG("iterate %s: start\n", what);
    do
    {
        parent = device->parent;
//        DLOG("iterate(bridge " B() ", state 0x%x, parent 0x%x)\n", BRIDGE_IDENT(device), device->iterator, parent ? parent->iterator : 0);
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

		if (!ok)
		{
			if (++revisits > (8 * fBridgeCount))
			{
				DLOG("iterate %s: HUNG?\n", what);
				ok = -1;
			}
		}

		if (ok < 0) break;

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
                if (bottomProc)
                {
                    (void) (this->*bottomProc)(ref, device);
                }
                device = parent;
            }
        }
    }
    while (device);
    DLOG("iterate %s: end(%d)\n", what, revisits);
}

//---------------------------------------------------------------------------

void CLASS::configure(uint32_t options)
{
    bool bootConfig = (kIOPCIConfiguratorBoot & options);

    if (bootConfig) IOLog("[ PCI configuration begin ]\n");

    // An Endpoint may return CRS after a Reset which requires up to 1s wait time
    // for Successful response. However it is arguable that boot is a form of reset.
    // rdar://106326237 shows that the Endpoint responded with CRS on boot scan
    fResetStartTime = mach_absolute_time();
    // Time scale is millisecond
    // Per PCIE Base spec v5.0, PCIE device must link train, initialize, and return
    // successful completion to valid Config Request 1s after reset exit
    // So we're supposed to wait 900ms since we waited 100ms after link up
    fResetWaitTime = kIOPCIEnumerationWaitTime;
	IOLog("[IOPCIConfigurator::%s()] PCI configuration %s\n", __func__, fRoot->child->dtNub->getName());

    PE_Video	       consoleInfo;

	fFlags |= options;

#if defined(__i386__) || defined(__x86_64__)
    if (bootConfig)
    {
        IOService::getPlatform()->getConsoleInfo(&consoleInfo);

        if (!fPFMConsole)
        {
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
#endif

    doConfigure(options);

#if defined(__i386__) || defined(__x86_64__)
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
#endif

	fFlags &= ~options;

    fResetStartTime = 0;
    fResetWaitTime = 0;
    if (bootConfig) IOLog("[ PCI configuration end, bridges %d, devices %d ]\n", fBridgeCount, fDeviceCount);
}

//---------------------------------------------------------------------------

IOPCIRange * CLASS::bridgeGetRange(IOPCIConfigEntry * bridge, uint32_t type)
{
    IOPCIRange * range;

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

void CLASS::logAllocatorRange(IOPCIConfigEntry * device, IOPCIRange * range, char c)
{
    DLOG("  %s: 0x%llx:0x%llx,0x%llx-0x%llx,0x%llx:0x%llx (at " D() ") %s%s%s%s%s%s%c",
            gPCIResourceTypeName[range->type], 
            range->start, 
            range->size, range->proposedSize,
            range->totalSize, range->extendSize, 
            range->alignment,
            DEVICE_IDENT(device),
            (NULL != range->nextSubRange)                ? "A" : "a",
            (kIOPCIRangeFlagRelocatable  & range->flags) ? "R" : "r",
            (kIOPCIRangeFlagSplay        & range->flags) ? "S" : "s",
            (kIOPCIRangeFlagMaximizeSize & range->flags) ? "M" : "m",
            (kIOPCIRangeFlagMaximizeRoot & range->flags) ? "B" : "b",
            (kIOPCIRangeFlagReserve      & range->flags) ? "V" : "v",
            c);
}

//---------------------------------------------------------------------------
bool CLASS::treeInState(IOPCIConfigEntry * entry, uint32_t state, uint32_t mask)
{
	for (; entry; entry = entry->parent)
	{
		if (state == (mask & entry->deviceState)) break;
	}
    return (NULL != entry);
}

static bool IOPCIRangeAppendRangeByAlignment(IOPCIRange ** list, IOPCIRange * newRange)
{
    IOPCIRange ** prev;
    IOPCIRange *  range;

    prev = list;
    do
    {
        range = *prev;
        if (!range)												          break;
		if (newRange->alignment > range->alignment)  					  break;
		else if (newRange->alignment < range->alignment)  			      continue;
    }
    while (prev = &range->nextToAllocate, true);

    *prev = newRange;
    newRange->nextToAllocate = range;
    
    return (true);
}

static uint32_t IOPCIRangeStateOrder(IOPCIRange * range)
{
	uint32_t order = 0;

	// order 1st allocated, placed, nonresize or shrink, resize, maximise

	if (range->nextSubRange) 											 order |= (1 << 31);
	if (range->start) 													 order |= (1 << 30);
	if (!((kIOPCIRangeFlagMaximizeRoot | kIOPCIRangeFlagMaximizeSize | kIOPCIRangeFlagSplay) 
		& range->flags)) 												 order |= (1 << 29);
	if (/*range->nextSubRange &&*/ (range->proposedSize <= range->size)) order |= (1 << 28);

	return (order);
}

bool IOPCIRangeAppendSubRange(IOPCIRange ** list, IOPCIRange * newRange)
{
    IOPCIRange ** prev;
    IOPCIRange *  range;
	uint32_t      newOrder, oldOrder;

    newOrder = IOPCIRangeStateOrder(newRange);
    prev = list;
    do
    {
        range = *prev;
        if (!range)												          break;
		oldOrder = IOPCIRangeStateOrder(range);
		if (newOrder > oldOrder) 									      break;
		else if (newOrder < oldOrder) 									  continue;
		if (newRange->alignment > range->alignment)  					  break;
		else if (newRange->alignment < range->alignment)  			      continue;
		if (newRange->proposedSize >= range->proposedSize)  			  break;
		else if (newRange->proposedSize < range->proposedSize)  	      continue;
    }
    while (prev = &range->nextToAllocate, true);

    *prev = newRange;
    newRange->nextToAllocate = range;
    
    return (true);
}

//---------------------------------------------------------------------------

bool CLASS::bridgeTotalResources(IOPCIConfigEntry * bridge, uint32_t typeMask)
{
	IOPCIConfigEntry * child;
	IOPCIRange *       range;
    IOPCIRange *       childRange;
    uint32_t           type;
	bool		       ok = true;

	IOPCIRange *       ranges[kIOPCIResourceTypeCount];
    IOPCIScalar		   totalSize[kIOPCIResourceTypeCount];
    IOPCIScalar        maxAlignment[kIOPCIResourceTypeCount];
    IOPCIScalar        minAddress[kIOPCIResourceTypeCount];
    IOPCIScalar        maxAddress[kIOPCIResourceTypeCount];
    IOPCIScalar        countMaximize[kIOPCIResourceTypeCount];

	if (bridge == fRoot) 							        return (ok);
    if (kPCIDeviceStateDeadOrHidden & bridge->deviceState)  return (ok);

    DLOG("bridgeTotalResources(bridge " B() ", iter 0x%x, state 0x%x, type 0x%x)\n",
            BRIDGE_IDENT(bridge), bridge->iterator, bridge->deviceState, typeMask);

    bzero(&ranges[0], sizeof(ranges));
    bzero(&totalSize[0], sizeof(totalSize));
    if ((bridge != fRoot) && !bridge->fpbDown) totalSize[kIOPCIResourceTypeBusNumber] = 1;

    bcopy(&minBridgeAlignments[0], &maxAlignment[0], sizeof(maxAlignment));

    bzero(&minAddress[0], sizeof(minAddress));
    bcopy(&maxBridgeAddressDefault[0], &maxAddress[0], sizeof(maxAddress));
    if (bridge->clean64 && (kIOPCIConfiguratorPFM64 & fFlags))
        maxAddress[kIOPCIResourceTypePrefetchMemory] = 0xFFFFFFFFFFFFFFFFULL;
    bzero(&countMaximize[0], sizeof(countMaximize));

	for (child = bridge->child; child; child = child->peer)
    {
		if (kPCIDeviceStateHidden & child->deviceState)				continue;

        for (int i = 0; i < kIOPCIRangeCount; i++)
        {
            childRange = child->ranges[i];
            if (!childRange)										continue;
            if (!((1 << childRange->type) & typeMask))				continue;
            range = bridgeGetRange(bridge, childRange->type);
            if (!range)                                             continue;
            type = range->type;
            if (kIOPCIRangeFlagMaximizeSize & childRange->flags)    countMaximize[type]++;
            if (!(childRange->totalSize + childRange->extendSize))  continue;

            logAllocatorRange(child, childRange, '\n');

			IOPCIRangeAppendRangeByAlignment(&ranges[type], childRange);

            if (childRange->alignment > maxAlignment[type])
                maxAlignment[type] = childRange->alignment;

            if (childRange->minAddress < minAddress[type])
                minAddress[type] = childRange->minAddress;
            if (childRange->maxAddress < maxAddress[type])
            {
                DLOG("  %s: maxAddr change 0x%llx -> 0x%llx (at " D() ")\n",
                        gPCIResourceTypeName[type], 
                        maxAddress[type], childRange->maxAddress, DEVICE_IDENT(child));
                maxAddress[type] = childRange->maxAddress;
			}
        }
    }

    for (type = 0; type < kIOPCIResourceTypeCount; type++)
    {
        if (!((1 << type) & typeMask)) continue;
        if (bridge->isHostBridge)      continue;
        range = bridgeGetRange(bridge, type);
        if (range) do
        {
			totalSize[type] += IOPCIRangeListSize(ranges[type]);

			if (((kIOPCIRangeFlagMaximizeSize | kIOPCIRangeFlagMaximizeRoot) & range->flags)
			 && !totalSize[type])            totalSize[type] = minBridgeAlignments[type];
            totalSize[type] = IOPCIScalarAlign(totalSize[type], minBridgeAlignments[type]);

			if (!(kIOPCIRangeFlagPermanent & range->flags)
			 && ((kIOPCIResourceTypeBusNumber != type)
			  || (totalSize[type] != range->totalSize)))
			{
				DLOG("  %s: 0x%llx: size change 0x%llx -> 0x%llx\n",
					  gPCIResourceTypeName[type], 
					  range->start, range->proposedSize, totalSize[type]);
				range->totalSize = totalSize[type];
				if (kIOPCIRangeFlagMaximizeRoot & range->flags) {}
				else
				{
					bridge->rangeSizeChanges    |= (1 << BRN(type));
					bridge->rangeRequestChanges |= (1 << type);
					ok = false;
				}
			}
			if (!((kIOPCIRangeFlagMaximizeSize | kIOPCIRangeFlagMaximizeRoot) & range->flags))
			{
				range->alignment = maxAlignment[type];
			}
			range->minAddress = minAddress[type];
			range->maxAddress = maxAddress[type];

			DLOG("  %s: total child reqs 0x%llx of 0x%llx maxalign 0x%llx\n",
					gPCIResourceTypeName[type], 
					range->totalSize, range->size, range->alignment);
        }
		while (false);
    }

	return (ok);
}


int32_t CLASS::bridgeAllocateResources(IOPCIConfigEntry * bridge, uint32_t typeMask)
{
    IOPCIRange * requests[kIOPCIResourceTypeCount];
    IOPCIScalar  shortage[kIOPCIResourceTypeCount];
	IOPCIScalar  shortageAlignments[kIOPCIResourceTypeCount];
    IOPCIRange * range;
    IOPCIRange * childRange;
    IOPCIScalar  shrink;
    uint32_t     type;
    uint32_t     haveAllocs = 0;
    uint32_t     haveRelocs = 0;
    uint32_t     doOptimize = 0;
    uint32_t     failTypes  = 0;
    int32_t      result;
    bool         expressCards = false;
    bool         ok;
	bool         canRelocate;

    DLOG("bridgeAllocateResources(bridge " B() ", state 0x%x, type 0x%x)\n",
            BRIDGE_IDENT(bridge), bridge->deviceState, typeMask);

    if (bridge == fRoot)                                   return (true);
    if (kPCIDeviceStateDeadOrHidden & bridge->deviceState) return (true);

	if (treeInState(bridge, kPCIDeviceStateRequestPause, 
		(kPCIDeviceStateRequestPause | kPCIDeviceStatePaused))) return (-1);

    bzero(requests, sizeof(requests));
    bzero(shortage, sizeof(shortage));
    bzero(shortageAlignments, sizeof(shortageAlignments));

    haveAllocs = bridge->haveAllocs;
    bridge->haveAllocs = 0;

    // determine kIOPCIRangeFlagRelocatable
    FOREACH_CHILD(bridge, child)
    {
		if (kPCIDeviceStateHidden & child->deviceState)				    continue;

		// Clear relocatable flag from static children e.g. if they were previously splayed
        for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
        {
            childRange = child->ranges[rangeIndex];
            if (!childRange)                           continue;
            if (!((1 << childRange->type) & typeMask)) continue;

            if (kPCIStatic == (kPCIHPTypeMask & child->supportsHotPlug))
            {
                childRange->flags &= ~kIOPCIRangeFlagRelocatable;
            }
        }

        expressCards |= (kPCIHotPlugRoot == child->supportsHotPlug);
        for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
        {
            childRange = child->ranges[rangeIndex];
            if (!childRange)                           continue;
            if (!((1 << childRange->type) & typeMask)) continue;

            canRelocate = (kIOPCIConfiguratorBoot & fFlags);
            canRelocate |= (0 != (kPCIDeviceStatePaused & child->deviceState));
            canRelocate |= !(kPCIDeviceStateAttached & child->deviceState);

            // Static devices can relocate only if they're not attached
            if (!(kPCIDeviceStateAttached & child->deviceState))
            {
                canRelocate |= true;
            }
            else if (kPCIStatic == (kPCIHPTypeMask & child->supportsHotPlug))
            {
                continue;
            }

            if ((rangeIndex == kIOPCIRangeBridgeBusNumber) && !canRelocate)
            {
                // bridges with no i/o allocations are bus relocatable
                canRelocate |= 
                    ((!child->ranges[kIOPCIRangeBridgeMemory]
                        || !child->ranges[kIOPCIRangeBridgeMemory]->nextSubRange)
                    && (!child->ranges[kIOPCIRangeBridgePFMemory]
                        || !child->ranges[kIOPCIRangeBridgePFMemory]->nextSubRange)
                    && (!child->ranges[kIOPCIRangeBridgeIO]
                        || !child->ranges[kIOPCIRangeBridgeIO]->nextSubRange));
            }

			// Don't relocate dead children; wait until they terminate then dealloc their ranges.
			canRelocate &= (0 == (kPCIDeviceStateDead & child->deviceState));

            if (canRelocate)
            {
                childRange->flags |= kIOPCIRangeFlagRelocatable;
            }
            else
            {
                childRange->flags &= ~kIOPCIRangeFlagRelocatable;
            }
        }
    }

	// Reserve the bridge's secondary bus from its own range
	if (((1 << kIOPCIResourceTypeBusNumber) & typeMask)
	 && (range = bridgeGetRange(bridge, kIOPCIResourceTypeBusNumber))
	 && !bridge->busResv.nextSubRange
	 && !bridge->fpbDown)
	{
		IOPCIRangeInit(&bridge->busResv, kIOPCIResourceTypeBusNumber,
						bridge->secBusNum, 1, kPCIBridgeBusNumberAlignment);
		ok = IOPCIRangeListAllocateSubRange(range, &bridge->busResv);
		DLOG("  BUS: reserved(%sok) 0x%llx\n", ok ? "" : "!", bridge->busResv.start);
	}

    // Do any frees, look for relocs, look for new allocs

	FOREACH_CHILD(bridge, child)
	{
		if (kPCIDeviceStateHidden & child->deviceState) continue;

		for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
		{
			childRange = child->ranges[rangeIndex];
			if (!childRange)                           continue;
			if (!((1 << childRange->type) & typeMask)) continue;
			range = bridgeGetRange(bridge, childRange->type);
			if (!range)                                continue;

			if (!childRange->nextSubRange
			 && ((kIOPCIRangeFlagMaximizeRoot | kIOPCIRangeFlagMaximizeSize) & childRange->flags))
			{
                // minSize
                childRange->size = minBridgeAlignments[childRange->type];

                // maxSize
                if (kIOPCIRangeFlagMaximizeRoot & childRange->flags)
                {
					if (bridge->isHostBridge && range && expressCards
					 && (kIOPCIResourceTypeBusNumber == childRange->type)) 
					{
						childRange->proposedSize = (range->size * 240) / 255;
					}
					else
					{
						childRange->proposedSize = -1ULL;
					}
				}
				else
				{
					childRange->proposedSize = childRange->totalSize + childRange->extendSize;
                }
			}
			else 
			{
                // maxSize
			    if (!childRange->nextSubRange 
			     || ((childRange->totalSize + childRange->extendSize) > childRange->size))
					childRange->proposedSize = childRange->totalSize + childRange->extendSize;
			}
            //
    
            logAllocatorRange(child, childRange, '\n');

			if (!fConsoleRange 
				&& fPFMConsole 
				&& (rangeIndex <= kIOPCIRangeBAR5)
				&& (fPFMConsole >= childRange->start) 
				&& (fPFMConsole < (childRange->start + childRange->proposedSize)))
			{
				DLOG("  hit console\n");
				fPFMConsole -= childRange->start;
				fConsoleRange = childRange;
			}

			if (!range) continue;
			type = range->type;

			if (bridge->isHostBridge 
				&& !childRange->nextSubRange
				&& (kIOPCIResourceTypeMemory == type)
				&& (childRange->maxAddress > 0xFFFFFFFFULL))
			{
				childRange->minAddress = (1ULL << 32);
				childRange->start = 0;
			}

			if (!childRange->proposedSize)
			{
				if (childRange->nextSubRange)
				{
					child->rangeBaseChanges |= (1 << rangeIndex);
					ok = IOPCIRangeListDeallocateSubRange(range, childRange);
					if (!ok) panic("IOPCIRangeListDeallocateSubRange");
					haveAllocs |= (1 << type);
				}
				childRange->start = 0;
				childRange->end   = 0;
				childRange->size  = 0;
				continue;
			}
			if (kIOPCIRangeFlagRelocatable & childRange->flags)  haveRelocs |= (1 << type);
			if (!childRange->nextSubRange)						 haveAllocs |= (1 << type);
			if (childRange->proposedSize != childRange->size)    haveAllocs |= (1 << type);
		}
	}

	// Free anything relocatable if new allocs needed

	if (haveAllocs & haveRelocs)
	{
		FOREACH_CHILD(bridge, child)
		{
			for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
			{
				childRange = child->ranges[rangeIndex];
				if (!childRange)                             continue;
				if (!((1 << childRange->type) & haveAllocs)) continue;
                if (!childRange->nextSubRange)               continue;

                range = bridgeGetRange(bridge, childRange->type);
                if (!range) continue;
				if (kIOPCIRangeFlagRelocatable & childRange->flags)
				{
					DLOG("  %s:    free reloc 0x%llx:0x%llx (at " D() ")\n",
							gPCIResourceTypeName[childRange->type],
							childRange->start, childRange->size,
							DEVICE_IDENT(child));
					child->rangeBaseChanges |= (1 << rangeIndex);
					ok = IOPCIRangeListDeallocateSubRange(range, childRange);
					if (!ok) panic("IOPCIRangeListDeallocateSubRange");
					childRange->start = 0;
					childRange->proposedSize = childRange->totalSize + childRange->extendSize;
				}
			}
		}
	}

	// Apply configuration changes to all children.
	FOREACH_CHILD(bridge, child)
	{
		applyConfiguration(child, typeMask, false);
	}

    // Find allocations to make

	FOREACH_CHILD(bridge, child)
	{
		for (int rangeIndex = 0; rangeIndex < kIOPCIRangeCount; rangeIndex++)
		{
			childRange = child->ranges[rangeIndex];
			if (!childRange)                           continue;
			if (!((1 << childRange->type) & typeMask)) continue;
			if (!childRange->proposedSize)			   continue;
			if (childRange->nextSubRange && (childRange->proposedSize == childRange->size))  continue;

			range = bridgeGetRange(bridge, childRange->type);
			if (!range) continue;
			type = range->type;

			child->rangeBaseChanges |= (1 << rangeIndex);
			childRange->device = child;
			IOPCIRangeAppendSubRange(&requests[type], childRange);

		    if ((kIOPCIRangeFlagSplay | kIOPCIRangeFlagMaximizeSize)
		             & childRange->flags)
		    {
                doOptimize |= (1 << type);
                if (!childRange->start) childRange->flags |= kIOPCIRangeFlagRelocatable;
		    }
		}
	}

    // Make allocations

    for (type = 0; type < kIOPCIResourceTypeCount; type++)
    {
		if (!(haveAllocs & (1 << type))) continue;
        range = bridgeGetRange(bridge, type);
        if (!range)                      continue;
        while ((childRange = requests[type]))
        {
            requests[type] = childRange->nextToAllocate;
			childRange->nextToAllocate = NULL;

			IOPCIScalar placed = childRange->start;
            ok = IOPCIRangeListAllocateSubRange(range, childRange);

            logAllocatorRange(childRange->device, childRange, ' ');
			DLOG("%sok allocated%s\n", 
                 ok ? " " : "!",
                 (ok && (childRange->size != childRange->proposedSize)) ? "(short)" : "");

			if (ok && (childRange->size == childRange->proposedSize))  continue;

			canRelocate = (0 != (kIOPCIConfiguratorBoot & fFlags));
			canRelocate |= (!childRange->nextSubRange);
			canRelocate |= (0 != (kIOPCIRangeFlagRelocatable & childRange->flags));

			if (canRelocate && placed)
			{
				if (childRange->nextSubRange)
				{
					DLOG("  %s:  free 0x%llx:0x%llx\n",
								gPCIResourceTypeName[type], 
								childRange->start, childRange->size);
					ok = IOPCIRangeListDeallocateSubRange(range, childRange);
					if (!ok) panic("IOPCIRangeListDeallocateSubRange");
				}
				childRange->start = 0;
				IOPCIRangeAppendSubRange(&requests[type], childRange);
			}
			else
			{
				shortage[type] += childRange->proposedSize;
				if (childRange->nextSubRange) shortage[type] -= childRange->size;
				if (childRange->alignment > shortageAlignments[type])
					shortageAlignments[type] = childRange->alignment;
                if (bridge->isHostBridge)
                {
                    DLOG("  %s: new host req:\n", gPCIResourceTypeName[type]);
                    IOPCIRangeDump(range);
                }
			}
        }

        if (bridge->isHostBridge)
		{
			if ((1 << type) & doOptimize)
			{
                DLOG("  %s: optimize\n", gPCIResourceTypeName[type]);
				IOPCIRangeListOptimize(range);
			}
			continue;
		}

		if (shortage[type])
		{
			IOPCIScalar newSize, extendAvail;
			extendAvail = IOPCIRangeListLastFree(range, shortageAlignments[type]);
			if (shortage[type] > extendAvail)
			{
				newSize = range->size + shortage[type] - extendAvail;
				if (newSize > (range->totalSize + range->extendSize))
				{
					if (kIOPCIRangeFlagMaximizeRoot & range->flags)	{}
					else
					{
						range->extendSize = IOPCIScalarAlign(newSize - range->totalSize, minBridgeAlignments[type]);;
						DLOG("  %s: shortage 0x%llx -> 0x%llx, 0x%llx\n",
							   gPCIResourceTypeName[type], range->size, range->totalSize, range->extendSize);
						bridge->rangeSizeChanges |= (1 << BRN(type));
						bridge->rangeRequestChanges |= (1 << type);
					}
				}
			}
			failTypes |= (1 << type);
		}
		else
		{
			if ((1 << type) & doOptimize)
			{
                DLOG("  %s: optimize\n", gPCIResourceTypeName[type]);
				IOPCIRangeListOptimize(range);
			}
			if (!(kIOPCIRangeFlagNoCollapse & range->flags))
			{
				shrink = IOPCIRangeListCollapse(range, minBridgeAlignments[type]);
				if (shrink)
				{
					DLOG("  %s: shrunk 0x%llx:0x%llx-0x%llx, 0x%llx,0x%llx\n",
						   gPCIResourceTypeName[type], range->start, 
						   range->size, range->proposedSize,
						   range->totalSize, range->extendSize);
					bridge->rangeSizeChanges    |= (1 << BRN(type));
					bridge->rangeRequestChanges |= (1 << type);
					failTypes |= (1 << type);
				}
			}
		}
    }

	result = (0 == failTypes);

    if ((failTypes && !(bridge->rangeRequestChanges & typeMask)) || (kIOPCIConfiguratorForcePause & fFlags))
    {
		bool artificialPause = !(failTypes && !(bridge->rangeRequestChanges & typeMask));

		if (!artificialPause)
		{
			result = true;
		}

		FOREACH_CHILD(bridge, child)
		{
			// Force a pause by pretending we failed to allocate memory and bus numbers.
			if (artificialPause && (child->deviceState & kPCIDeviceStateLinkInt))
			{
				failTypes = kIOPCIResourceTypeMemory | kIOPCIResourceTypeBusNumber;
			}
			if (artificialPause && !(child->deviceState & kPCIDeviceStateLinkInt))
			{
				continue;
			}

			if (!(kIOPCIConfiguratorUsePause & fFlags))   				  continue;
			// no pause for i/o
			if ((1 << kIOPCIResourceTypeIO) == failTypes) 				  continue;
			if ((kPCIDeviceStateRequestPause | kPCIDeviceStatePaused) 
				& child->deviceState) 				    				  continue;
			if ((kPCIHPTypeMask & child->supportsHotPlug) < kPCIHotPlug)  continue;
			if (!child->dtNub || !child->dtNub->inPlane(gIOServicePlane)) continue;
			if (treeInState(child, 
				kPCIDeviceStatePaused, kPCIDeviceStatePaused))            continue;

			DLOG("Request pause for " D() "\n", DEVICE_IDENT(child));
			child->deviceState |= kPCIDeviceStateRequestPause;
			markChanged(child);
			fWaitingPause++;
			result = -1;
		}
        if (true == result) DLOG("  exhausted\n");
    }

    if (!result)
    {
        bridge->rangeRequestChanges &= ~typeMask;
    }

	// Apply configuration changes to all children.
	FOREACH_CHILD(bridge, child)
	{
		applyConfiguration(child, typeMask, true);
	}

    return (result);
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

void CLASS::applyConfiguration(IOPCIConfigEntry * device, uint32_t typeMask, bool dolog)
{
    if ((!device->isHostBridge) && !(kPCIDeviceStateDeadOrHidden & device->deviceState))
    {
        if (device->rangeBaseChanges || device->rangeSizeChanges) 
        {
            switch (device->headerType)
            {
                case kPCIHeaderType0:
                    deviceApplyConfiguration(device, typeMask, dolog);
                    break;
                case kPCIHeaderType1:
                case kPCIHeaderType2:
                    bridgeApplyConfiguration(device, typeMask, dolog);
                    break;
            }
            device->deviceState &= ~kPCIDeviceStatePropertiesDone;

			IOPCIDevice *
			pciDevice = OSDynamicCast(IOPCIDevice, device->dtNub);
			if (pciDevice
			  && (kPCIDeviceStatePaused & device->deviceState)
				&& pciDevice->getProperty(kIOPCIResourcedKey))
			{
				// give up vtd sourceID
				pciDevice->relocate(true);
			}
        }
		if (!(kPCIDeviceStatePropertiesDone & device->deviceState))
			writeLatencyTimer(device);
    }

    device->deviceState |= kPCIDeviceStateConfigurationDone;
}

void CLASS::deviceApplyConfiguration(IOPCIConfigEntry * device, uint32_t typeMask, bool dolog)
{
    IOPCIScalar start;
    IOPCIRange * range;
    uint16_t     reg16;
    uint32_t     changedRanges = 0;

    DLOGI("Applying config (bm 0x%x, sm 0x%x) for device " D() "\n",
            device->rangeBaseChanges, device->rangeSizeChanges,
            DEVICE_IDENT(device));

	// Check for changed ranges
    for (int rangeIndex = kIOPCIRangeBAR0; rangeIndex <= kIOPCIRangeExpansionROM; rangeIndex++)
    {
        range = device->ranges[rangeIndex];
        if (!range || !((1 << range->type) & typeMask))
            continue;
        if ((0 != ((1 << rangeIndex) & device->rangeBaseChanges)) && range->start)
        {
            changedRanges |= (1 << rangeIndex);
        }
        device->rangeBaseChanges &= ~(1 << rangeIndex);
        device->rangeSizeChanges &= ~(1 << rangeIndex);
    }

    if (changedRanges == 0)
    {
        return;
    }

    reg16 = disableAccess(device, true);

	// Apply changes
    for (int rangeIndex = kIOPCIRangeBAR0; rangeIndex <= kIOPCIRangeExpansionROM; rangeIndex++)
    {
        uint32_t bar = 0;

        if ((changedRanges & (1 << rangeIndex)) == 0)
        {
            continue;
        }

        range = device->ranges[rangeIndex];
        start = range->start;
        if (rangeIndex <= kIOPCIRangeBAR5)
            bar = kIOPCIConfigBaseAddress0 + (rangeIndex * 4);
        else
            bar = kIOPCIConfigExpansionROMBase;
        configWrite32(device, bar, static_cast<uint32_t>(start));
        DLOGI("  [0x%x %s] 0x%llx, read 0x%x\n",
            bar, gPCIResourceTypeName[range->type],
            start & 0xFFFFFFFF, configRead32(device, bar));
        if (kIOPCIConfigExpansionROMBase != bar)
        {
            if (kIOPCIRangeFlagBar64 & range->flags)
            {
                rangeIndex++;
                bar += 4;
                start >>= 32;
                configWrite32(device, bar, static_cast<uint32_t>(start));
                DLOGI("  [0x%x %s] 0x%llx, read 0x%x\n",
                    bar, gPCIResourceTypeName[range->type],
                    start, configRead32(device, bar));
            }
        }
    }

//    reg16 &= ~(kIOPCICommandIOSpace | kIOPCICommandMemorySpace |
//               kIOPCICommandBusLead | kIOPCICommandMemWrInvalidate);
    restoreAccess(device, reg16);

    DLOGI("  Device Command = 0x%08x\n", (uint32_t) 
         configRead32(device, kIOPCIConfigCommand));
}

void CLASS::bridgeApplyConfiguration(IOPCIConfigEntry * bridge, uint32_t typeMask, bool dolog)
{
    IOPCIScalar  start;
    IOPCIScalar  end;
    IOPCIRange * range;
    uint16_t     commandReg = 0;
    uint32_t     baselim32;
    uint16_t     baselim16;
    bool         accessDisabled;

    enum { 
        kBridgeCommand = (kIOPCICommandIOSpace | kIOPCICommandMemorySpace | kIOPCICommandBusLead) 
    };

    do
    {
        accessDisabled = false;
        DLOGI("Applying config for bridge " B() " (disabled %d)\n",
                BRIDGE_IDENT(bridge), accessDisabled);

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
				uint32_t bar;
                start = range->start;
                bar = kIOPCIConfigBaseAddress0 + (rangeIndex * 4);
                configWrite32(bridge, bar, static_cast<uint32_t>(start));
				DLOGI("  [0x%x %s] 0x%llx, read 0x%x\n", 
					bar, gPCIResourceTypeName[range->type],
					start & 0xFFFFFFFF, configRead32(bridge, bar));
                if (kIOPCIRangeFlagBar64 & range->flags)
                {
                    rangeIndex++;
                    bar += 4;
                    start >>= 32;
                    configWrite32(bridge, bar, static_cast<uint32_t>(start));
                    DLOGI("  [0x%x %s] 0x%llx, read 0x%x\n", 
                        bar, gPCIResourceTypeName[range->type],
                        start, configRead32(bridge, bar));
                }
            }
            bridge->rangeBaseChanges &= ~(1 << thisIndex);
            bridge->rangeSizeChanges &= ~(1 << thisIndex);
        }

        if (((1 << kIOPCIResourceTypeBusNumber) & typeMask)
          && ((1 << kIOPCIRangeBridgeBusNumber) & (bridge->rangeBaseChanges | bridge->rangeSizeChanges)))
        {
			range = bridge->ranges[kIOPCIRangeBridgeBusNumber];
            DLOGI_RANGE("  BUS", range);
            uint8_t secondaryBus;

			if (range->start && range->size)
			{
				bridge->secBusNum = range->start;
				bridge->subBusNum = range->start + range->size - 1;
			}
			else if (bridge->fpbDown)
			{
				bridge->secBusNum = bridge->space.s.busNum;
				bridge->subBusNum = 0;
			}
			else
			{
				bridge->secBusNum = bridge->subBusNum = 0;
			}

            // Give children the correct bus
            secondaryBus = (bridge->fpbDown ? bridge->space.s.busNum : bridge->secBusNum);

            FOREACH_CHILD(bridge, child)
            {
				child->space.s.busNum = secondaryBus;
				child->deviceState &= ~kPCIDeviceStatePropertiesDone;
            }

            DLOGI("  OLD: prim/sec/sub = 0x%02x:0x%02x:0x%02x\n",
                 configRead8(bridge, kPCI2PCIPrimaryBus),
                 configRead8(bridge, kPCI2PCISecondaryBus),
                 configRead8(bridge, kPCI2PCISubordinateBus));
    
            // Program bridge bus numbers
    
            uint32_t reg32 = configRead32(bridge, kPCI2PCIPrimaryBus);
            reg32 &= ~0x00ffffff;
            reg32 |= bridge->space.s.busNum | (bridge->secBusNum << 8) | (bridge->subBusNum << 16);
            configWrite32(bridge, kPCI2PCIPrimaryBus, reg32);

            DLOGI("  BUS: prim/sec/sub = 0x%02x:0x%02x:0x%02x\n",
                 configRead8(bridge, kPCI2PCIPrimaryBus),
                 configRead8(bridge, kPCI2PCISecondaryBus),
                 configRead8(bridge, kPCI2PCISubordinateBus));

            // Program FPB caps

			if (bridge->fpbUp || bridge->fpbDown)
			{
				reg32 = (secondaryBus << 8) | (bridge->subDeviceNum << 3);
				configWrite32(bridge, bridge->fpbCapBlock + 8, (1 | (reg32 << 16)));
				configWrite32(bridge, bridge->fpbCapBlock + 12, reg32);

				configWrite32(bridge, bridge->fpbCapBlock + 28, 0);
				configWrite32(bridge, bridge->fpbCapBlock + 32, bridge->fpbDown ? 1 : 0);
				DLOGI("  FPB: control      = 0x%08x:0x%08x\n",
					 configRead32(bridge, bridge->fpbCapBlock + 8),
					 configRead32(bridge, bridge->fpbCapBlock + 12));
			}

            bridge->rangeBaseChanges &= ~(1 << kIOPCIRangeBridgeBusNumber);
            bridge->rangeSizeChanges &= ~(1 << kIOPCIRangeBridgeBusNumber);
        }

        // That's it for cardbus
        if (kPCIHeaderType2 == bridge->headerType) break;

        if (((1 << kIOPCIResourceTypeIO) & typeMask)
          && ((1 << kIOPCIRangeBridgeIO) & (bridge->rangeBaseChanges | bridge->rangeSizeChanges)))
        {
            // Program I/O base and limit

            DLOGI_RANGE("  I/O", bridge->ranges[kIOPCIRangeBridgeIO]);
    
            baselim16 = 0x00f0; // closed range
            range = bridge->ranges[kIOPCIRangeBridgeIO];
            if (range && range->start && range->size)
            {
                assert((range->size  & (4096-1)) == 0);
                assert((range->start & (4096-1)) == 0);
                assert((range->start & 0xffff0000) == 0);
    
                start = range->start;
                end = start + range->size - 1;
                baselim16 = ((start >> 8) & 0xf0) | (end & 0xf000);        
            }
            configWrite16(bridge, kPCI2PCIIORange, baselim16);
            configWrite32(bridge, kPCI2PCIUpperIORange, 0);

            DLOGI("  I/O: base/limit   = 0x%04x\n",
                 configRead16(bridge, kPCI2PCIIORange));

            bridge->rangeBaseChanges &= ~(1 << kIOPCIRangeBridgeIO);
            bridge->rangeSizeChanges &= ~(1 << kIOPCIRangeBridgeIO);
        }

        if (((1 << kIOPCIResourceTypeMemory) & typeMask))
//          && (((1 << kIOPCIRangeBridgeMemory) & (bridge->rangeBaseChanges | bridge->rangeSizeChanges))))
        {
            // Program memory base and limit
    
            DLOGI_RANGE("  MEM", bridge->ranges[kIOPCIRangeBridgeMemory]);

            baselim32 = 0x0000FFF0; // closed range
            range = bridge->ranges[kIOPCIRangeBridgeMemory];
            if (range && range->start && range->size && !(kPCIDeviceStateNoLink & bridge->deviceState))
            {
                assert((range->size  & (0x100000-1)) == 0);
                assert((range->start & (0x100000-1)) == 0);
    
                start = range->start;
                end   = range->start + range->size - 1;
                baselim32 = ((start >> 16) & 0xFFF0) | (end & 0xFFF00000);
            }
            configWrite32(bridge, kPCI2PCIMemoryRange, baselim32);

            DLOGI("  MEM: base/limit   = 0x%08x\n", (uint32_t) 
                 configRead32(bridge, kPCI2PCIMemoryRange));

            bridge->rangeBaseChanges &= ~(1 << kIOPCIRangeBridgeMemory);
            bridge->rangeSizeChanges &= ~(1 << kIOPCIRangeBridgeMemory);
        }

        if (((1 << kIOPCIResourceTypePrefetchMemory) & typeMask))
//          && (((1 << kIOPCIRangeBridgePFMemory) & (bridge->rangeBaseChanges | bridge->rangeSizeChanges))
        {
            // Program prefetchable memory base and limit

            DLOGI_RANGE("  PFM", bridge->ranges[kIOPCIRangeBridgePFMemory]);
    
            baselim32 = 0x0000FFF0; // closed range
            start     = 0xFFFFFFFFFFFFFFFF;
            end       = 0;

            if ((1 << kIOPCIRangeBridgePFMemory) & bridge->rangeBaseChanges)
			{
				configWrite32(bridge, kPCI2PCIPrefetchUpperBase,  -1U);
				configWrite32(bridge, kPCI2PCIPrefetchUpperLimit,  0);
				configWrite32(bridge, kPCI2PCIPrefetchMemoryRange, baselim32);
			}
            range = bridge->ranges[kIOPCIRangeBridgePFMemory];
            if (range && range->start && range->size && !(kPCIDeviceStateNoLink & bridge->deviceState))
            {
                assert((range->size  & (0x100000-1)) == 0);
                assert((range->start & (0x100000-1)) == 0);
    
                start = range->start;
                end = range->start + range->size - 1;
                baselim32 = ((start >> 16) & 0xFFF0) | (end & 0xFFF00000);
            }
            configWrite32(bridge, kPCI2PCIPrefetchMemoryRange, baselim32);
            configWrite32(bridge, kPCI2PCIPrefetchUpperLimit, (end   >> 32));
            configWrite32(bridge, kPCI2PCIPrefetchUpperBase,  (start >> 32));

            DLOGI("  PFM: base/limit   = 0x%08x, 0x%08x, 0x%08x\n",  
                 (uint32_t)configRead32(bridge, kPCI2PCIPrefetchMemoryRange),
                 (uint32_t)configRead32(bridge, kPCI2PCIPrefetchUpperBase),
                 (uint32_t)configRead32(bridge, kPCI2PCIPrefetchUpperLimit));

            bridge->rangeBaseChanges &= ~(1 << kIOPCIRangeBridgePFMemory);
            bridge->rangeSizeChanges &= ~(1 << kIOPCIRangeBridgePFMemory);
        }
    }
    while (false);

    // Set IOSE, memory enable, Bus Lead transaction forwarding

    DLOGI("Enabling bridge " B() "\n", BRIDGE_IDENT(bridge));

    if (kPCIHeaderType2 == bridge->headerType)
    {
    }
    else
    {
        uint16_t bridgeControl;

        commandReg |= (kIOPCICommandIOSpace 
                     | kIOPCICommandMemorySpace 
//                     | kIOPCICommandSERR 
                     | kIOPCICommandBusLead);

        // Turn off ISA bit.
        bridgeControl = configRead16(bridge, kPCI2PCIBridgeControl);
        if (bridgeControl & 0x0004)
        {
            bridgeControl &= ~0x0004;
            configWrite16(bridge, kPCI2PCIBridgeControl, bridgeControl);
            DLOGI("  Bridge Control    = 0x%04x\n",
                 configRead16(bridge, kPCI2PCIBridgeControl));
        }
    }

    restoreAccess(bridge, commandReg);

    DLOGI("  Bridge Command    = 0x%08x\n", 
         configRead32(bridge, kIOPCIConfigCommand));
}

//---------------------------------------------------------------------------

#ifndef ExtractLSB
#define ExtractLSB(x) ((x) & (~((x) - 1)))
#endif

void CLASS::checkCacheLineSize(IOPCIConfigEntry * device)
{
    uint8_t cacheLineSize, cls, was;

    if (device->isHostBridge)
        return;

    if (kPCIStatic == (kPCIHPTypeMask & device->parent->supportsHotPlug))
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

int32_t CLASS::bridgeFinalizeConfigProc(void * unused, IOPCIConfigEntry * bridge)
{
	uint32_t deviceControl, newControl;

    if (!(kPCIDeviceStateAllocated & bridge->deviceState)) return (true);
    bridge->deviceState &= ~kPCIDeviceStateChildChanged;

#if defined(__i386__) || defined(__x86_64__)
	if ((kPCIHPTypeMask & bridge->supportsHotPlug) >= kPCIHotPlug)
#endif
	{
		FOREACH_CHILD(bridge, child)
		{
			if (kPCIDeviceStateDeadOrHidden   & child->deviceState) continue;
			if (!child->expressCapBlock) 							continue;
			if (!(child->rootPortEntry->deviceState & kPCIDeviceStateDomainChanged)) continue;
			deviceControl = configRead16(child, child->expressCapBlock + 0x08);
			if ((fStates & kIOPCIConfiguratorMPSOverride) && (child->deviceState & kPCIDeviceStatePaused))
			{
				fStates &= ~kIOPCIConfiguratorMPSOverride;
				topologyMPSOverride(child->rootPortEntry);
				// since the root port is not paused, this is the USP connected to the root port. We need to manually update root port's MPS.
				uint32_t rootPortDeviceControl = configRead16(child->rootPortEntry, child->rootPortEntry->expressCapBlock + 0x08);
				uint32_t rootPortNewControl = rootPortDeviceControl & ~(7 << 5);
				rootPortNewControl |= ((child->rootPortEntry->expressMaxPayload & 7) << 5);
				configWrite16(child->rootPortEntry, child->rootPortEntry->expressCapBlock + 0x08, rootPortNewControl);
			}
			newControl    = deviceControl & ~(7 << 5);
			newControl    |= ((child->expressMaxPayload & 7) << 5);
			if (child->isBridge && child != child->rootPortEntry && (child->expressMaxPayload == child->rootPortEntry->expressMaxPayload))
			{
				newControl = newControl & ~(7 << 12);
				newControl |= (DEFAULT_MRRS << 12);
			}
			if (!child->isBridge)
			{
				bool epMRRSOverride = (child->hostBridgeEntry->expressEndpointMaxReadRequestSize != -1);
				bool epMPSLowerThanRPMPS = (child->expressMaxPayload < child->rootPortEntry->expressMaxPayload);
				int8_t tmpEPMRRS = -1;
				if (epMPSLowerThanRPMPS && epMRRSOverride)
				{
					// if EP MPS < RP MPS and we have EP MRRS override, EP MRRS = min(EP MPS, EP MRRS override)
					tmpEPMRRS = (child->expressMaxPayload < child->hostBridgeEntry->expressEndpointMaxReadRequestSize) ? child->expressMaxPayload : child->hostBridgeEntry->expressEndpointMaxReadRequestSize;
				}
				else if (epMPSLowerThanRPMPS && !epMRRSOverride)
				{
					// if EP MPS < RP MPS and there's no EP MRRS override, EP MRRS = EP MPS
					tmpEPMRRS = child->expressMaxPayload;
				}
				else if (!epMPSLowerThanRPMPS)
				{
					if (epMRRSOverride)
					{
						// if EP MPS == RP MPS and we have EP MRRS override, EP MRRS = EP MRRS override
						tmpEPMRRS = child->hostBridgeEntry->expressEndpointMaxReadRequestSize;
					}
					else
					{
						// if EP MPS == RP MPS and there's no EP MRRS override, EP MRRS = 512
						tmpEPMRRS = DEFAULT_MRRS;
					}
				}

				if (tmpEPMRRS != -1)
				{
					tmpEPMRRS = (tmpEPMRRS & 7);
					newControl = newControl & ~(7 << 12);
					newControl |= (tmpEPMRRS << 12);
					DLOG("device " D() " Max Read Request Size set to 0x%x\n", DEVICE_IDENT(child), tmpEPMRRS);
				}
			}
			if (newControl != deviceControl)
			{
				configWrite16(child, child->expressCapBlock + 0x08, newControl);
				DLOG("payload set 0x%08x -> 0x%08x (at " D() "), maxPayload 0x%x\n",
					  deviceControl, newControl,
					  DEVICE_IDENT(child), child->expressMaxPayload);
            }
		}
	}

	if ((kPCIDeviceStateChildAdded & bridge->deviceState) && !fWaitingPause)
	{
		bridge->deviceState &= ~kPCIDeviceStateChildAdded;
		markChanged(bridge);
	}

	return (bridgeConstructDeviceTree(unused, bridge));
}

//---------------------------------------------------------------------------

int32_t CLASS::domainFinalizeConfigProc(void * unused, IOPCIConfigEntry * bridge)
{
    if ((bridge == bridge->rootPortEntry) && (bridge->deviceState & kPCIDeviceStateDomainChanged) && (bridge->deviceState & kPCIDeviceStateAllocated))
    {
        bridge->deviceState &= ~kPCIDeviceStateDomainChanged;
    }
    return true;
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
		DLOG("config protect fail(1) for device " D() "\n", DEVICE_IDENT(device));
		OSReportWithBacktrace("config protect fail(1) for device " D() "\n",
								DEVICE_IDENT(device));
	}

	return (ok);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum
{
   kConfigWrite = 0x00,
   kConfigRead  = 0x01,
   kConfig32    = (sizeof(uint32_t) << 1),
   kConfig16    = (sizeof(uint16_t) << 1),
   kConfig8     = (sizeof(uint8_t)  << 1),
};

void CLASS::configAccess(IOPCIConfigEntry * device, uint32_t access, uint32_t offset, void * data)
{
	uint8_t * addr;

	addr = device->configShadow + offset;
	if (kConfigRead & access) bcopy(addr, data, (access >> 1));
	else                      bcopy(data, addr, (access >> 1));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint32_t CLASS::configRead32( IOPCIConfigEntry * device, uint32_t offset, IOPCIAddressSpace *targetAddressSpace )
{
    IOPCIAddressSpace space = device->space;
    if (targetAddressSpace == NULL)
    {
        if (device->configShadow)
        {
            uint32_t data;
            configAccess(device, kConfig32|kConfigRead, offset, &data);
            return (data);
        }

        if (!configAccess(device, false)) return (0xFFFFFFFF);
    }
    else
    {
        space = *targetAddressSpace;
    }

    space.es.registerNumExtended = (offset >> 8);
    assert(device->hostBridge);
    return (device->hostBridge->configRead32(space, offset));
}

void CLASS::configWrite32( IOPCIConfigEntry * device,
                           uint32_t offset, uint32_t data, IOPCIAddressSpace *targetAddressSpace )
{
    IOPCIAddressSpace space = device->space;
    if (targetAddressSpace == NULL)
    {
        if (device->configShadow)
        {
            configAccess(device, kConfig32|kConfigWrite, offset, &data);
        }

        if (!configAccess(device, true)) return;
    }
    else
    {
        space = *targetAddressSpace;
    }

    space.es.registerNumExtended = (offset >> 8);
    assert(device->hostBridge);
    device->hostBridge->configWrite32(space, offset, data);
}

uint16_t CLASS::configRead16( IOPCIConfigEntry * device, uint32_t offset, IOPCIAddressSpace *targetAddressSpace )
{
    IOPCIAddressSpace space = device->space;
    if (targetAddressSpace == NULL)
    {
        if (device->configShadow)
        {
            uint16_t data;
            configAccess(device, kConfig16|kConfigRead, offset, &data);
            return (data);
        }

        if (!configAccess(device, false)) return (0xFFFF);
    }
    else
    {
        space = *targetAddressSpace;
    }

    space.es.registerNumExtended = (offset >> 8);
    assert(device->hostBridge);
    return (device->hostBridge->configRead16(space, offset));
}

void CLASS::configWrite16( IOPCIConfigEntry * device,
                           uint32_t offset, uint16_t data, IOPCIAddressSpace *targetAddressSpace )
{
    IOPCIAddressSpace space = device->space;
    if (targetAddressSpace == NULL)
    {
        if (device->configShadow)
        {
            configAccess(device, kConfig16|kConfigWrite, offset, &data);
        }

        if (!configAccess(device, true)) return;
    }
    else
    {
        space = *targetAddressSpace;
    }

    space.es.registerNumExtended = (offset >> 8);
    assert(device->hostBridge);
    device->hostBridge->configWrite16(space, offset, data);
}

uint8_t CLASS::configRead8( IOPCIConfigEntry * device, uint32_t offset, IOPCIAddressSpace *targetAddressSpace )
{
    IOPCIAddressSpace space = device->space;
    if (targetAddressSpace == NULL)
    {
        if (device->configShadow)
        {
            uint8_t data;
            configAccess(device, kConfig8|kConfigRead, offset, &data);
            return (data);
        }

        if (!configAccess(device, false)) return (0xFF);
    }
    else
    {
        space = *targetAddressSpace;
    }

    space.es.registerNumExtended = (offset >> 8);
    assert(device->hostBridge);
    return (device->hostBridge->configRead8(space, offset));
}

void CLASS::configWrite8( IOPCIConfigEntry * device,
                            uint32_t offset, uint8_t data, IOPCIAddressSpace *targetAddressSpace )
{
     IOPCIAddressSpace space = device->space;
    if (targetAddressSpace == NULL)
    {
        if (device->configShadow)
        {
            configAccess(device, kConfig8|kConfigWrite, offset, &data);
        }

        if (!configAccess(device, true)) return;
    }
    else
    {
        space = *targetAddressSpace;
    }

    space.es.registerNumExtended = (offset >> 8);
    assert(device->hostBridge);
    device->hostBridge->configWrite8(space, offset, data);
}
/* -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: t -*- */
