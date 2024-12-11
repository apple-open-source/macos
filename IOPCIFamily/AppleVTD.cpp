/*
 * Copyright (c) 2012-2021 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/pci/IOPCIPrivate.h>

#if ACPI_SUPPORT

#include <IOKit/IOMapper.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOTimerEventSource.h>
#include <libkern/tree.h>
#include <libkern/OSDebug.h>
#include <i386/cpuid.h>
#include <libkern/sysctl.h>

#include "AppleVTD.h"

IOPCIMessagedInterruptController  * gIOPCIMessagedInterruptController = nullptr;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
extern "C" ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define FREE_ON_FREE	0

#define KP				0
#define	VTASRT			0

#define kLargeThresh	(128)
#define kLargeThresh2	(32)
#define kVPages  		(1<<25)
#define kBPagesLog2 	(19)
#define kBPagesSafe		((1<<kBPagesLog2)-(1<<(kBPagesLog2 - 2)))      /* 3/4 */
#define kBPagesReserve	((1<<kBPagesLog2)-(1<<(kBPagesLog2 - 3)))      /* 7/8 */
#define kRPages  		(1<<20)

#define kQIPageCount        (2)
#define kQIIndexMask        ((kQIPageCount * 256) - 1)
#define kQIIndexStoreMask   (31)
#define kQIIndexStampMask   (kQIIndexMask >> 1)

#define kTlbDrainReads  (0ULL)
#define kTlbDrainWrites (0ULL)

#define kMaxRounding    (10)

#define VTLOG(fmt, args...)                   \
    do {                                                    						\
        if ((gIOPCIFlags & kIOPCIConfiguratorVTLog) && !ml_at_interrupt_context())  \
            IOLog(fmt, ## args);                           							\
        if (gIOPCIFlags & kIOPCIConfiguratorVTLog)        							\
            kprintf(fmt, ## args);                          						\
    } while(0)


#if VTASRT

#define vtassert(ex)  \
	((ex) ? (void)0 : Assert(__FILE__, __LINE__, # ex))

#define vtd_space_nfault(x,y,z) _vtd_space_nfault(x,y,z)

#define STAT_ADD(space, name, value) do { space->stats.name += value; } while (false);

#else	/* VTASRT */

#define vtassert(ex)  
#define vtd_space_nfault(x,y,z)

#define STAT_ADD(space, name, value) do { space->stats.name += value; } while (false);
//#define STAT_ADD(space, name, value)

#endif	/* !VTASRT */

#define table_flush(addr, size, linesize) __mfence();

#define BLOCK(l)	IOSimpleLockLock(l)
#define BUNLOCK(l)	IOSimpleLockUnlock(l)

#define arrayCount(x)	(sizeof(x) / sizeof(x[0]))

#define stampPassed(a,b)	(((int32_t)((a)-(b))) >= 0)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static inline void __mfence(void)
{
    __asm__ volatile("mfence");
}

static inline void __clflush(void *ptr)
{
	__asm__ volatile("clflush (%0)" : : "r" (ptr));
}

static inline void clflush(uintptr_t addr, unsigned int count, uintptr_t linesize)
{
	uintptr_t  bound = (addr + count + linesize -1) & ~(linesize - 1);
	__mfence();
	while (addr < bound) 
	{
		__clflush((void *) (uintptr_t) addr);
		addr += linesize;
	}
	__mfence();
}

static void unit_quiesce(vtd_unit_t * unit);

static
vtd_unit_t * unit_init(ACPI_DMAR_HARDWARE_UNIT * dmar)
{
	vtd_unit_t             * unit;
	ACPI_DMAR_DEVICE_SCOPE * scope;
	ACPI_DMAR_PCI_PATH     * path;
	uintptr_t                enddmar, endscope;
	uint32_t                 paths, bus0ep;
	int x2apic_mode = 0;
	size_t siz = sizeof(x2apic_mode);

	unit = IOMallocType(vtd_unit_t);
	if (!unit) return (NULL);

	unit->dmar = dmar;

	VTLOG("dmar %p Address %llx, Flags %x\n",
			dmar, dmar->Address, dmar->Flags);

	unit->regs = (typeof unit->regs) ml_io_map(dmar->Address, 0x1000);

	uint32_t
	offset = (unit->regs->extended_capability >> (8 - 4)) & (((1 << 10) - 1) << 4);
	unit->iotlb = (typeof(unit->iotlb)) (((uintptr_t)unit->regs) + offset);

	offset = (unit->regs->capability >> (24 - 4)) & (((1 << 10) - 1) << 4);
	unit->faults = (typeof(unit->faults)) (((uintptr_t)unit->regs) + offset);
	unit->num_fault = (1 + ((unit->regs->capability >> 40) & ((1 << 8) - 1)));

	unit->selective = (1 & (unit->regs->capability >> 39));
	unit->rounding = unit->hwrounding = (0x3f & (unit->regs->capability >> 48));
	if (unit->rounding > kMaxRounding) unit->rounding = kMaxRounding;
	unit->caching = (1 & (unit->regs->capability >> 7));
	unit->global = (ACPI_DMAR_INCLUDE_ALL & dmar->Flags);
	unit->domains = (1 << (4 + (14 & (unit->regs->capability << 1))));

	unit->intmapper = (1 & (unit->regs->extended_capability >> 3));
	unit->qi = (1 & (unit->regs->extended_capability >> 1));

	/* Get x2APIC enable status from XNU */
	(void)sysctlbyname("machdep.x2apic_enabled", &x2apic_mode, &siz, 0, 0);

	/* Bit 4 indicates support for Extended Interrupt Mode (x2APIC support) */
	if (x2apic_mode && ((1UL << 4) & unit->regs->extended_capability) == 0) {
		x2apic_mode = 0;
		/*
		 * Note that we should not need to tell XNU to go back to
		 * legacy xAPIC mode because this scenario should never happen
		 * (if the CPU reports x2APIC mode is supported, the VT-d IRE
		 * must support x2APIC (and EIM)).
		 */
		VTLOG("XNU reports x2APIC mode enabled, but IRE does not support it!\n");
		kprintf("XNU reports x2APIC mode enabled, but IRE does not support it!\n");
	}

	unit->x2apic_mode = x2apic_mode;

	VTLOG(" cap 0x%llx extcap 0x%llx glob %d round %d cache sel %d mode %d iotlb %p nfault[%d] %p x2apic %d\n",
			unit->regs->capability, unit->regs->extended_capability,
			unit->global, unit->hwrounding, unit->selective, unit->caching,
			unit->iotlb, unit->num_fault, unit->faults, (int)unit->x2apic_mode);

	if (os_add_overflow(((uintptr_t)dmar), dmar->Header.Length, &enddmar)) enddmar = 0;
	scope = (typeof(scope)) (dmar + 1);
	bus0ep = paths = 0;
    while (scope && (((uintptr_t) &scope[1]) <= enddmar))
    {
		VTLOG(" scope type %d, bus %d", scope->EntryType, scope->Bus);
		bus0ep += ((ACPI_DMAR_SCOPE_TYPE_ENDPOINT == scope->EntryType) && (!scope->Bus));
		if (os_add_overflow(((uintptr_t)scope), scope->Length, &endscope)) endscope = 0;
		if (endscope > enddmar)                                            endscope = 0;
	    path = (typeof(path)) (scope + 1);
	    while (((uintptr_t) &path[1]) <= endscope)
	    {
			VTLOG(", path %d,%d", path->Device, path->Function);
			paths++;
			path++;
	    }
	    VTLOG("\n");
	    scope = (typeof(scope)) endscope;
    }
    unit->ig = (!unit->global && (1 == paths) && (1 == bus0ep));

	// enable translation on non-IG unit
	if (unit->global || !unit->ig || (kIOPCIConfiguratorIGIsMapped & gIOPCIFlags))
	{
		unit->translating = 1;
	}
	VTLOG(" ig %d translating %d\n", unit->ig, unit->translating);

	// caching is only allowed for VMs
	if (unit->caching || !unit->qi)
	{
		IOFreeType(unit, vtd_unit_t);
		unit = NULL;
	}
	
	return (unit);
}

static void 
unit_faults(vtd_unit_t * unit, bool log)
{
	uint32_t idx;
	for (idx = 0; idx < unit->num_fault; idx++)
	{
		uint64_t h, l;
		uint32_t faults_pending;

		faults_pending = unit->regs->fault_status;
		h = unit->faults[idx].fault_high;
		l = unit->faults[idx].fault_low;
		unit->faults[idx].fault_high = h;
		unit->regs->fault_status = faults_pending;
		__mfence();
		if (log && ((1ULL << 63) & h))
		{
			char msg[256];
			snprintf(msg, sizeof(msg), "vtd[%d] fault: device %d:%d:%d reason 0x%x %c:0x%llx", idx,
				(int)(255 & (h >> 8)), (int)(31 & (h >> 3)), (int)(7 & (h >> 0)),
				(int)(255 & (h >> (96 - 64))), (h & (1ULL << (126 - 64))) ? 'R' : 'W', l);
			IOLog("%s\n", msg);
			kprintf("%s\n", msg);
			if (kIOPCIConfiguratorPanicOnFault & gIOPCIFlags) panic("%s", msg);
		}
	}		
}

static void 
unit_quiesce(vtd_unit_t * unit)
{
	VTLOG("unit %p quiesce status 0x%x\n", unit, unit->regs->global_status);

	// disable DMA translation, interrupt remapping, invalidation queue.
	// it is not expected for any DMA to be inflight at the time of this call.
	// invalidation completions will be halted, considered complete, since the
	// tlbs will be flushed on enable.
	unit->regs->global_command = 0;
	__mfence();
	while (((1UL<<26)|(1<<31)|(1<<25)) & unit->regs->global_status) {}
}

static void
unit_interrupts_enable(vtd_unit_t * unit)
{
	if (unit->msi_address)
	{
		unit->regs->invalidation_completion_event_data          = unit->msi_data;
		unit->regs->invalidation_completion_event_address       = unit->msi_address & 0xFFFFFFFF;
		unit->regs->invalidation_completion_event_upper_address = (unit->msi_address >> 32);

		unit->regs->fault_event_data          = unit->msi_data + 1;
		unit->regs->fault_event_address       = unit->msi_address & 0xFFFFFFFF;
		unit->regs->fault_event_upper_address = (unit->msi_address >> 32);

		__mfence();
		unit_faults(unit, false);

		unit->regs->fault_event_control = 0;					// ints ena
		unit->regs->invalidation_completion_event_control = 0;	// ints ena
		unit->regs->invalidation_completion_status = 1;
	}
}

static void
unit_enable(vtd_unit_t * unit, uint32_t qi_stamp)
{
	uint32_t command;

	// unit may already be enabled if EFI left it that way, disable.
	unit_quiesce(unit);

	VTLOG("unit %p enable status 0x%x\n", unit, unit->regs->global_status);
	if (unit->translating)
	{
		unit->regs->root_entry_table = unit->root;
		__mfence();

		unit->regs->global_command = (1UL<<30);
		__mfence();
		while (!((1UL<<30) & unit->regs->global_status)) {}
		//	VTLOG("did set root\n");

		unit->regs->context_command = (1ULL<<63) | (1ULL<<61);
		__mfence();
		while ((1ULL<<63) & unit->regs->context_command) {}
		//	VTLOG("did context inval\n");

		// global & rw drain
		unit->iotlb->command = (1ULL<<63) | (1ULL<<60) | (1ULL<<49) | (1ULL<<48);
		__mfence();
		while ((1ULL<<63) & unit->iotlb->command) {}
		//	VTLOG("did iotlb inval\n");
	}

	// hw resets head on disable
	unit->qi_tail = 0;
	unit->regs->invalidation_queue_tail = 0;
	unit->regs->invalidation_queue_address = unit->qi_address;
	unit->qi_stamp = qi_stamp;

	command = 0;

	// enable QI
	command |= (1UL<<26);
	unit->regs->global_command = command;
	__mfence();
	while (!((1UL<<26) & unit->regs->global_status)) {}
	VTLOG("did ena qi p 0x%qx v %p\n", unit->qi_address, unit->qi_table);

	if (unit->intmapper && unit->ir_address)
	{
		if (unit->x2apic_mode) {
			/* Bit 11 is the Extended Interrupt Mode Enable (1=X2APIC Active) bit */
			unit->regs->interrupt_remapping_table = unit->ir_address | (1UL << 11);
		} else {
			unit->regs->interrupt_remapping_table = unit->ir_address;
		}
		unit->regs->global_command = (1UL<<24);
		__mfence();
		while (!((1UL<<24) & unit->regs->global_status)) {}
		VTLOG("did set ir p 0x%qx\n", unit->ir_address);

		// enable IR
		command |= (1UL<<25);
#if 0
		if (!unit->x2apic_mode) {
			command |= (1UL<<23);                      // Compatibility Format Interrupt ok
		}
#endif
		unit->regs->global_command = command;
		__mfence();
		while (!((1UL<<25) & unit->regs->global_status)) {}
		VTLOG("did ena ir\n");
	}

	if (unit->translating)
	{
		 // enable translation
		 command |= (1UL<<31);
		 unit->regs->global_command = command;
		 __mfence();
		 while (!((1UL<<31) & unit->regs->global_status)) {}
		 VTLOG("did ena trans\n");
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */


static vtd_vaddr_t 
vtd_log2up(vtd_vaddr_t size)
{
	if (1 == size) size = 0;
	else size = 32 - __builtin_clz((unsigned int)size - 1);
	return (size);
}

static vtd_vaddr_t 
vtd_log2down(vtd_vaddr_t size)
{
	size = 31 - __builtin_clz((unsigned int)size);
	return (size);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static vtd_bitmap_t *
vtd_bitmap_alloc(uint32_t count)
{
    vtd_bitmap_t * bitmap;
    uint32_t	   bitmapwords;

	bitmapwords = (count + 63) >> 6;

    bitmap = (typeof(bitmap)) IOMallocData(sizeof(vtd_bitmap_t) + bitmapwords * sizeof(uint64_t));
    if (!bitmap) return (bitmap);

	bitmap->count       = count;
	bitmap->bitmapwords = bitmapwords;
	bzero(&bitmap->bitmap[0], bitmapwords * sizeof(uint64_t));

    return (bitmap);
}

static void
vtd_bitmap_free(vtd_bitmap_t * bitmap)
{
    IOFreeData(bitmap, sizeof(vtd_bitmap_t) + bitmap->bitmapwords * sizeof(uint64_t));
}

static void
vtd_bitmap_bitset(vtd_bitmap_t * bitmap, boolean_t set, uint32_t index)
{
	if (set)
	{
	    bitmap->bitmap[index >> 6] |= (0x8000000000000000ULL >> (index & 63));
	}
	else
	{
	    bitmap->bitmap[index >> 6] &= ~(0x8000000000000000ULL >> (index & 63));
	}
}

static boolean_t
vtd_bitmap_bittst(vtd_bitmap_t * bitmap, uint32_t index)
{
    boolean_t result;
	result = (0 != (bitmap->bitmap[index >> 6] & (0x8000000000000000ULL >> (index & 63))));
    return (result);
}

// count bits clear or set (set == TRUE) starting at index.
static uint32_t
vtd_bitmap_count(vtd_bitmap_t * bitmap, uint32_t set, uint32_t index, uint32_t max)
{
    uint32_t word, bit, count, chunk;
    uint64_t bits;

    count = 0;

	vtassert((index + max) <= bitmap->count);

    word = index >> 6;
    bit =  index & 63;

    bits = bitmap->bitmap[word];
    if (set) bits = ~bits;
    bits = (bits << bit);
    if (bits)
    {
        chunk = __builtin_clzll(bits);
        if (chunk > max) chunk = max;
        count += chunk;
    }
    else
    {
		chunk = 64 - bit;
		if (chunk > max) chunk = max;
        count += chunk;
        max -= chunk;

		while (max && (++word < bitmap->bitmapwords))
		{
			bits = bitmap->bitmap[word];
			if (set) bits = ~bits;
			if (bits)
			{
				chunk = __builtin_clzll(bits);
				if (chunk > max) chunk = max;
				count += chunk;
				break;
			}
			chunk = 64;
			if (chunk > max) chunk = max;
			max -= chunk;
			count += chunk;
		}
    }

    return (count);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static bool
vtd_space_present(vtd_space_t * bf, vtd_vaddr_t start)
{
	vtassert(start < bf->vsize);
    return (vtd_bitmap_bittst(bf->table_bitmap, (start >> 9)));
}

static void __unused
_vtd_space_nfault(vtd_space_t * bf, vtd_vaddr_t start, vtd_vaddr_t size)
{
	vtd_vaddr_t count;

	vtassert((start + size) <= bf->vsize);

	count = (start + size + 511) >> 9;
	start >>= 9;
	count -= start;

	vtassert(count == vtd_bitmap_count(bf->table_bitmap, true, start, count));
}

static void
vtd_space_fault(vtd_space_t * bf, vtd_vaddr_t start, vtd_vaddr_t size)
{
	IOReturn    kr;
    vtd_vaddr_t count;
    uint32_t    present, missing, page;

	vtassert((start + size) <= bf->vsize);

	count = (start + size + 511) >> 9;
	start >>= 9;
	count -= start;

	while (count)
	{
        present = vtd_bitmap_count(bf->table_bitmap, true, start, count);

        vtassert(count >= present);
        count -= present;
		if (!count) break;

        start += present;
        missing = vtd_bitmap_count(bf->table_bitmap, false, start, count);
        vtassert(missing);
        vtassert(count >= missing);

		kr = bf->table_map->wireRange(kIODirectionOutIn, ptoa_64(start), ptoa_64(missing));
		vtassert(kr == KERN_SUCCESS);
		STAT_ADD(bf, tables, missing);

		for (page = start; page < (start + missing); page++)
		{
//			VTLOG("table fault addr 0x%x, table %p\n", start, &bf->tables[0][start]);
			vtd_bitmap_bitset(bf->table_bitmap, true, page);

			bf->tables[0][page << 9].bits = 0;
			ppnum_t lvl0page = pmap_find_phys(kernel_pmap, (uintptr_t) &bf->tables[0][page << 9]);
			if (!lvl0page) panic("!lvl0page");
			bf->tables[1][page].bits = ptoa_64(lvl0page) | kPageAccess;
		}
		table_flush(&bf->tables[1][start], missing * sizeof(vtd_table_entry_t), bf->cachelinesize);
		count -= missing;
		start += missing;
	}
}

static void
vtd_space_set(vtd_space_t * bf, vtd_vaddr_t start, vtd_vaddr_t size,
			  uint32_t mapOptions, const upl_page_info_t * pageList)
{
	ppnum_t idx;
	uint8_t access = 0*kReadAccess | 0*kWriteAccess;

	if (kIODMAMapPhysicallyContiguous & mapOptions) VTLOG("map phys %x, %x\n", pageList[0].phys_addr, size);

	if (mapOptions & kIODMAMapReadAccess)  access |= kReadAccess;
	if (mapOptions & kIODMAMapWriteAccess) access |= kWriteAccess;

	vtassert((start + size) <= bf->vsize);
	vtd_space_nfault(bf, start, size);

	if (kIODMAMapPhysicallyContiguous & mapOptions)
	{
		for (idx = 0; idx < size; idx++)
		{
			bf->tables[0][start + idx].bits = (access | ptoa_64(pageList[0].phys_addr + idx));
		}
	}
	else
	{
		for (idx = 0; idx < size; idx++)
		{
			bf->tables[0][start + idx].bits = (access | ptoa_64(pageList[idx].phys_addr));
		}
	}
	__mfence();
}

#include "balloc.c"
#include "rballoc.c"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleVTDDeviceMapper;
enum
{
	kDeviceMapperPause      = 0x00000000,
	kDeviceMapperUnpause    = 0x00000001,
	kDeviceMapperActivate   = 0x00000002,
	kDeviceMapperDeactivate = 0x00000004,
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(AppleVTD, IOMapper);
#define super IOMapper

IOLock * gAppleVTDDeviceMapperLock;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
AppleVTD::install(IOWorkLoop * wl, uint32_t flags,
					IOService * provider, const OSData * data,
				    IOPCIMessagedInterruptController  * messagedInterruptController)
{
	AppleVTD * mapper = 0;
	bool ok = false;

	gIOPCIMessagedInterruptController = messagedInterruptController;

	VTLOG("DMAR %p\n", data);
	if (data) 
	{
        gAppleVTDDeviceMapperLock = IOLockAlloc();
		mapper = new AppleVTD;
		if (mapper)
		{
			if (mapper->init(wl, data) && mapper->attach(provider))
			{
				ok = mapper->start(provider);
				if (!ok) mapper->detach(provider);
			}
			mapper->release();
		}
	}
	if (!ok || mapper->fDisabled)
	{
		IOService::getPlatform()->removeProperty(kIOPlatformMapperPresentKey);
		IOMapper::setMapperRequired(false);
	}
}

void
AppleVTD::installInterrupts(void)
{
	AppleVTD   * vtd;
	vtd_unit_t * unit;
	uint32_t     idx;

	if (!(vtd = OSDynamicCast(AppleVTD, IOMapper::gSystem))) return;
	for (idx = 0; (unit = vtd->units[idx]); idx++) unit_interrupts_enable(unit);
}

void
AppleVTD::adjustDevice(IOService * device)
{
	AppleVTDDeviceMapper * mapper;
	IOPCIDevice          * pciDevice;
    uint32_t               vidDidDev = 0;
    char                   devMapArg[kIOPCIDeviceMapArgLen+1];
    OSData               * data;
    uint32_t               mapperSelection = kIOPCIMapperSelectionSystem;
    
	if (!IOMapper::gSystem)                                return;
	if (!device->getDeviceMemoryCount())                   return;
	if (!(pciDevice = OSDynamicCast(IOPCIDevice, device))) return;

    // If we've already changed the mapper we're done
    if ((data = OSDynamicCast(OSData, pciDevice->getProperty("iommu-selection")))) {
        mapperSelection = (*((uint32_t *)data->getBytesNoCopy()));
        if (mapperSelection & kIOPCIMapperSelectionChanged) {
            return;
        }
    }

    // If the child driver had IOPCIUseDeviceMapper set, it wants to opt-in
    if (pciDevice->getProperty(kIOPCIUseDeviceMapperKey)) {
        mapperSelection |= kIOPCIMapperSelectionBundlePlist;
    }
    
    // Check if the boot arg is set to enable for this vid/did or bundle ID
    if (PE_parse_boot_argn("pci-dev-mapper", &devMapArg, kIOPCIDeviceMapArgLen)) {
        vidDidDev = pciDevice->savedConfig[kIOPCIConfigVendorID >> 2];
        
        OSString *id = OSDynamicCast(OSString, pciDevice->getProperty(kIOPCIChildBundleIdentifierKey));

        devMapArg[strlen(devMapArg)+1] = '\0';
        devMapArg[strlen(devMapArg)] = ',';
        char *tok = devMapArg;

        for (char *p = devMapArg; *p != '\0'; p++) {
            if (*p == ',') {
                *p = '\0';
                
                if (strlen(tok) == 9 && tok[4] == ':' && vidDidDev == (strtoul(tok, NULL, 16) | (strtoul(tok+5, NULL, 16) << 16))) {
                    mapperSelection |= kIOPCIMapperSelectionDeviceBootArg;
                    break;
                } else if (id != NULL && id->isEqualTo(tok)) {
                    mapperSelection |= kIOPCIMapperSelectionBundleBootArg;
                    break;
                }

                *p = ',';
                tok = p+1;
            }
        }
    }
    
    // Check if the PCI flags say we should use the device mapper for all devices
	if (kIOPCIConfiguratorDeviceMap & gIOPCIFlags) {
        mapperSelection |= kIOPCIMapperSelectionConfiguratorFlag;
    }
    
    // Check if the PCI flags say we should override and force the system mapper
	if (kIOPCIConfiguratorSystemMap & gIOPCIFlags) {
        mapperSelection = kIOPCIMapperSelectionSystem;
    }

	if ((kIOPCIClassGraphics == (pciDevice->savedConfig[kIOPCIConfigRevisionID >> 2] >> 24))
		&& (0 == pciDevice->space.s.busNum)
		&& (0x8086 == (pciDevice->savedConfig[kIOPCIConfigVendorID >> 2] & 0xffff))
	 && (!(kIOPCIConfiguratorIGIsMapped & gIOPCIFlags)))
	{
		device->setProperty(kIOMemoryDescriptorOptionsKey, kIOMemoryMapperNone, 32);
	}
	else if (mapperSelection != kIOPCIMapperSelectionSystem)
	{
		IOService * originator = IOPCIDeviceDMAOriginator(pciDevice);

        IOLockLock(gAppleVTDDeviceMapperLock);
        mapper = (typeof(mapper)) originator->copyProperty("iommu-parent");
        if (!mapper)
		{
			mapper = AppleVTDDeviceMapper::forDevice(originator, 0);
            VTLOG("%s->%s: mapper %p\n", pciDevice->getName(), originator->getName(), mapper);
            if (mapper) originator->setProperty("iommu-parent", mapper);
        }
		if (mapper)
		{
            mapperSelection |= kIOPCIMapperSelectionChanged;
			pciDevice->setProperty("iommu-parent", mapper);
			mapper->release();
		}
        IOLockUnlock(gAppleVTDDeviceMapperLock);
	}

    pciDevice->setProperty("iommu-selection", &mapperSelection, sizeof(mapperSelection));
}

void
AppleVTD::removeDevice(IOService * device)
{
	AppleVTDDeviceMapper * mapper;
	AppleVTD             * vtd;
	OSObject             * obj;

	if (!(vtd = OSDynamicCast(AppleVTD, IOMapper::gSystem))) return;

	obj = device->copyProperty("iommu-parent");
	if ((mapper = OSDynamicCast(AppleVTDDeviceMapper, obj)))
	{
		device->removeProperty("iommu-parent");
        vtd->deviceMapperActivate(mapper, kDeviceMapperDeactivate);
	}

	if (obj) obj->release();
}

void
AppleVTD::relocateDevice(IOService * device, bool paused)
{
	AppleVTDDeviceMapper * mapper;
	AppleVTD             * vtd;
	OSObject             * obj;

	if (!(vtd = OSDynamicCast(AppleVTD, IOMapper::gSystem))) return;

	obj = device->copyProperty("iommu-parent");
	if ((mapper = OSDynamicCast(AppleVTDDeviceMapper, obj)))
	{
        vtd->deviceMapperActivate(mapper, paused ? kDeviceMapperPause : kDeviceMapperUnpause);
	}

	if (obj) obj->release();
}

bool 
AppleVTD::init(IOWorkLoop * wl, const OSData * data)
{
	uint32_t unitIdx;
	bool x2apic_mode_disabled_on_at_least_one_unit = false;

	if (!super::init()) return (false);

	data->retain();
	fDMARData = data;
	wl->retain();
	fWorkLoop = wl;
	fCacheLineSize = cpuid_info()->cache_linesize;
	x2apic_mode = false;

	ACPI_TABLE_DMAR *           dmar = (typeof(dmar))      data->getBytesNoCopy();
	ACPI_DMAR_HEADER *          dmarEnd = (typeof(dmarEnd))(((uintptr_t) dmar) + data->getLength());
	ACPI_DMAR_HEADER *          hdr = (typeof(hdr))      (dmar + 1);
	ACPI_DMAR_HARDWARE_UNIT *   unit;

	VTLOG("DMAR Width %x, Flags %x\n", dmar->Width, dmar->Flags);

	for (unitIdx = 0; hdr < dmarEnd;
			hdr = (typeof(hdr))(((uintptr_t) hdr) + hdr->Length))
	{
		switch (hdr->Type)
		{
			case ACPI_DMAR_TYPE_HARDWARE_UNIT:
				unit = (typeof(unit)) hdr;
				if ((units[unitIdx] = unit_init(unit))) {
					/* If one enables x2apic mode, all units must */
					if (units[unitIdx]->x2apic_mode == 0) {
						x2apic_mode_disabled_on_at_least_one_unit = true;
					} else {
						x2apic_mode = true;
					}
					unitIdx++;
				}
				break;
		}
	}

	if (x2apic_mode && x2apic_mode_disabled_on_at_least_one_unit) {
		VTLOG("x2APIC mode enabled on at least one IRE, but disabled on other(s)!\n");
	}

	fNumMemoryRanges = 0;
	fSpace = 0;
	fAMDSpace = NULL;
	fAMDMapperLock = NULL;

	return (unitIdx != 0);
}

void AppleVTD::free()
{
	uint32_t idx;
	vtd_unit_t * unit;

	for (idx = 0; (unit = units[idx]); idx++)
	{
		if (unit->qi_table_stamps) {
			IOFreeData(unit->qi_table_stamps, sizeof(uint32_t) * kQIPageCount * 256);
			unit->qi_table_stamps = NULL;
		}
		IOFreeType(unit, vtd_unit_t);
		units[idx] = NULL;
	}

	OSSafeReleaseNULL(fDMARData);
	OSSafeReleaseNULL(fWorkLoop);

	super::free();
}

void
AppleVTD::space_destroy(vtd_space_t * bf)
{
	IOReturn    kr;
    vtd_vaddr_t count, start;
    uint32_t    present, missing;

	VTHWLOCK(fHWLock);
	// Flush free_queue entries belonging to this space to avoid a race
	// where another thread calls checkFree() while/after this space is
	// freed.
	while (bf->pending_free > 0)
	{
		for (int i = 0; i < kFreeQCount; i++)
		{
			checkFree(bf, i);
		}
	}

	if (bf->domain) vtd_bitmap_bitset(fDomainBitmap, false, bf->domain);
	VTHWUNLOCK(fHWLock);

	vtd_rballocator_free(bf);
	vtassert(0 == bf->rentries);

	if (bf->table_map && bf->table_bitmap)
	{
		count = (bf->vsize >> 9);
		start = 0;
		while (count)
		{
			present = vtd_bitmap_count(bf->table_bitmap, true, start, count);
			vtassert(count >= present);
			if (present)
			{
				kr = bf->table_map->wireRange(kIODirectionNone, ptoa_64(start), ptoa_64(present));
				vtassert(kr == KERN_SUCCESS);
				count -= present;
				start += present;
				if (!count) break;
			}
			missing = vtd_bitmap_count(bf->table_bitmap, false, start, count);
			vtassert(missing);
			vtassert(count >= missing);
			count -= missing;
			start += missing;
		}
		kr = bf->table_map->wireRange(kIODirectionNone, bf->levels_wired,
										bf->table_map->getLength() - bf->levels_wired);
		vtassert(kr == KERN_SUCCESS);
	}
	if (bf->table_map)
	{
		bf->table_map->release();
	    bf->table_map = 0;
	}
	if (bf->table_bitmap)
	{
	    vtd_bitmap_free(bf->table_bitmap);
	    bf->table_bitmap = 0;
	}
	if (bf->block)
	{
		IOSimpleLockFree(bf->block);
		bf->block = 0;
	}
	if (bf->rlock)
	{
		IOLockFree(bf->rlock);
		bf->rlock = 0;
	}
	IOFreeType(bf, vtd_space_t);
}

vtd_space_t *
AppleVTD::space_create(ppnum_t vsize, uint32_t buddybits, ppnum_t rsize)
{
	IOBufferMemoryDescriptor * md;
	vtd_space_t              * bf;
	vtd_table_entry_t        * table;
	IOReturn 	               kr;
	mach_vm_size_t 	           alloc;
	uint32_t 	               count;
	uint32_t 	               level;
	uint32_t 	               bit;
    uint32_t 	               treebits;
    uint32_t 	               domain;
    bool                       ok;

	treebits = fTreeBits;
	vtassert(vsize >= (1U << buddybits));
	vtassert(vsize > rsize);
	vtassert(!buddybits || (buddybits > (9 + 3)));
	vtassert(treebits > 12);

	bf = IOMallocType(vtd_space_t);
	if (!bf) return (NULL);

	ok = false;
	do
	{
		bf->rlock = IOLockAlloc();
		if (!bf->rlock) break;
		bf->cachelinesize = fCacheLineSize;

		VTHWLOCK(fHWLock);
		domain = vtd_bitmap_count(fDomainBitmap, true, 0, fDomainSize);
		if (domain != fDomainSize) vtd_bitmap_bitset(fDomainBitmap, true, domain);
		VTHWUNLOCK(fHWLock);

		if (domain == fDomainSize) break;
		bf->domain = domain;

		treebits -= 12;
		vsize = (vsize + 511) & ~511;
		bf->vsize = vsize;
		bf->table_bitmap = vtd_bitmap_alloc(vsize >> 9);
		if (!bf->table_bitmap) break;

		alloc = 0;
		level = 0;
		bit   = 0;
		while (bit < treebits)
		{
			count = (vsize >> bit);
			if (!count) count = 1;
			alloc += round_page_64(count * sizeof(vtd_table_entry_t));
			bit += 9;
			level++;
		}
		bf->max_level = level - 1;

		VTLOG("domain %d, level %d, bmd...0x%llx\n", bf->domain, bf->max_level, alloc);
		md = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
							kIOMemoryPageable |
							kIOMapWriteCombineCache |
							kIOMemoryMapperNone,
							alloc, page_size);
		VTLOG("bmd %p\n", md);
		if (!md) break;

		bf->table_map = md->map();
		md->release();
		if (!bf->table_map) break;

		table = (typeof(table)) bf->table_map->getVirtualAddress();

		vtd_table_entry_t * prior = NULL;
		vtd_table_entry_t * next = table;
		mach_vm_size_t      offset;
		uint32_t idx;

		level = 0;
		bit   = 0;
		while (bit < treebits)
		{
			count = (vsize >> bit);
			if (!count) count = 1;

			vtassert(level < arrayCount(bf->tables));
			vtassert(level <= bf->max_level);
			bf->tables[level] = next;
			if (level == 1)
			{
				// wire levels >0
				offset = ((next - table) * sizeof(vtd_table_entry_t));
				VTLOG("wire [%llx, %llx]\n", offset, alloc);
				bf->levels_wired = offset;
				kr = bf->table_map->wireRange(kIODirectionOutIn, offset, alloc - offset);
				vtassert(KERN_SUCCESS == kr);
				STAT_ADD(bf, tables, atop_64(alloc - offset));
				if (KERN_SUCCESS != kr)
				{
					bf->table_map->release();
					return (NULL);
				}
			}
			else if (level >= 2)
			{
				for (idx = 0; idx < count; idx++)
				{
					ppnum_t lvl2page = pmap_find_phys(kernel_pmap, (uintptr_t) &prior[idx << 9]);
					if (!lvl2page) panic("!lvl2page");
					VTLOG("lvl2 %p[%x] = %p\n", next, idx, &prior[idx << 9]);
					next[idx].bits = (kPageAccess | ptoa_64(lvl2page));
				}
			}
			prior = next;
			next = next + ((count + 511) & ~511);
			bit += 9;
			level++;
		}
		table_flush(&bf->tables[1][0], alloc - offset, bf->cachelinesize);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

		IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) &bf->tables[0][0], page_size, kIOCopybackCache);

#pragma clang diagnostic pop

		VTLOG("tables %p, %p, %p, %p, %p, %p\n", bf->tables[0], bf->tables[1], bf->tables[2], 
													bf->tables[3], bf->tables[4], bf->tables[5]);

		bf->root_page = pmap_find_phys(kernel_pmap, (uintptr_t) bf->tables[bf->max_level]);
		if (!bf->root_page) panic("!root_page");
		VTLOG("tree root 0x%llx\n", ptoa_64(bf->root_page));

		if (buddybits)
		{
			bf->block = IOSimpleLockAlloc();
			if (!bf->block) break;
			vtd_ballocator_init(bf, buddybits);
		}
		bf->rsize = rsize;
		vtd_rballocator_init(bf, rsize, vsize - rsize);
		STAT_ADD(bf, vsize, vsize);
		ok = true;
	}
	while (false);
	if (!ok)
	{
		space_destroy(bf);
		bf = NULL;
	}
	return (bf);
}

vtd_baddr_t
AppleVTD::space_alloc(vtd_space_t * bf, vtd_vaddr_t addr, vtd_vaddr_t size,
					  uint32_t mapOptions, const IODMAMapSpecification * mapSpecification,
					  const upl_page_info_t * pageList)
{
    vtd_vaddr_t align = 1;
    vtd_baddr_t largethresh;
    bool        uselarge;
	uint32_t    list;

	if ((kIODMAMapPagingPath & mapOptions) && (size > bf->stats.largest_paging))
		bf->stats.largest_paging = size;

	list = vtd_log2up(size);

	if (mapSpecification)
	{
		if (mapSpecification->numAddressBits 
			&& (mapSpecification->numAddressBits <= 32)
			&& (size > bf->stats.largest_32b))		bf->stats.largest_32b = size;

		if (mapSpecification->alignment > page_size) align = static_cast<vtd_vaddr_t>(atop_64(mapSpecification->alignment));
	}

	if (!bf->block)
	{
		uselarge = true;
	}
	else
	{
		if (bf->stats.bused >= kBPagesReserve)
		{
			largethresh = 1;
		}
		else if (bf->stats.bused >= kBPagesSafe)
		{
			largethresh = kLargeThresh2;
		}
		else
		{
			largethresh = kLargeThresh;
		}

		if (!(kIODMAMapPagingPath & mapOptions)
			&& (size >= largethresh)
			&& mapSpecification
			&& mapSpecification->numAddressBits
			&& ((1ULL << (mapSpecification->numAddressBits - 12)) >= bf->vsize))
		{
			uselarge = true;
		}
		else
		{
			uselarge = false;
			if (align > size) size = align;
		}
	}

#if !FREE_ON_FREE
	VTHWLOCK(fHWLock);
	checkFree(bf, uselarge);
	VTHWUNLOCK(fHWLock);
#endif

	do
	{
		if (uselarge)
		{
			vtd_rbaddr_t hwalign, hwalignsize;

			IOLockLock(bf->rlock);
			if (kIODMAMapFixedAddress & mapOptions)
			{
				hwalignsize = size;
				addr = vtd_rballoc_fixed(bf, addr, hwalignsize);
			}
			else
			{
				hwalign = vtd_log2up(size);
				if (hwalign > fMaxRoundSize) hwalign = fMaxRoundSize;
				hwalign = (1 << hwalign);
				if (align < hwalign) align = hwalign;
				hwalign--;
				hwalignsize = (size + hwalign) & ~hwalign;

				addr = vtd_rballoc(bf, hwalignsize, align, mapOptions, pageList);
			}
			STAT_ADD(bf, allocs[list], 1);
			if (addr)
			{
				STAT_ADD(bf, rused, hwalignsize);
#if RBCHECK
				vtd_rbaddr_t checksize = vtd_rbtotal(bf);
				if (checksize != (bf->stats.vsize - bf->rsize - bf->stats.rused))
				{
				    panic("checksize 0x%x, vsize 0x%x, rsize 0x%x, rused 0x%x",
			                checksize, bf->stats.vsize, bf->rsize, bf->stats.rused);
				}
#endif /* RBCHECK */
				vtd_space_fault(bf, addr, size);
			}
			IOLockUnlock(bf->rlock);
			if (addr && pageList) vtd_space_set(bf, addr, size, mapOptions, pageList);
		}
		else
		{
			vtassert(!(kIODMAMapFixedAddress & mapOptions));
			BLOCK(bf->block);
			addr = vtd_balloc(bf, size, mapOptions, pageList);
			__mfence(); // the VTD table has relaxed consistency
			STAT_ADD(bf, allocs[list], 1);
			if (addr) STAT_ADD(bf, bused, (1 << list));
			BUNLOCK(bf->block);
		}
		if (addr)                                            break;
		if (!uselarge && (size >= (1 << (kBPagesLog2 - 2)))) break;
		if (kIODMAMapFixedAddress & mapOptions)              break;

		IOLockLock(bf->rlock);
		bf->waiting_space = true;
		IOLockSleep(bf->rlock, &bf->waiting_space, THREAD_UNINT);
		IOLockUnlock(bf->rlock);
//		IOLog("AppleVTD: waiting space (%d)\n", size);
		VTLOG("AppleVTD: waiting space (%d, bused %d, rused %d)\n",
				size, bf->stats.bused, bf->stats.rused);
	}
	while (true);

	return (addr);
}

void 
AppleVTD::space_free(vtd_space_t * bf, vtd_baddr_t addr, vtd_baddr_t size)
{
	uint32_t list;

	vtassert(addr);
	vtassert((addr + size) <= bf->vsize);

	if (addr >= bf->rsize)
	{
		vtd_rbaddr_t hwalign, hwalignsize;

		hwalign = vtd_log2up(size);
		if (hwalign > fMaxRoundSize) hwalign = fMaxRoundSize;
		hwalign = (1 << hwalign);
		hwalign--;
		hwalignsize = (size + hwalign) & ~hwalign;

		IOLockLock(bf->rlock);

		vtd_rbfree(bf, addr, hwalignsize);
		STAT_ADD(bf, rused, -hwalignsize);

#if RBCHECK
		vtd_rbaddr_t checksize = vtd_rbtotal(bf);
		if (checksize != (bf->stats.vsize - bf->rsize - bf->stats.rused))
		{
		    panic("checksize 0x%x, vsize 0x%x, rsize 0x%x, rused 0x%x",
	                checksize, bf->stats.vsize, bf->rsize, bf->stats.rused);
		}
#endif /* RBCHECK */
		IOLockUnlock(bf->rlock);
	}
	else
	{
		list = vtd_log2up(size);
		BLOCK(bf->block);
		vtd_bfree(bf, addr, size);
		__mfence(); // the VTD table has relaxed consistency
		STAT_ADD(bf, bused, -(1 << list));
		BUNLOCK(bf->block);
	}

	if (bf->waiting_space)
	{
		IOLockLock(bf->rlock);
		bf->waiting_space = false;
		IOLockWakeup(bf->rlock, &bf->waiting_space, false);
		IOLockUnlock(bf->rlock);
	}
}

void 
AppleVTD::space_alloc_fixed(vtd_space_t * bf, vtd_baddr_t addr, vtd_baddr_t size, bool fault)
{
	if (bf->block)
	{
		BLOCK(bf->block);
		vtd_balloc_fixed(bf, addr, size);
		BUNLOCK(bf->block);
	}
	IOLockLock(bf->rlock);
	vtd_rballoc_fixed(bf, addr, size);
	if (fault)
		vtd_space_fault(bf, addr, size);
	IOLockUnlock(bf->rlock);
}

static page_entry_t __unused
vtd_tree_read(page_entry_t root, uint32_t width, addr64_t addr)
{
	page_entry_t entry = root;
	page_entry_t table;
	uint32_t index;
	uint32_t level = 0;

	while (width > 12)
	{	
		width -= 9;
		index = (addr >> (width - 3)) & (511 << 3);

		table = entry & kPageAddrMask;
		entry = ml_phys_read_double_64(table + index);

		if (!(kPageAccess & entry)) break;
		level++;
	}

	return (entry);
}

void
AppleVTD::addMemoryRange(IOPhysicalAddress start, IOPhysicalLength length)
{
	AppleVTD   * vtd;

	if (!(vtd = OSDynamicCast(AppleVTD, IOMapper::gSystem)))
	{
		IOLog("[%s()] AppleVTD is not yet installed as gSystem\n", __func__);
		return;
	}

	return vtd->fWorkLoop->runActionBlock(^IOReturn
	{
		if (vtd->fNumMemoryRanges == kMaxMemRegions)
		{
			IOLog("[%s()] Mem regions limit reached\n", __func__);
			return kIOReturnError;
		}

		vtd->fMemoryRangeBase[vtd->fNumMemoryRanges] = start;
		vtd->fMemoryRangeLen[vtd->fNumMemoryRanges] = length;

		if (vtd->fSpace)
		{
			uint64_t idx = vtd->fNumMemoryRanges;
			uint64_t addr = vtd->fMemoryRangeBase[idx];
			uint32_t count = atop_32(vtd->fMemoryRangeLen[idx]);

			VTLOG("[%s(%p)] Freeing 0x%llx->0x%llx\n", __func__, vtd->fSpace, vtd->fMemoryRangeBase[idx], vtd->fMemoryRangeBase[idx] + vtd->fMemoryRangeLen[idx]);
			vtd->space_alloc_fixed(vtd->fSpace, static_cast<vtd_baddr_t>(atop_64(vtd->fMemoryRangeBase[idx])), count, false);
		}

		vtd->fNumMemoryRanges++;

		return kIOReturnSuccess;
	});
}

void
AppleVTD::reserveRanges(vtd_space_t * bf, bool systemMapper)
{
	uint32_t idx;

	space_alloc_fixed(bf, atop_64(0xfee00000), atop_64(0xfef00000-0xfee00000), systemMapper);
	bf->tables[0][atop_64(0xfee00000)].bits = 0xfee00000 | kPageAccess;

	ACPI_TABLE_DMAR *           dmar = (typeof(dmar))      fDMARData->getBytesNoCopy();
	ACPI_DMAR_HEADER *          dmarEnd = (typeof(dmarEnd))(((uintptr_t) dmar) + fDMARData->getLength());
	ACPI_DMAR_HEADER *          hdr = (typeof(hdr))      (dmar + 1);
	ACPI_DMAR_RESERVED_MEMORY * mem;

	for (; hdr < dmarEnd;
			hdr = (typeof(hdr))(((uintptr_t) hdr) + hdr->Length))
	{
		uint64_t addr;
		uint32_t count;
		switch (hdr->Type)
		{
			case ACPI_DMAR_TYPE_RESERVED_MEMORY:
				mem = (typeof(mem)) hdr;
				VTLOG("ACPI_DMAR_TYPE_RESERVED_MEMORY 0x%llx, 0x%llx\n",
					mem->BaseAddress, mem->EndAddress);

				addr = mem->BaseAddress;
				count = atop_32(mem->EndAddress - addr);

				space_alloc_fixed(bf, static_cast<vtd_baddr_t>(atop_64(addr)), count, systemMapper);
				for (; count; addr += page_size, count--)
				{
					bf->tables[0][atop_64(addr)].bits = (addr | kPageAccess);
				}
				break;
		}
	}

	for (idx = 0; idx < fNumMemoryRanges; idx++)
	{
		uint64_t addr = fMemoryRangeBase[idx];
		uint32_t count = atop_32(fMemoryRangeLen[idx]);

		VTLOG("[%s(%p)] Freeing 0x%llx->0x%llx\n", __func__, bf, fMemoryRangeBase[idx], fMemoryRangeBase[idx] + fMemoryRangeLen[idx]);
		space_alloc_fixed(bf, static_cast<vtd_baddr_t>(atop_64(fMemoryRangeBase[idx])), count, false);
	}
}

bool
AppleVTD::initHardware(IOService *provider)
{
	IOBufferMemoryDescriptor * md;
    OSData                   * data;
	vtd_unit_t               * unit;
    context_entry_t          * context;
	IOReturn	kr;
    ppnum_t		context_page;
	uint32_t	idx;
    ppnum_t		stamp_page;
	uint64_t	msiAddress;
	uint32_t	msiData;
	bool        mapInterrupts;

	fDisabled     = (!IOService::getPlatform()->getProperty(kIOPlatformMapperPresentKey));
	fIsSystem     = !fDisabled;
	mapInterrupts = (fIsSystem && (0 != (kIOPCIConfiguratorMapInterrupts & gIOPCIFlags)));
	free_mask     = (kFreeQElems - 1);

	fTreeBits = 0;
	// prefer smallest tree?
	for (idx = 0; (unit = units[idx]); idx++)
	{
		if (!unit->intmapper)   mapInterrupts = false;
		if (!unit->translating) continue;

		for (fContextWidth = kAddressWidth30;
				(fContextWidth <= kAddressWidth64);
				fContextWidth++)
		{
			if ((0x100 << fContextWidth) & unit->regs->capability)
			{
				fTreeBits = (30 + 9 * static_cast<uint32_t>(fContextWidth));  // (57+9) for 64
				break;
			}
		}
		break;
	}

    fDomainSize = 512;

	for (idx = 0; (unit = units[idx]); idx++)
	{	
		if (!unit->translating) continue;
		if (unit->domains < fDomainSize) fDomainSize = unit->domains;

		if (!((0x100 << fContextWidth) & unit->regs->capability))
			panic("!tree bits %d on unit %d", fTreeBits, idx);
		if (unit->selective && ((unit->rounding > fMaxRoundSize)))
			fMaxRoundSize = unit->rounding;
	}

	VTLOG("domains %d, contextwidth %lld, treebits %d, round %d\n",
			fDomainSize, fContextWidth, fTreeBits, fMaxRoundSize);

    // need better legacy checks
	if (!fMaxRoundSize)                                                                              return (false);
	if (!((CPUID_LEAF7_FEATURE_SMEP|CPUID_LEAF7_FEATURE_SMAP) & cpuid_info()->cpuid_leaf7_features)) return (false);
	//

	fHWLock = VTHWLOCKALLOC();

	fDomainBitmap = vtd_bitmap_alloc(fDomainSize);
	vtd_bitmap_bitset(fDomainBitmap, true, 0);

	fSpace = space_create(kVPages, kBPagesLog2, kRPages);
	if (!fSpace) return (false);
	VTLOG("bsize 0x%x, bsafe 0x%x, breserve 0x%x, rsize 0x%x\n",
	        (1<<kBPagesLog2), kBPagesSafe, kBPagesReserve, fSpace->rsize);

	data = OSData::withBytesNoCopy(&fSpace->stats, sizeof(fSpace->stats));
	if (data)
	{
		setProperty("stats", data);
		data->release();
	}

	reserveRanges(fSpace, true);

	md = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
						kIOMemoryPageable |
						kIOMapWriteCombineCache |
						kIOMemoryMapperNone,
#ifdef BLOCK_DEVICE
						3 * page_size,
#else
						2 * page_size,
#endif
						page_size);
	vtassert(md);
	if (!md) return (kIOReturnNoMemory);

	kr = md->prepare(kIODirectionOutIn);
	vtassert(KERN_SUCCESS == kr);

	fGlobalContextMap = md->map();
	vtassert(fGlobalContextMap);
	md->release();

    // context entries

	fContextMaps = IONew(typeof(fContextMaps[0]), 256);
	vtassert(fContextMaps);
	context = (typeof(context)) fGlobalContextMap->getVirtualAddress();

	for (idx = 0; idx < 256; idx++)
	{
		fContextMaps[idx] = fGlobalContextMap;

		context[idx].address_space_root = ptoa_64(fSpace->root_page)
										| kEntryPresent
										| kTranslationType0;
		context[idx].context_entry      = fContextWidth
										| fSpace->domain*kDomainIdentifier1;
	}
	context_page = pmap_find_phys(kernel_pmap, (uintptr_t) &context[0]);
	if (!context_page) panic("!context_page");

	// root

	fRootEntryTable = (typeof(fRootEntryTable)) (fGlobalContextMap->getAddress() + page_size);
	for (idx = 0; idx < 256; idx++)
	{
		fRootEntryTable[idx].context_entry_ptr = ptoa_64(context_page) | kEntryPresent;
		fRootEntryTable[idx].resv = 0;
	}

#ifdef BLOCK_DEVICE
	{
		enum { bus = 5, dev = 7, fun = 0 };
		context = (typeof(context)) (fGlobalContextMap->getAddress() + 2 * page_size);
		for (idx = 0; idx < 256; idx++)
		{
			context[idx].address_space_root = ptoa_64(fSpace->root_page)
											| kEntryPresent
											| kTranslationType0;
			context[idx].context_entry      = fContextWidth
											| fSpace->domain*kDomainIdentifier1;
		}
		context_page = pmap_find_phys(kernel_pmap, (uintptr_t) &context[0]);
		if (!context_page) panic("!context_page");
		fRootEntryTable[bus].context_entry_ptr = ptoa_64(context_page) | kEntryPresent;
		context[(dev << 3) | (fun << 0)].address_space_root = 0;
	}
#endif

	fRootEntryPage = pmap_find_phys(kernel_pmap, (uintptr_t) &fRootEntryTable[0]);
	if (!fRootEntryPage) panic("!fRootEntryPage");
	for (idx = 0; (unit = units[idx]); idx++) 
	{
		unit->root = ptoa_64(fRootEntryPage);
	}

    if (mapInterrupts)
    {
		md = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
							kIOMemoryHostPhysicallyContiguous |
							kIOMapWriteCombineCache |
							kIOMemoryMapperNone,
							kIRPageCount * page_size, page_size);
		vtassert(md);
		if (!md) return (kIOReturnNoMemory);

		kr = md->prepare(kIODirectionOutIn);
		vtassert(KERN_SUCCESS == kr);

		fIRMap = md->map();
		vtassert(fIRMap);
		md->release();

		fIRTable = (typeof(fIRTable)) (fIRMap->getAddress());
		fIRAddress = (vtd_log2down(kIRCount) - 1)
						 | md->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
		VTLOG("ir p 0x%qx v %p\n", fIRAddress, fIRTable);
		for (idx = 0; (unit = units[idx]); idx++)
		{
			unit->ir_address = fIRAddress;
		}
		bzero(fIRTable, kIRPageCount * page_size);
		__mfence();
    }

	// QI

	for (idx = 0; (unit = units[idx]); idx++) 
	{
		fQIStamp[idx] = 0x100;

		md = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
							kIOMemoryHostPhysicallyContiguous |
							kIOMapWriteCombineCache |
							kIOMemoryMapperNone,
							kQIPageCount * page_size, page_size);
		vtassert(md);
		if (!md) return (kIOReturnNoMemory);
	
		kr = md->prepare(kIODirectionOutIn);
		vtassert(KERN_SUCCESS == kr);
	
		unit->qi_map = md->map();
		vtassert(unit->qi_map);
		unit->qi_mask    = kQIIndexMask;
		unit->qi_table   = (typeof(unit->qi_table)) (unit->qi_map->getAddress());
		unit->qi_address = vtd_log2down(kQIPageCount)
					     | md->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);

		unit->qi_table_stamps = (uint32_t *)IOMallocZeroData(sizeof(uint32_t) * kQIPageCount * 256);
		vtassert(unit->qi_table_stamps);

		stamp_page = pmap_find_phys(kernel_pmap, (uintptr_t) &unit->qi_stamp);
		vtassert(stamp_page);
		unit->qi_stamp_address = ptoa_64(stamp_page) | (page_mask & ((uintptr_t) &unit->qi_stamp));

		md->release();
    }

	// interrupts, timers

	kr = gIOPCIMessagedInterruptController->allocateDeviceInterrupts(
													this, 2, 0, &msiAddress, &msiData);
	if (kIOReturnSuccess == kr)
	{
        fIntES = IOInterruptEventSource::interruptEventSource(
                      this,
                      OSMemberFunctionCast(IOInterruptEventSource::Action,
                                            this, &AppleVTD::handleInterrupt),
                      this, 0);
		if (fIntES) fWorkLoop->addEventSource(fIntES);
        fFaultES = IOInterruptEventSource::interruptEventSource(
                      this,
                      OSMemberFunctionCast(IOInterruptEventSource::Action,
                                            this, &AppleVTD::handleFault),
                      this, 1);
		if (fFaultES) fWorkLoop->addEventSource(fFaultES);
	}


	fTimerES = IOTimerEventSource::timerEventSource(this, 
	                      OSMemberFunctionCast(IOTimerEventSource::Action,
												this, &AppleVTD::timer));
	if (fTimerES) fWorkLoop->addEventSource(fTimerES);

	if (!fIntES || !fFaultES) msiData = msiAddress = 0;

	__mfence();
	for (idx = 0; (unit = units[idx]); idx++) 
	{
		unit->msi_data    = msiData & 0xff;
		unit->msi_address = msiAddress;
		unit_enable(unit, fQIStamp[idx]);
	}
	contextInvalidate(0);
	interruptInvalidate(0, 256);
	if (fDisabled) for (idx = 0; (unit = units[idx]); idx++) unit_quiesce(unit);

	if (fIntES)   fIntES->enable();
	if (fFaultES) fFaultES->enable();
	
//	fTimerES->setTimeoutMS(10);

	setProperty(kIOPlatformQuiesceActionKey, INT32_MAX - 1000, 64);
	setProperty(kIOPlatformActiveActionKey, INT32_MAX - 1000, 64);

	if (!fDisabled)	registerService();

	return (true);
}

enum
{
    /* Redirection Table Entries */
    kRTLOVectorNumberMask           = 0x000000FF,
    kRTLOVectorNumberShift          = 0,

    kRTLODeliveryModeMask           = 0x00000700,
    kRTLODeliveryModeShift          = 8,
    kRTLODeliveryModeFixed          = 0 << kRTLODeliveryModeShift,
    kRTLODeliveryModeLowestPriority = 1 << kRTLODeliveryModeShift,
    kRTLODeliveryModeSMI            = 2 << kRTLODeliveryModeShift,
    kRTLODeliveryModeNMI            = 4 << kRTLODeliveryModeShift,
    kRTLODeliveryModeINIT           = 5 << kRTLODeliveryModeShift,
    kRTLODeliveryModeExtINT         = 7 << kRTLODeliveryModeShift,

    kRTLODestinationModeMask        = 0x00000800,
    kRTLODestinationModeShift       = 11,
    kRTLODestinationModePhysical    = 0 << kRTLODestinationModeShift,
    kRTLODestinationModeLogical     = 1 << kRTLODestinationModeShift,

    kRTLODeliveryStatusMask         = 0x00001000,
    kRTLODeliveryStatusShift        = 12,

    kRTLOInputPolarityMask          = 0x00002000,
    kRTLOInputPolarityShift         = 13,
    kRTLOInputPolarityHigh          = 0 << kRTLOInputPolarityShift,
    kRTLOInputPolarityLow           = 1 << kRTLOInputPolarityShift,

    kRTLORemoteIRRMask              = 0x00004000,
    kRTLORemoteIRRShift             = 14,

    kRTLOTriggerModeMask            = 0x00008000,
    kRTLOTriggerModeShift           = 15,
    kRTLOTriggerModeEdge            = 0 << kRTLOTriggerModeShift,
    kRTLOTriggerModeLevel           = 1 << kRTLOTriggerModeShift,

    kRTLOMaskMask                   = 0x00010000,
    kRTLOMaskShift                  = 16,
    kRTLOMaskEnabled                = 0,
    kRTLOMaskDisabled               = kRTLOMaskMask,

    kRTHIExtendedDestinationIDMask  = 0x00FF0000,
    kRTHIExtendedDestinationIDShift = 16,

    kRTHIDestinationMask            = 0xFF000000,
    kRTHIDestinationShift           = 24
};

uint64_t
IOPCISetAPICInterrupt(uint64_t entry)
{
	AppleVTD * vtd;
	uint64_t   vector;
	uint64_t   destID;
	uint64_t   destMode;
	uint64_t   triggerMode;
	uint64_t   irte;
	uint64_t   prior;

	if (!(vtd = OSDynamicCast(AppleVTD, IOMapper::gSystem))) return (entry);
	if (!vtd->fIRTable)                                      return (entry);

	vector      = (kRTLOVectorNumberMask & entry);
	destMode    = ((kRTLODestinationModeMask & entry) >> kRTLODestinationModeShift);
	triggerMode = ((kRTLOTriggerModeMask & entry) >> kRTLOTriggerModeShift);
	destID      = ((kRTHIDestinationMask & (entry >> 32)) >> kRTHIDestinationShift);

	irte = (destID << 40)     // destID
		 | (vector << 16)	  // vector
		 | (0 << 5)           // fixed delivery mode
		 | (triggerMode << 4) // trigger
		 | (0 << 3)           // redir
		 | (destMode << 2)    // dest
		 | (1 << 1)           // faults ena
		 | (1 << 0);	      // present

    prior = vtd->fIRTable[vector].data;
	VTLOG("ir[0x%qx] 0x%qx -> 0x%qx\n", vector, prior, irte);
	if (irte != prior)
	{
		vtd->fIRTable[vector].data = irte;
		__mfence();
		vtd->interruptInvalidate(vector, 1);
    }

	entry &= ~(kRTLODeliveryModeMask
			 | kRTLODestinationModeMask
			 | (((uint64_t) kRTHIExtendedDestinationIDMask) << 32)
			 | (((uint64_t) kRTHIDestinationMask) << 32));
	entry |= (1ULL << 48);
	entry |= (vector << 49);

	return (entry);
}

IOReturn
IOPCISetMSIInterrupt(uint32_t vector, uint32_t count, uint32_t * msiData)
{
	AppleVTD * vtd;
	uint64_t   present;
	uint64_t   destVector;
	uint64_t   destID;
	uint64_t   levelTrigger;
	uint64_t   irte;
	uint64_t   prior;
    uint32_t   idx;
    bool       inval;

	if (!(vtd = OSDynamicCast(AppleVTD, IOMapper::gSystem))) return (kIOReturnUnsupported);
	if (!vtd->fIRTable)                                      return (kIOReturnUnsupported);

    if ((vector + count) > kIRCount) panic("IOPCISetMSIInterrupt(%d, %d)", vector, count);

    inval = false;
    for (idx = vector; idx < (vector + count); idx++)
    {
		extern int cpu_to_lapic[];
		int destShift;

		levelTrigger = 0;
		present      = (msiData != 0);
		destVector   = (0xFF & idx);
		destID       = (uint64_t)cpu_to_lapic[((idx & 0xFF00) >> 8)];
		/* In x2APIC mode, the destination starts at bit 32; legacy xAPIC: bit 40 */
		destShift    = vtd->x2apic_mode ? 32 : 40;
		irte = (destID << destShift)      // destID
			 | (destVector << 16)  // vector
			 | (0 << 5)            // fixed delivery mode
			 | (levelTrigger << 4) // trigger
			 | (0 << 3)            // redir
			 | (0 << 2)            // phys dest
			 | (1 << 1)            // faults ena
			 | (present << 0);	   // present

		prior = vtd->fIRTable[idx].data;
		VTLOG("ir[0x%x] 0x%qx -> 0x%qx\n", idx, prior, irte);

		// msi should only be set once and removed
		if (!(1 & (prior ^ irte))) panic("msi irte 0x%qx prior 0x%qx", irte, prior);

		if (irte != prior)
		{
			vtd->fIRTable[idx].data = irte;
			__mfence();
			if (1 & prior) inval = true;
		}
    }

    if (inval) vtd->interruptInvalidate(vector, count);

    if (msiData)
    {
		/* WARNING: If x2APIC is enabled, this MUST be in remappable format! */
		msiData[0] = 0xfee00000                	// addr lo
			   | ((vector & 0x7fff) << 5)	// handle[14:0]
			   | (1 << 4)  			// remap format
			   | (1 << 3)  			// SHV (add subhandle to vector)
			   | ((vector & 0x8000) >> 13); // b2 handle[15]

		msiData[1] = 0;         		// addr hi
		msiData[2] = 0;				// data (subhandle)
    }

    return (kIOReturnSuccess);
}

IOReturn
AppleVTD::handleInterrupt(IOInterruptEventSource * source, int count)
{
	uint32_t idx;
	vtd_unit_t * unit;

	VTHWLOCK(fHWLock);
	for (idx = 0; idx < kFreeQCount; idx++) checkFree(fSpace, idx);
	for (idx = 0; (unit = units[idx]); idx++) 
	{
		unit->regs->invalidation_completion_status = 1;
	}
	VTHWUNLOCK(fHWLock);

	return (kIOReturnSuccess);
}

IOReturn
AppleVTD::handleFault(IOInterruptEventSource * source, int count)
{
	uint32_t idx;
	vtd_unit_t * unit;

	for (idx = 0; (unit = units[idx]); idx++) unit_faults(unit, true || (idx != 0));

	return (kIOReturnSuccess);
}

IOReturn
AppleVTD::timer(OSObject * owner, IOTimerEventSource * sender)
{
	uint32_t idx;

	VTHWLOCK(fHWLock);
	for (idx = 0; idx < kFreeQCount; idx++) checkFree(fSpace, idx);
	VTHWUNLOCK(fHWLock);

	fTimerES->setTimeoutMS(10);

	return (kIOReturnSuccess);
}

IOReturn 
AppleVTD::callPlatformFunction(const OSSymbol * functionName,
							   bool waitForFunction,
							   void * param1, void * param2,
							   void * param3, void * param4)
{
    if (functionName)
    {
		uint32_t idx;
		vtd_unit_t * unit;
    	if (functionName->isEqualTo(gIOPlatformActiveActionKey))
		{
			for (idx = 0; (unit = units[idx]); idx++) 
			{
				unit_enable(unit, fQIStamp[idx]);
				unit_interrupts_enable(unit);
			}
			contextInvalidate(0);
			interruptInvalidate(0, 256);

			if (fDisabled) for (idx = 0; (unit = units[idx]); idx++) unit_quiesce(unit);

			return (kIOReturnSuccess);
		}
		else if (functionName->isEqualTo(gIOPlatformQuiesceActionKey))
		{
			for (idx = 0; (unit = units[idx]); idx++) 
			{
				unit_quiesce(unit);
			}
			return (kIOReturnSuccess);
		}
	}
    return (super::callPlatformFunction(functionName, waitForFunction,
                                        param1, param2, param3, param4));
}

IOReturn 
AppleVTD::spaceMapMemory(
			  vtd_space_t                 * space,
			  IOMemoryDescriptor          * memory,
			  uint64_t                      descriptorOffset,
			  uint64_t                      length,
			  uint32_t                      mapOptions,
			  const IODMAMapSpecification * mapSpecification,
			  IODMACommand                * dmaCommand,
			  const IODMAMapPageList      * pageList,
			  uint64_t                    * mapAddress,
			  uint64_t                    * mapLength)
{
	vtd_vaddr_t base;
	uint64_t pageCount;
	const upl_page_info_t * pageInfo;

	IOMDDMAWalkSegmentState  walkState;
	IOMDDMAWalkSegmentArgs * walkArgs = (IOMDDMAWalkSegmentArgs *) (void *)&walkState;
	IOOptionBits             mdOp;
	uint64_t                 index;
	IOPhysicalLength         segLen;
	uint64_t                 phys, align;

	uint64_t mapperPageMask;
	uint64_t mapperPageShift;
	uint64_t insertOffset;

    uint64_t mappedAddress;
    uint64_t mappedLength;
    uint64_t firstPageOffset;
    bool     discontig;
    IOReturn ret;

	mapperPageMask  = 4096 - 1;
	mapperPageShift = (64 - __builtin_clzll(mapperPageMask));
	mappedAddress = 0;
	pageInfo = NULL;
	firstPageOffset = 0;
	discontig = false;

	if (pageList)
	{
		firstPageOffset = pageList->pageOffset;
		pageCount       = pageList->pageListCount;
		pageInfo        = pageList->pageList;
		discontig       = (pageCount != atop_64(round_page_64(length + firstPageOffset)));
	}
	else if (memory)
	{
		walkArgs->fMapped = false;
		mdOp = kIOMDFirstSegment;
		pageCount = 0;

		for (index = 0; index < length; )
		{
			walkArgs->fOffset = descriptorOffset + index;
			ret = memory->dmaCommandOperation(mdOp, &walkState, sizeof(walkState));
			mdOp = kIOMDWalkSegments;
			if (ret != kIOReturnSuccess) break;
			phys = walkArgs->fIOVMAddr;
			segLen = walkArgs->fLength;
			if (segLen > (length - index)) {
				segLen = length - index;
			}
			align = (phys & mapperPageMask);
			if (index) {
				if (align) {
					discontig = true;
				}
				if (mapperPageMask & index) {
					discontig = true;
				}
			} else {
				firstPageOffset = align;
			}
			pageCount += ((align + segLen + mapperPageMask) >> mapperPageShift);
			index += segLen;
		}

		vtassert (index >= length);
		if (index < length) return (kIOReturnVMError);
	}
	else pageCount = ((length + mapperPageMask) >> mapperPageShift);

	if (kIODMAMapFixedAddress & mapOptions)
	{
		mappedAddress = *mapAddress;
		base = static_cast<vtd_vaddr_t>(mappedAddress >> mapperPageShift);
		if (firstPageOffset != (mappedAddress - (base << mapperPageShift))) return (kIOReturnNotAligned);
	}
	else base = 0;

	base = space_alloc(space, base, static_cast<vtd_vaddr_t>(pageCount), mapOptions, mapSpecification, pageInfo);

	if (!base) return (kIOReturnNoResources);
	vtassert((base + pageCount) <= space->vsize);

#if KP
	VTLOG("iovmMapMemory: (0x%x)=0x%x\n", length, (int)base);
#endif

	mappedAddress   = base;      mappedAddress <<= mapperPageShift;
	mappedLength    = pageCount; mappedLength  <<= mapperPageShift;

	if (memory && !pageInfo)
	{
		mdOp = kIOMDFirstSegment;
		for (insertOffset = 0, index = 0; index < length; )
		{
			walkArgs->fOffset = descriptorOffset + index;
			ret = memory->dmaCommandOperation(mdOp, &walkState, sizeof(walkState));
			mdOp = kIOMDWalkSegments;
			if (ret != kIOReturnSuccess) break;
			phys = walkArgs->fIOVMAddr;
			segLen = walkArgs->fLength;
			if (segLen > (length - index)) {
				segLen = length - index;
			}

			index += segLen;

			align = (phys & mapperPageMask);
			segLen = ((phys + segLen + mapperPageMask) & ~mapperPageMask);
			phys -= align;
			segLen -= phys;

			spaceInsert(space, mapOptions, mappedAddress, insertOffset, phys, segLen);
			insertOffset += segLen;
		}
	}

    *mapAddress = mappedAddress + firstPageOffset;
	if (discontig) {
		// returning the total DMA map size will cause IOMD to scatter gather the DMA
		*mapLength  = mappedLength - firstPageOffset;
	} else {
		// returning the exact map length means the DMA must be a single range
		*mapLength  = length;
	}

    return (kIOReturnSuccess);
}



#define WAIT_QI_FREE(unit, idx)		\
	if (!stampPassed((unit)->qi_stamp, (unit)->qi_table_stamps[(idx)]))				\
	{																				\
		(unit)->qi_stalled_stamp = (unit)->qi_table_stamps[(idx)];					\
		while (!stampPassed((unit)->qi_stamp, (unit)->qi_table_stamps[(idx)])) {}	\
	}

IOReturn 
AppleVTD::spaceUnmapMemory(	vtd_space_t * space,
							IOMemoryDescriptor * memory,
							IODMACommand * dmaCommand,
							uint64_t mapAddress, uint64_t mapLength)
{
	vtd_unit_t * unit;
	unsigned int leaf, isLarge;
	unsigned int unitIdx;
	ppnum_t      unitAddr;
	IOItemCount  unitPages;
	uint32_t     freeQueueIdx;
	uint32_t     did;
	uint32_t     idx;
	uint32_t     next;
	uint32_t     count;
	uint32_t     stamp;
	ppnum_t      addr;
	ppnum_t      pages;

    did = space->domain;
    addr = static_cast<ppnum_t>(atop_64(mapAddress));
    pages = static_cast<ppnum_t>(atop_64(round_page_64(mapAddress + mapLength)) - addr);

#if KP
	VTLOG("iovmFree: 0x%x,0x%x\n", (int)pages, addr);
#endif

	vtassert((addr + pages) <= space->vsize);
	vtd_space_nfault(space, addr, pages);
	bzero(&space->tables[0][addr], pages * sizeof(vtd_table_entry_t));
	table_flush(&space->tables[0][addr], pages * sizeof(vtd_table_entry_t), fCacheLineSize);

	leaf = true;
	isLarge = (addr >= space->rsize);

	VTHWLOCK(fHWLock);

#if 0
	int32_t      freeCount;
	freeCount = space->free_tail[isLarge] - space->free_head[isLarge];
	if (freeCount < 0) freeCount = kFreeQElems - freeCount;
	if (freeCount >= 8)
#endif
#if FREE_ON_FREE
	{
		checkFree(space, isLarge);
	}
#endif

	freeQueueIdx = free_tail[isLarge];
	next = (freeQueueIdx + 1) & free_mask;
	if (next == free_head[isLarge])
	{
	    uint64_t deadline;
	    clock_interval_to_deadline(600, kMillisecondScale, &deadline);
	    while (true)
	    {
			checkFree(space, isLarge);
			freeQueueIdx = free_tail[isLarge];
			next = (freeQueueIdx + 1) & free_mask;
			if (next != free_head[isLarge]) break;
			if (mach_absolute_time() >= deadline) panic("qfull");
	    }
	}
	free_queue[isLarge][freeQueueIdx].addr = addr;
	free_queue[isLarge][freeQueueIdx].size = pages;
	free_queue[isLarge][freeQueueIdx].space = space;
	space->pending_free++;
	free_tail[isLarge] = next;

	for (unitIdx = 0; (unit = units[unitIdx]); unitIdx++)
	{
		if (!unit->translating) continue;

		stamp = ++fQIStamp[unitIdx];
		free_queue[isLarge][freeQueueIdx].stamp[unitIdx] = stamp;

		unitAddr = addr;
		unitPages = pages;
		idx = unit->qi_tail;
		count = 0;
		while (unitPages)
		{
			next = (idx + 1) & kQIIndexMask;
			WAIT_QI_FREE(unit, idx);
			if (unit->selective)
			{
				uint32_t mask = unit->rounding;
				if (unitPages < (1U << unit->rounding)) mask = vtd_log2up(unitPages);
				unit->qi_table[idx].command = (did<<16) | (kTlbDrainReads<<7) | (kTlbDrainWrites<<6) | (3<<4) | (2);
				unit->qi_table[idx].address = ptoa_64(unitAddr) | (leaf << 6) | mask;
			}
			else
			{
				unit->qi_table[idx].command = (kTlbDrainReads<<7) | (kTlbDrainWrites<<6) | (1<<4) | (2);
			}
			unit->qi_table_stamps[idx] = stamp;

			if (!unit->selective 
				|| (unitPages <= (1U << unit->rounding)))
			{
				unitPages = 0;
			}
			else
			{
				unitPages -= (1U << unit->rounding);
				unitAddr  += (1U << unit->rounding);
				count++;
				if (!(count & kQIIndexStoreMask))
				{
					__mfence();
					unit->regs->invalidation_queue_tail = (next << 4);
				}
			}
			idx = next;
            if (!unitPages || (!(count & kQIIndexStampMask)))
            // write stamp command
            {
                count++;
                next = (idx + 1) & kQIIndexMask;
                WAIT_QI_FREE(unit, idx);
                uint64_t command = (static_cast<uint64_t>(stamp)<<32) | (1<<5) | (5);
//       		command |= (1<<4); // make an int
                unit->qi_table[idx].command = command;
                unit->qi_table[idx].address = unit->qi_stamp_address;
                unit->qi_table_stamps[idx] = stamp;
                idx = next;
            }
		}
		__mfence();
		unit->regs->invalidation_queue_tail = (next << 4);
//		__mfence();
		unit->qi_tail = next;
	}

	VTHWUNLOCK(fHWLock);

    return (kIOReturnSuccess);
}

void 
AppleVTD::checkFree(vtd_space_t * space, uint32_t isLarge)
{
	vtd_space_t *free_space;
	vtd_unit_t * unit;
	uint32_t     unitIdx;
	uint32_t     idx;
	uint32_t     next;
	ppnum_t      addr, size, count;
    bool         ok;

	count = 0;
	idx = free_head[isLarge];
	do
	{
		if (idx == free_tail[isLarge]) break;
		for (unitIdx = 0, ok = true; ok && (unit = units[unitIdx]); unitIdx++)
		{
			if (!unit->translating) continue;
			ok &= stampPassed(unit->qi_stamp, free_queue[isLarge][idx].stamp[unitIdx]);
		}
	
		if (ok)
		{
			next = (idx + 1) & free_mask;
			addr = free_queue[isLarge][idx].addr;
			size = free_queue[isLarge][idx].size;
			free_space = free_queue[isLarge][idx].space;
			free_head[isLarge] = next;
			VTHWUNLOCK(fHWLock);

			space_free(free_space, addr, size);

			VTHWLOCK(fHWLock);
			free_space->pending_free--;
			idx = free_head[isLarge];
			count++;
		}
	}
	while (ok && (count < 8));

	if (count > space->stats.max_inval[isLarge]) space->stats.max_inval[isLarge] = count;
}

void
AppleVTD::contextInvalidate(uint16_t domainID)
{
	vtd_unit_t * unit;
	unsigned int unitIdx;
	uint32_t     idx;
	uint32_t     next;
	uint32_t     gran;
	uint32_t     stamp[kMaxUnits];
	uint64_t     deadline;
	bool         ok;

	VTHWLOCK(fHWLock);

	gran = (domainID != 0) ? 2 : 1;		// global or domain selective

	for (unitIdx = 0; (unit = units[unitIdx]); unitIdx++)
	{
		if (!unit->translating) continue;

		stamp[unitIdx] = ++fQIStamp[unitIdx];
		idx = unit->qi_tail;

		// fence
		next = (idx + 1) & kQIIndexMask;
		WAIT_QI_FREE(unit, idx);
		unit->qi_table[idx].address = 0;
		unit->qi_table[idx].command = (1<<6) | (5);
		unit->qi_table_stamps[idx] = stamp[unitIdx];

		// context invalidate domain
		idx = next;
		next = (idx + 1) & kQIIndexMask;
		WAIT_QI_FREE(unit, idx);
		unit->qi_table[idx].address = 0;
		unit->qi_table[idx].command = (domainID << 16) | (gran << 4) | (1);
		unit->qi_table_stamps[idx] = stamp[unitIdx];

		// fence
		idx = next;
		next = (idx + 1) & kQIIndexMask;
		WAIT_QI_FREE(unit, idx);
		unit->qi_table[idx].address = 0;
		unit->qi_table[idx].command = (1<<6) | (5);
		unit->qi_table_stamps[idx] = stamp[unitIdx];

		// iotlb invalidate domain
		idx = next;
		next = (idx + 1) & kQIIndexMask;
		WAIT_QI_FREE(unit, idx);
		unit->qi_table[idx].address = 0;
		unit->qi_table[idx].command = (domainID << 16) | (kTlbDrainReads<<7) | (kTlbDrainWrites<<6) | (gran << 4) | (2);
		unit->qi_table_stamps[idx] = stamp[unitIdx];

		// stamp
		idx = next;
		next = (idx + 1) & kQIIndexMask;
		WAIT_QI_FREE(unit, idx);
		unit->qi_table[idx].address = unit->qi_stamp_address;
		unit->qi_table[idx].command = (static_cast<uint64_t>(stamp[unitIdx])<<32) | (1<<5) | (5);
		unit->qi_table_stamps[idx] = stamp[unitIdx];

		__mfence();
		unit->regs->invalidation_queue_tail = (next << 4);
		unit->qi_tail = next;
	}

	VTHWUNLOCK(fHWLock);

	clock_interval_to_deadline(600, kMillisecondScale, &deadline);
	while (true)
	{
		for (unitIdx = 0, ok = true; ok && (unit = units[unitIdx]); unitIdx++)
		{
			if (!unit->translating) continue;
			ok &= stampPassed(unit->qi_stamp, stamp[unitIdx]);
		}
		if (ok) break;
		if (mach_absolute_time() >= deadline) panic("context qi");
	}
}

void
AppleVTD::interruptInvalidate(uint16_t index, uint16_t count)
{
	vtd_unit_t * unit;
	unsigned int unitIdx;
	uint32_t     idx;
	uint32_t     next;
	uint32_t     stamp[kMaxUnits];
	uint64_t     deadline;
	bool         ok;

	if (!fIRTable) return;

	count = vtd_log2up(count);

	VTHWLOCK(fHWLock);

	for (unitIdx = 0; (unit = units[unitIdx]); unitIdx++)
	{
		if (!unit->ir_address) continue;
		stamp[unitIdx] = ++fQIStamp[unitIdx];
		idx = unit->qi_tail;
		next = (idx + 1) & kQIIndexMask;

		// int invalidate
		WAIT_QI_FREE(unit, idx);
		unit->qi_table[idx].address = 0;
		unit->qi_table[idx].command = (((uint64_t) index) << 32) | (count << 27) | (0<<4) | (4);
		unit->qi_table_stamps[idx] = stamp[unitIdx];

		// stamp
		idx = next;
		next = (idx + 1) & kQIIndexMask;
		WAIT_QI_FREE(unit, idx);
		unit->qi_table[idx].address = unit->qi_stamp_address;
		unit->qi_table[idx].command = (static_cast<uint64_t>(stamp[unitIdx])<<32) | (1<<5) | (5);
		unit->qi_table_stamps[idx] = stamp[unitIdx];

		__mfence();
		unit->regs->invalidation_queue_tail = (next << 4);
		unit->qi_tail = next;
	}

	VTHWUNLOCK(fHWLock);

	clock_interval_to_deadline(600, kMillisecondScale, &deadline);
	while (true)
	{
		for (unitIdx = 0, ok = true; ok && (unit = units[unitIdx]); unitIdx++)
		{
			ok &= stampPassed(unit->qi_stamp, stamp[unitIdx]);
		}
		if (ok) break;
		if (mach_absolute_time() >= deadline) panic("interrupt qi");
	}
}

uint64_t
AppleVTD::spaceMapToPhysicalAddress(vtd_space_t * space, uint64_t addr)
{
	ppnum_t      page = static_cast<ppnum_t>(atop_64(addr));
	page_entry_t entry;

	if (page >= space->vsize) return (addr);

	if (!vtd_space_present(space, page)) return (addr);

	entry = space->tables[0][page].bits;

#if KP
	VTLOG("spaceMapToPhysicalAddress: 0x%x=0x%llx\n", (int)addr, entry);
#endif

	if (kPageAccess & entry)
		return (trunc_page_64(entry) | (addr & page_mask));
	else
		return (addr);

    return (0);
}

IOReturn
AppleVTD::spaceInsert(vtd_space_t * space, uint32_t mapOptions,
					  uint64_t mapAddress, uint64_t byteOffset,
					  uint64_t physicalAddress, uint64_t length)
{
	ppnum_t addr;
	ppnum_t offset;
	ppnum_t idx;
	ppnum_t phys;
	uint32_t pageCount;

    addr = static_cast<ppnum_t>(atop_64(mapAddress));
    offset = static_cast<ppnum_t>(atop_64(mapAddress + byteOffset) - addr);
	phys = static_cast<ppnum_t>(atop_64(physicalAddress));
	pageCount = static_cast<uint32_t>(atop_64(physicalAddress + length) - phys);

	addr += offset;
	vtassert((addr + pageCount) <= space->vsize);
	vtd_space_nfault(space, addr, pageCount);
    for (idx = 0; idx < pageCount; idx++)
    {
		space->tables[0][addr + idx].bits = (ptoa_64(phys + idx)) | kPageAccess;
	}
	table_flush(&space->tables[0][addr], pageCount * sizeof(vtd_table_entry_t), fCacheLineSize);
	STAT_ADD(space, inserts, pageCount);

    return (kIOReturnSuccess);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

uint64_t
AppleVTD::getPageSize(void) const
{
    return (4096);
}

IOReturn
AppleVTD::iovmMapMemory(
			  IOMemoryDescriptor          * memory,
			  uint64_t                      descriptorOffset,
			  uint64_t                      length,
			  uint32_t                      mapOptions,
			  const IODMAMapSpecification * mapSpecification,
			  IODMACommand                * dmaCommand,
			  const IODMAMapPageList      * pageList,
			  uint64_t                    * mapAddress,
			  uint64_t                    * mapLength)
{
    return (spaceMapMemory(fSpace, memory, descriptorOffset, length,
			  mapOptions, mapSpecification, dmaCommand, pageList, mapAddress, mapLength));
}

IOReturn
AppleVTD::iovmUnmapMemory(IOMemoryDescriptor * memory,
						  IODMACommand * dmaCommand,
						  uint64_t mapAddress, uint64_t mapLength)
{
    return (spaceUnmapMemory(fSpace, memory, dmaCommand, mapAddress, mapLength));
}

uint64_t
AppleVTD::mapToPhysicalAddress(uint64_t addr)
{
    return (spaceMapToPhysicalAddress(fSpace, addr));
}

IOReturn
AppleVTD::iovmInsert(uint32_t mapOptions,
							     uint64_t mapAddress, uint64_t byteOffset,
							     uint64_t physicalAddress, uint64_t length)
{
    return (spaceInsert(fSpace, mapOptions, mapAddress, byteOffset,
						physicalAddress, length));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
AppleVTD::newContextPage(uint32_t idx)
{
	IOBufferMemoryDescriptor * md;
	IOMemoryMap              * map;
    context_entry_t          * context;
    context_entry_t          * globalContext;
	IOReturn                   kr;
    ppnum_t                    context_page;

	md = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
													 kIOMemoryPageable |
													 kIOMapWriteCombineCache |
													 kIOMemoryMapperNone,
													 1 * page_size, page_size);
	vtassert(md);
	if (!md) return (kIOReturnNoMemory);

	kr = md->prepare(kIODirectionOutIn);
	vtassert(KERN_SUCCESS == kr);

	map = md->map();
	vtassert(map);
	md->release();

	globalContext = (typeof(globalContext)) fGlobalContextMap->getVirtualAddress();
	context = (typeof(context)) map->getVirtualAddress();
	context[0].address_space_root = 0;

	VTHWLOCK(fHWLock);

	if (fGlobalContextMap == fContextMaps[idx])
	{
		bcopy(&globalContext[0], &context[0], 256 * sizeof(context[0]));

		context_page = pmap_find_phys(kernel_pmap, (uintptr_t) &context[0]);
		if (!context_page) panic("!context_page");

		// update root table
		fRootEntryTable[idx].context_entry_ptr = ptoa_64(context_page)| kEntryPresent;
		fContextMaps[idx] = map;
		map = 0;
	}

	VTHWUNLOCK(fHWLock);

	if (map) map->release();

	return (kr);
}

IOReturn
AppleVTD::deviceMapperActivate(AppleVTDDeviceMapper * mapper, uint32_t options)
{
    context_entry_t * context;
    vtd_space_t     * space;
	IOReturn          ret;
	uint32_t          idx, count;

    ret = kIOReturnSuccess;
    if (!(kDeviceMapperActivate & options) && !mapper->fSpace) return (ret);

    if ((kDeviceMapperActivate | kDeviceMapperUnpause) & options)
    {
		mapper->fSourceID = ((mapper->fDevice->space.s.busNum      << 8)
						   | (mapper->fDevice->space.s.deviceNum   << 3)
						   | (mapper->fDevice->space.s.functionNum << 0));
	}

	VTLOG("mapper activate(%s, 0x%04x) 0x%x\n", mapper->fDevice->getName(), mapper->fSourceID, options);

	idx = (mapper->fSourceID >> 8) & 0xFF;
	if (fGlobalContextMap == fContextMaps[idx]) newContextPage(idx);
	context = (typeof(context)) fContextMaps[idx]->getVirtualAddress();

	space = 0;
    if ((kDeviceMapperActivate | kDeviceMapperUnpause) & options)
    {
		bool created = false;
		// Check if the AMD mapper space has been created
		if (mapper->fIsAMD)
		{
			mapper->fSpace = mapper->fVTD->fAMDSpace;
		}

		if (!mapper->fSpace)
		{
			mapper->fSpace = space_create(mapper->vsize, kBPagesLog2, kRPages);
			created = true;
		}
		space = mapper->fSpace;
		if (!space) ret = kIOReturnNoMemory;

		// If we created the AMD mapper space, save it
		if (mapper->fIsAMD && !mapper->fVTD->fAMDSpace)
		{
			mapper->fVTD->fAMDSpace = space;
		}

		if (created)
		{
			mapper->fVTD->reserveRanges(space, false);
		}
	}
	if (!space) space = fSpace;

	idx = (mapper->fSourceID >> 0) & 0xFF;
	count = 1;
	if (mapper->fAllFunctions)
	{
		idx &= ~7U;
		count = 8;
	}
	for (; count; idx++, count--)
	{
		context[idx].address_space_root = ptoa_64(space->root_page)
										  | kEntryPresent
										  | kTranslationType0;
		context[idx].context_entry = fContextWidth
										  | space->domain*kDomainIdentifier1;
	}
	if (!(kDeviceMapperActivate & options)) contextInvalidate(mapper->fSpace->domain);

	// The AMD mapper space is persistent
    if ((kDeviceMapperDeactivate & options) && !(mapper->fIsAMD))
    {
		space_destroy(mapper->fSpace);
		mapper->fSpace = 0;
    }

	return (ret);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(AppleVTDDeviceMapper, IOMapper);
#define super IOMapper

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

AppleVTDDeviceMapper *
AppleVTDDeviceMapper::forDevice(IOService * device, uint32_t flags)
{
	AppleVTDDeviceMapper * mapper;
	IOPCIDevice          * pciDevice;
	uint32_t               vendorProduct;
	bool                   isAMD;

	if (!(pciDevice = OSDynamicCast(IOPCIDevice, device))) return (NULL);

	vendorProduct = pciDevice->savedConfig[kIOPCIConfigVendorID >> 2];

	isAMD = (0x1002 == (vendorProduct & 0xffff));

	mapper = OSTypeAlloc(AppleVTDDeviceMapper);
	if (!mapper) return (0);

	mapper->fVTD      = OSDynamicCast(AppleVTD, IOMapper::gSystem);
	mapper->fDevice   = pciDevice;
	mapper->fIsAMD    = isAMD;

	mapper->fAllFunctions =    ((0x91201b4b == vendorProduct)
							 || (0x91231b4b == vendorProduct)
							 || (0x91281b4b == vendorProduct)
							 || (0x91301b4b == vendorProduct)
							 || (0x91721b4b == vendorProduct)
							 || (0x917a1b4b == vendorProduct)
							 || (0x91821b4b == vendorProduct)
							 || (0x91831b4b == vendorProduct)
							 || (0x91a01b4b == vendorProduct)
							 || (0x92201b4b == vendorProduct)
							 || (0x92301b4b == vendorProduct)
							 || (0x06421103 == vendorProduct)
							 || (0x06451103 == vendorProduct)
							 || (0x2392197B == vendorProduct)
							 || (0x01221c28 == vendorProduct)
							 || (0x92351b4b == vendorProduct)
							 || (0x08300034 == vendorProduct));

	mapper->vsize = 1<<21;
	if (isAMD)
	{
		mapper->vsize = kVPages;
	}

    mapper->initHardware(NULL);

	if (!isAMD)
	{
		//allocate the IO lock
		mapper->fAppleVTDforDeviceLock = IOLockAlloc();
	}
	else
	{
		//allocate the shared IO lock
		if (!mapper->fVTD->fAMDMapperLock)
		{
			mapper->fVTD->fAMDMapperLock = IOLockAlloc();
		}
		mapper->fAppleVTDforDeviceLock = mapper->fVTD->fAMDMapperLock;
	}

    return (mapper);
}

bool
AppleVTDDeviceMapper::initHardware(IOService *provider)
{
    return (super::init());
}

void
AppleVTDDeviceMapper::free()
{
	super::free();
}

uint64_t
AppleVTDDeviceMapper::getPageSize(void) const
{
    return (fVTD->getPageSize());
}

IOReturn
AppleVTDDeviceMapper::iovmMapMemory(
			  IOMemoryDescriptor          * memory,
			  uint64_t                      descriptorOffset,
			  uint64_t                      length,
			  uint32_t                      mapOptions,
			  const IODMAMapSpecification * mapSpecification,
			  IODMACommand                * dmaCommand,
			  const IODMAMapPageList      * pageList,
			  uint64_t                    * mapAddress,
			  uint64_t                    * mapLength)
{
    IOReturn ret = kIOReturnSuccess;

	IOLockLock(fAppleVTDforDeviceLock);
	if (!fSpace)
	{
        ret = fVTD->deviceMapperActivate(this, kDeviceMapperActivate);
	}
	IOLockUnlock(fAppleVTDforDeviceLock);
	
	if (kIOReturnSuccess != ret) {
        return (ret);
	}

    ret = fVTD->spaceMapMemory(fSpace, memory, descriptorOffset, length,
				mapOptions, mapSpecification, dmaCommand, pageList, mapAddress, mapLength);

    return (ret);
}

IOReturn
AppleVTDDeviceMapper::iovmUnmapMemory(IOMemoryDescriptor * memory,
									  IODMACommand * dmaCommand,
									  uint64_t mapAddress, uint64_t mapLength)
{
	if (!fSpace) return kIOReturnSuccess;
    return (fVTD->spaceUnmapMemory(fSpace, memory, dmaCommand, mapAddress, mapLength));
}

uint64_t
AppleVTDDeviceMapper::mapToPhysicalAddress(uint64_t addr)
{
    return (fVTD->spaceMapToPhysicalAddress(fSpace, addr));
}

IOReturn
AppleVTDDeviceMapper::iovmInsert(uint32_t mapOptions,
							     uint64_t mapAddress, uint64_t byteOffset,
							     uint64_t physicalAddress, uint64_t length)
{
    return (fVTD->spaceInsert(fSpace, mapOptions, mapAddress, byteOffset,
			physicalAddress, length));
}

#endif /* ACPI_SUPPORT */
