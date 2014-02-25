/*
 * Copyright (c) 2012 Apple Computer, Inc. All rights reserved.
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

#if ACPI_SUPPORT

#include <IOKit/IOMapper.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <libkern/tree.h>
#include <libkern/OSDebug.h>
#include <i386/cpuid.h>
#include "dmar.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" vm_offset_t ml_io_map(vm_offset_t phys_addr, vm_size_t size);
extern "C" ppnum_t pmap_find_phys(pmap_t pmap, addr64_t va);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define ENA_QI			1
#define TABLE_CB		0
#define BSIMPLE			0

#define KP				0
#define	VTASRT			0

#define kLargeThresh	(128)
#define kLargeThresh2	(32)
#define kVPages  		(1<<22)
#define kBPagesLog2 	(18)
#define kBPagesSafe		((1<<kBPagesLog2)-(1<<(kBPagesLog2 - 2)))      /* 3/4 */
#define kBPagesReserve	((1<<kBPagesLog2)-(1<<(kBPagesLog2 - 3)))      /* 7/8 */
#define kRPages  		(1<<20)

#define kQIPageCount    (2)

#define kTlbDrainReads  (0ULL)
#define kTlbDrainWrites (0ULL)

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


#if TABLE_CB
#define table_flush(addr, size, linesize) clflush((uintptr_t)(addr), (size), linesize);
#else
#define table_flush(addr, size, linesize) __mfence();
#endif


#if BSIMPLE
#define BLOCK(l)	IOSimpleLockLock(l)
#define BUNLOCK(l)	IOSimpleLockUnlock(l)
#else
#define BLOCK(l)	IOLockLock(l)
#define BUNLOCK(l)	IOLockUnlock(l)
#endif

#define arrayCount(x)	(sizeof(x) / sizeof(x[0]))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum
{
	kEntryPresent = 0x00000001ULL
};

struct root_entry_t
{
    uint64_t context_entry_ptr;
    uint64_t resv;
};

struct context_entry_t
{
    uint64_t address_space_root;
    uint64_t context_entry;
};

struct qi_descriptor_t
{
    uint64_t command;
    uint64_t address;
};

// address_space_root
enum
{
//	kEntryPresent 			= 0x00000001ULL,
	kFaultProcessingDisable = 0x00000002ULL,
	kTranslationType0 		= 0x00000000ULL,
	kTranslationType1 		= 0x00000004ULL,
	kTranslationType2 		= 0x00000008ULL,
	kTranslationType3 		= 0x0000000CULL,
	kEvictionHint 		    = 0x00000010ULL,
	kAddressLocalityHint    = 0x00000020ULL,
};

// context_entry
enum
{
	kAddressWidth30			= 0x00000000ULL,
	kAddressWidth39			= 0x00000001ULL,
	kAddressWidth48			= 0x00000002ULL,
	kAddressWidth57			= 0x00000003ULL,
	kAddressWidth64			= 0x00000004ULL,
	
	kContextAvail1		    = 0x00000008ULL,	// 4b
	kDomainIdentifier1		= 0x00000100ULL,	// 16b
};

enum
{
	kNotTheDomain = 1ULL << 32,
	kTheDomain    = 2ULL
};

typedef uint64_t page_entry_t;

// page_entry_t
enum
{
	kReadAccess 			= 0x00000001ULL,
	kWriteAccess			= 0x00000002ULL,
	kPageAccess				= kReadAccess|kWriteAccess,
	kPageAvail1			    = 0x00000004ULL,	// 5b
	kSuperPage			    = 0x00000080ULL,
	kPageAvail2			    = 0x00000100ULL,	// 3b
	kSnoopBehavior		    = 0x00000800ULL,
	kTransientMapping		= 0x4000000000000000ULL,
	kPageAvail3				= 0x8000000000000000ULL, // 1b

	kPageAddrMask			= 0x3ffffffffffff000ULL
};

struct vtd_registers_t
{
/*00*/ 	uint32_t version;
/*04*/	uint32_t res1;
/*08*/	uint64_t capability;
/*10*/	uint64_t extended_capability;
/*18*/	uint32_t global_command;
/*1c*/	uint32_t global_status;
/*20*/	uint64_t root_entry_table;
/*28*/	uint64_t context_command;
/*30*/	uint32_t res2;
/*34*/	uint32_t fault_status;
/*38*/	uint32_t fault_event_control;
/*3c*/	uint32_t fault_event_data;
/*40*/	uint32_t fault_event_address;
/*44*/	uint32_t fault_event_upper_address;
/*48*/	uint64_t res3[2];
/*58*/	uint64_t advanced_fault;
/*60*/	uint32_t res4;
/*64*/	uint32_t protected_memory_enable;
/*68*/	uint32_t protected_low_memory_base;
/*6c*/	uint32_t protected_low_memory_limit;
/*70*/	uint64_t protected_high_memory_base;
/*78*/	uint64_t protected_high_memory_limit;
/*80*/	uint64_t invalidation_queue_head;
/*88*/	uint64_t invalidation_queue_tail;
/*90*/	uint64_t invalidation_queue_address;
/*98*/	uint32_t res5;
/*9c*/	uint32_t invalidation_completion_status;
/*a0*/	uint32_t invalidation_completion_event_control;
/*a4*/	uint32_t invalidation_completion_event_data;
/*a8*/	uint32_t invalidation_completion_event_address;
/*ac*/	uint32_t invalidation_completion_event_upper_address;
/*b0*/	uint64_t res6;
/*b8*/	uint64_t interrupt_remapping_table;
/*c0*/	
};

struct vtd_iotlb_registers_t
{
/*00*/	uint64_t address;
/*08*/	uint64_t command;
};
struct vtd_fault_registers_t
{
/*00*/	uint64_t fault_low;
/*08*/	uint64_t fault_high;
};

typedef char vtd_registers_t_check[(sizeof(vtd_registers_t) == 0xc0) ? 1 : -1];

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct vtd_unit_t
{
    ACPI_DMAR_HARDWARE_UNIT * dmar;
    volatile vtd_registers_t * regs;
    volatile vtd_iotlb_registers_t * iotlb;
    volatile vtd_fault_registers_t * faults;

    IOMemoryMap *     qi_map;
    qi_descriptor_t * qi_table;

	uint64_t root;
	uint64_t msi_address;
    uint64_t qi_address;
    uint64_t qi_stamp_address;

	uint32_t qi_tail;
	uint32_t qi_mask;
    volatile
    uint32_t qi_stamp;

	uint32_t msi_data;
    uint32_t num_fault;
    uint32_t rounding;

    uint8_t  global:1;
    uint8_t  caching:1;
    uint8_t  selective:1;
};

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

static
vtd_unit_t * unit_init(ACPI_DMAR_HARDWARE_UNIT * dmar)
{
	vtd_unit_t * unit;

	unit = IONew(vtd_unit_t, 1);
	if (!unit) return (NULL);
	bzero(unit, sizeof(vtd_unit_t));

	unit->dmar = dmar;

	VTLOG("unit %p Address %llx, Flags %x\n",
			dmar, dmar->Address, dmar->Flags);

	unit->regs = (typeof unit->regs) ml_io_map(dmar->Address, 0x1000);

	uint32_t
	offset = (unit->regs->extended_capability >> (8 - 4)) & (((1 << 10) - 1) << 4);
	unit->iotlb = (typeof(unit->iotlb)) (((uintptr_t)unit->regs) + offset);

	offset = (unit->regs->capability >> (24 - 4)) & (((1 << 10) - 1) << 4);
	unit->faults = (typeof(unit->faults)) (((uintptr_t)unit->regs) + offset);
	unit->num_fault = (1 + ((unit->regs->capability >> 40) & ((1 << 8) - 1)));

	unit->selective = (1 & (unit->regs->capability >> 39));
	unit->rounding = (0x3f & (unit->regs->capability >> 48));
	unit->caching = (1 & (unit->regs->capability >> 7));
	unit->global = (ACPI_DMAR_INCLUDE_ALL & dmar->Flags);

	VTLOG("cap 0x%llx extcap 0x%llx glob %d cache sel %d mode %d iotlb %p nfault[%d] %p\n", 
			unit->regs->capability, unit->regs->extended_capability,
			unit->global, unit->selective, unit->caching, 
			unit->iotlb, unit->num_fault, unit->faults);

	// caching is only allowed for VMs
	if (unit->caching
	// disable IG unit
	|| ((!unit->global) && (!(kIOPCIConfiguratorIGIsMapped & gIOPCIFlags))))
	{
		IODelete(unit, vtd_unit_t, 1);
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
unit_enable(vtd_unit_t * unit)
{
    uint32_t command;

	VTLOG("unit %p global status 0x%x\n", unit, unit->regs->global_status);

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

	unit->qi_tail = 0;
	unit->regs->invalidation_queue_head = 0;
	unit->regs->invalidation_queue_tail = 0;
    unit->regs->invalidation_queue_address = unit->qi_address;

	command = 0;

#if ENA_QI
	command |= (1UL<<26);
	unit->regs->global_command = command;
	__mfence();
	while (!((1UL<<26) & unit->regs->global_status)) {}
	VTLOG("did ena qi p 0x%qx v %p\n", unit->qi_address, unit->qi_table);
#endif

	command |= (1UL<<31);
	unit->regs->global_command = command;
	__mfence();
	while (!((1UL<<31) & unit->regs->global_status)) {}
	VTLOG("did ena\n");

	if (unit->msi_address)
	{
		unit->regs->invalidation_completion_event_data          = unit->msi_data;
		unit->regs->invalidation_completion_event_address       = unit->msi_address;
		unit->regs->invalidation_completion_event_upper_address = (unit->msi_address >> 32);

		unit->regs->fault_event_data          = unit->msi_data + 1;
		unit->regs->fault_event_address       = unit->msi_address;
		unit->regs->fault_event_upper_address = (unit->msi_address >> 32);

		__mfence();
		unit_faults(unit, false);

		unit->regs->fault_event_control = 0;					// ints ena
		unit->regs->invalidation_completion_event_control = 0;	// ints ena
		unit->regs->invalidation_completion_status = 1;
	}
}

static void 
unit_quiesce(vtd_unit_t * unit)
{
	VTLOG("unit %p quiesce\n", unit);
	// completion stamps will continue after wake
}

static void
unit_invalidate(vtd_unit_t * unit, 
							uint64_t did, ppnum_t addr, ppnum_t mask, bool leaf)
{
	if (unit->selective)
	{
		 unit->iotlb->address = ptoa_64(addr) | (leaf << 6) | mask;
		 __mfence();
		 unit->iotlb->command = (1ULL<<63) | (3ULL<<60) | (kTlbDrainReads<<49) | (kTlbDrainWrites<<48) | (did << 32);
	}
	else unit->iotlb->command = (1ULL<<63) | (1ULL<<60) | (kTlbDrainReads<<49) | (kTlbDrainWrites<<48);
	__mfence();
}

static void 
unit_invalidate_done(vtd_unit_t * unit)
{
	while ((1ULL<<63) & unit->iotlb->command) {}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

typedef uint32_t vtd_vaddr_t;

union vtd_table_entry
{
	struct
	{
		uint     read:1 	__attribute__ ((packed));
		uint     write:1 	__attribute__ ((packed));
		uint     resv:10 	__attribute__ ((packed));
		uint64_t addr:51 	__attribute__ ((packed));
		uint     used:1 	__attribute__ ((packed));
	} used;
	struct
	{
		uint access:2 		__attribute__ ((packed));
		uint next:28 		__attribute__ ((packed));
		uint prev:28 		__attribute__ ((packed));
		uint size:5 		__attribute__ ((packed));
		uint free:1 		__attribute__ ((packed));
	} free;
	uint64_t bits;
};
typedef union vtd_table_entry vtd_table_entry_t;

typedef uint32_t vtd_rbaddr_t;

struct vtd_rblock
{
	RB_ENTRY(vtd_rblock) address_link;
	RB_ENTRY(vtd_rblock) size_link;

	vtd_rbaddr_t start;
	vtd_rbaddr_t end;
};

RB_HEAD(vtd_rbaddr_list, vtd_rblock);
RB_HEAD(vtd_rbsize_list, vtd_rblock);

struct vtd_space_stats
{
    ppnum_t vsize;
    ppnum_t tables;
    ppnum_t bused;
    ppnum_t rused;
    ppnum_t largest_paging;
    ppnum_t largest_32b;
    ppnum_t inserts;
    ppnum_t max_inval[2];
    ppnum_t breakups;
    ppnum_t merges;
    ppnum_t allocs[64];
	ppnum_t bcounts[20];
};
typedef struct vtd_space_stats vtd_space_stats_t;

struct vtd_free_queued_t
{
    ppnum_t  addr;
    ppnum_t  size;
    uint32_t stamp;
};
enum 
{
	kFreeQCount = 2,
	kFreeQElems = 256
};

struct vtd_space
{
#if BSIMPLE
	IOSimpleLock *      block;
#else
	IOLock *            block;
#endif
	IOLock *            rlock;
	ppnum_t				vsize;
	ppnum_t				rsize;
	size_t      	    table_bitmap_size;
	uint8_t *   	    table_bitmap;
	IOMemoryMap *       table_map;
	vtd_table_entry_t *	tables[6];
	uint32_t            cachelinesize;
	ppnum_t             root_page;
	uint8_t				max_level;
    uint8_t             waiting_space;
	uint8_t     	    bheads_count;
	vtd_table_entry_t * bheads;

	vtd_space_stats_t   stats;

    vtd_free_queued_t   free_queue[kFreeQCount][kFreeQElems];
    volatile uint32_t	free_head[kFreeQCount];
    volatile uint32_t   free_tail[kFreeQCount];
    uint32_t			free_mask;
    uint32_t            stamp;

	struct vtd_rbaddr_list rbaddr_list;
	struct vtd_rbsize_list rbsize_list;
};
typedef struct vtd_space vtd_space_t;

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

static void
_vtd_space_nfault(vtd_space_t * bf, vtd_vaddr_t start, vtd_vaddr_t size)
{
	vtd_vaddr_t index;
	vtd_vaddr_t byte;
	uint8_t bit;

	vtassert((start + size) < bf->vsize);

	size += (start & 511);
	size = (size + 511) & ~511;

	while (true)
	{
		index = (start >> 9);
		byte = (index >> 3);
		bit = (1 << (7 & index));
		vtassert(bf->table_bitmap[byte] & bit);
		if (size < 512) break;
		size -= 512;
		start += 512;
	}
}

static bool
vtd_space_present(vtd_space_t * bf, vtd_vaddr_t start)
{
	vtd_vaddr_t byte;
	uint8_t bit;

	vtassert(start < bf->vsize);

	start >>= 9;
	byte = (start >> 3);
	bit = (1 << (7 & start));
	return (bf->table_bitmap[byte] & bit);
}

static void
vtd_space_fault(vtd_space_t * bf, vtd_vaddr_t start, vtd_vaddr_t size)
{
	vtd_vaddr_t index;
	vtd_vaddr_t byte;
	uint8_t     bits, bit;
	IOReturn    kr;

	vtassert((start + size) < bf->vsize);

	size += (start & 511);
	size = (size + 511) & ~511;

	while (true)
	{
		index = (start >> 9);
		byte = (index >> 3);
		index &= 7;
		bits = bf->table_bitmap[byte];
#if 1
		if (0xff == bits)
		{
			index = (8 - index) * 512;
			if (size <= index) break;
			size -= index;
			start += index;
			continue;
		}
#endif
		bit = (1 << index);
		if (!(bits & bit))
		{
			bf->table_bitmap[byte] = bits | bit;
			index = start & ~511;

//			VTLOG("table fault addr 0x%x, table %p\n", start, &bf->tables[0][start]);
			kr = bf->table_map->wireRange(kIODirectionOutIn, index << 3, page_size);
			vtassert(kr == KERN_SUCCESS);
			STAT_ADD(bf, tables, 1);

			bf->tables[0][index].bits = 0;
			ppnum_t lvl0page = pmap_find_phys(kernel_pmap, (uintptr_t) &bf->tables[0][index]);
			if (!lvl0page) panic("!lvl0page");
			bf->tables[1][index >> 9].bits = ptoa_64(lvl0page) | kPageAccess;
			table_flush(&bf->tables[1][index >> 9], sizeof(vtd_table_entry_t), bf->cachelinesize);
		}
		if (size <= 512) break;
		size -= 512;
		start += 512;
	}
}

static void
vtd_space_set(vtd_space_t * bf, vtd_vaddr_t start, vtd_vaddr_t size,
			  uint32_t mapOptions, upl_page_info_t * pageList)
{
	ppnum_t idx;
	uint8_t access = kReadAccess | 0*kWriteAccess;

	if (kIODMAMapPhysicallyContiguous & mapOptions) VTLOG("map phys %x, %x\n", pageList[0].phys_addr, size);

	if (mapOptions & kIODMAMapWriteAccess) access |= kWriteAccess;

	vtassert((start + size) <= bf->vsize);
	vtd_space_nfault(bf, start, size);

	if (kIODMAMapPhysicallyContiguous & mapOptions)
	{
		for (idx = 0; idx < size; idx++)
		{
			bf->tables[0][start + idx].bits = (access | ptoa_64(pageList[0].phys_addr + idx));
		}
#if TABLE_CB
		table_flush(&bf->tables[0][start], size * sizeof(vtd_table_entry_t), bf->cachelinesize);
#endif
	}
	else
	{
#if TABLE_CB
    	ppnum_t j;
		for (idx = 0; size >= 8; size -= 8, idx += 8)
		{
			j = 0;
			bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr)); j++;
			bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr)); j++;
			bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr)); j++;
			bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr)); j++;
			bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr)); j++;
			bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr)); j++;
			bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr)); j++;
			bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr));
			__mfence();
			__clflush((void *) &bf->tables[0][start + idx].bits);
		}
		if (size)
		{
			for (j = 0; j < size; j++)
			{
				bf->tables[0][start + idx + j].bits = (access | ptoa_64(pageList[idx + j].phys_addr));
			}
			__mfence();
			__clflush((void *) &bf->tables[0][start + idx].bits);
		}
#else
		for (idx = 0; idx < size; idx++)
		{
			bf->tables[0][start + idx].bits = (access | ptoa_64(pageList[idx].phys_addr));
		}
#endif
	}
	__mfence();
}

#include "balloc.c"
#include "rballoc.c"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleVTD : public IOMapper
{
    OSDeclareDefaultStructors(AppleVTD);

public:
	IOSimpleLock  		   * fHWLock;
	const OSData  	       * fDMARData;
	IOWorkLoop             * fWorkLoop;
	IOInterruptEventSource * fIntES;
	IOInterruptEventSource * fFaultES;
    IOTimerEventSource     * fTimerES;

	enum { kMaxUnits = 8 };
	vtd_unit_t * units[kMaxUnits];

	uint32_t fTreeBits;
	uint32_t fMaxRoundSize;

	uint32_t fCacheLineSize;

	IOMemoryMap * fTableMap;
	IOMemoryMap * fContextTableMap;

	ppnum_t  fRootEntryPage;

	vtd_space_t * fSpace;

	static void install(IOWorkLoop * wl, uint32_t flags, 
						IOService * provider, const OSData * data);
	bool init(IOWorkLoop * wl, const OSData * data);

    virtual void free();
    virtual bool initHardware(IOService *provider);

	vtd_space_t * space_create(uint32_t cachelinesize, uint32_t treebits, ppnum_t vsize,
							   uint32_t buddybits, ppnum_t rsize);
	vtd_vaddr_t space_alloc(vtd_space_t * bf, vtd_vaddr_t size,
							uint32_t mapOptions, const IODMAMapSpecification * mapSpecification, 
							upl_page_info_t * pageList);
	void space_free(vtd_space_t * bf, vtd_vaddr_t addr, vtd_vaddr_t size);
	void space_alloc_fixed(vtd_space_t * bf, vtd_vaddr_t addr, vtd_vaddr_t size);

    IOReturn handleInterrupt(IOInterruptEventSource * source, int count);
    IOReturn handleFault(IOInterruptEventSource * source, int count);
	IOReturn timer(OSObject * owner, IOTimerEventSource * sender);
	virtual IOReturn callPlatformFunction(const OSSymbol * functionName,
										  bool waitForFunction,
										  void * param1, void * param2,
										  void * param3, void * param4);

	void iovmInvalidateSync(ppnum_t addr, IOItemCount pages);
    void checkFree(uint32_t queue);

    virtual ppnum_t iovmAlloc(IOItemCount pages);
    virtual void iovmFree(ppnum_t addr, IOItemCount pages);

    virtual void iovmInsert(ppnum_t addr, IOItemCount offset, ppnum_t page);
    virtual void iovmInsert(ppnum_t addr, IOItemCount offset,
                            ppnum_t *pageList, IOItemCount pageCount);
    virtual void iovmInsert(ppnum_t addr, IOItemCount offset,
                            upl_page_info_t *pageList, IOItemCount pageCount);

    virtual ppnum_t iovmMapMemory(
    			  OSObject                    * memory,   // dma command or iomd
				  ppnum_t                       offsetPage,
				  ppnum_t                       pageCount,
				  uint32_t                      options,
				  upl_page_info_t             * pageList,
				  const IODMAMapSpecification * mapSpecification);

    virtual addr64_t mapAddr(IOPhysicalAddress addr);
};


OSDefineMetaClassAndStructors(AppleVTD, IOMapper);
#define super IOMapper

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
AppleVTD::install(IOWorkLoop * wl, uint32_t flags,
					IOService * provider, const OSData * data)
{
	AppleVTD * mapper = 0;
	bool ok = false;

	if (!IOService::getPlatform()->getProperty(kIOPlatformMapperPresentKey)) return;

	VTLOG("DMAR %p\n", data);
	if (data) 
	{
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
	if (!ok)
	{
		IOService::getPlatform()->removeProperty(kIOPlatformMapperPresentKey);
		IOMapper::setMapperRequired(false);
	}
}

bool 
AppleVTD::init(IOWorkLoop * wl, const OSData * data)
{
	uint32_t unitIdx;

	if (!super::init()) return (false);

	data->retain();
	fDMARData = data;
	wl->retain();
	fWorkLoop = wl;
	fCacheLineSize = cpuid_info()->cache_linesize;

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
				if ((units[unitIdx] = unit_init(unit))) unitIdx++;
				break;
		}
	}

	return (unitIdx != 0);
}

void AppleVTD::free()
{
	super::free();
}

vtd_space_t *
AppleVTD::space_create(uint32_t cachelinesize, 
						uint32_t treebits, ppnum_t vsize, uint32_t buddybits, ppnum_t rsize)
{
	IOBufferMemoryDescriptor * md;
	IOReturn 	   kr = kIOReturnSuccess;
	vtd_space_t *  bf;
	uint32_t       count;
	mach_vm_size_t alloc;
	uint32_t       level;
	uint32_t       bit;

	vtassert(vsize >= (1U << buddybits));
	vtassert(vsize > rsize);
	vtassert(buddybits > (9 + 3));
	vtassert(treebits > 12);

	bf = IONew(vtd_space_t, 1);
	if (!bf) return (NULL);
	bzero(bf, sizeof(vtd_space_t));

	bf->rlock = IOLockAlloc();
#if BSIMPLE
	bf->block = fHWLock;
#else
	bf->block = IOLockAlloc();
#endif
	bf->cachelinesize = cachelinesize;

	treebits -= 12;
	vsize = (vsize + 511) & ~511;
	bf->vsize = vsize;
	bf->table_bitmap_size = ((vsize / 512) + 7) / 8;
	bf->table_bitmap = IONew(uint8_t, bf->table_bitmap_size);
	if (!bf->table_bitmap) return (NULL);
	bzero(bf->table_bitmap, bf->table_bitmap_size);

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

	VTLOG("level %d, bmd...0x%llx\n", bf->max_level, alloc);
	md = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
						kIOMemoryPageable |
#if !TABLE_CB
						kIOMapWriteCombineCache |
#endif
						kIOMemoryMapperNone,
						alloc, page_size);
	VTLOG("bmd %p\n", md);
	vtassert(md);
	if (!md) return (NULL);

//	kr = bmd->prepare(kIODirectionOutIn);
//	vtassert(KERN_SUCCESS == kr);

	bf->table_map = md->map();
	vtassert(bf->table_map);
	md->release();

	vtassert(bf->table_map);
	if (!bf->table_map) return (NULL);

	vtd_table_entry_t * table;
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

#if !TABLE_CB
	IOSetProcessorCacheMode(kernel_task, (IOVirtualAddress) &bf->tables[0][0], page_size, kIOCopybackCache);
#endif

	VTLOG("tables %p, %p, %p, %p, %p, %p\n", bf->tables[0], bf->tables[1], bf->tables[2], 
						   						bf->tables[3], bf->tables[4], bf->tables[5]);

	bf->root_page = pmap_find_phys(kernel_pmap, (uintptr_t) bf->tables[bf->max_level]);
	if (!bf->root_page) panic("!root_page");
	VTLOG("tree root 0x%llx\n", ptoa_64(bf->root_page));

	vtd_ballocator_init(bf, buddybits);
	bf->rsize = rsize;
	vtd_rballocator_init(bf, rsize, vsize - rsize);

	VTLOG("bsize 0x%x, bsafe 0x%x, breserve 0x%x, rsize 0x%x\n", 
	        (1<<kBPagesLog2), kBPagesSafe, kBPagesReserve, bf->rsize);

	STAT_ADD(bf, vsize, vsize);
	OSData * 
	data = OSData::withBytesNoCopy(&bf->stats, sizeof(bf->stats));
	if (data)
	{
		setProperty("stats", data);
		data->release();
	}

	bf->stamp = 0x100;
	bf->free_mask  = (kFreeQElems - 1);

	return (bf);
}

vtd_baddr_t
AppleVTD::space_alloc(vtd_space_t * bf, vtd_baddr_t size,
					  uint32_t mapOptions, const IODMAMapSpecification * mapSpecification,
					  upl_page_info_t * pageList)
{
	vtd_vaddr_t addr;
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

		if (mapSpecification->alignment > page_size) align = atop_64(mapSpecification->alignment);
	}

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

#if 0
	IOSimpleLockLock(fHWLock);
	checkFree(uselarge);
	IOSimpleLockUnlock(fHWLock);
#endif

	do
	{
		if (uselarge)
		{
			IOLockLock(bf->rlock);
			addr = vtd_rballoc(bf, size, align, fMaxRoundSize, mapOptions, pageList);
			STAT_ADD(bf, allocs[list], 1);
			if (addr)
			{
				STAT_ADD(bf, rused, size);
				vtd_space_fault(bf, addr, size);
			}
			IOLockUnlock(bf->rlock);
			if (addr && pageList) vtd_space_set(bf, addr, size, mapOptions, pageList);
		}
		else
		{
			BLOCK(bf->block);
			addr = vtd_balloc(bf, size, mapOptions, pageList);
			STAT_ADD(bf, allocs[list], 1);
			if (addr) STAT_ADD(bf, bused, (1 << list));
			BUNLOCK(bf->block);
		}
		if (addr) break;
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
		IOLockLock(bf->rlock);
		vtd_rbfree(bf, addr, size, fMaxRoundSize);
		STAT_ADD(bf, rused, -size);
		IOLockUnlock(bf->rlock);
	}
	else
	{
		list = vtd_log2up(size);
		BLOCK(bf->block);
		vtd_bfree(bf, addr, size);
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
AppleVTD::space_alloc_fixed(vtd_space_t * bf, vtd_baddr_t addr, vtd_baddr_t size)
{
	vtd_balloc_fixed(bf, addr, size);
	vtd_rballoc_fixed(bf, addr, size);
	vtd_space_fault(bf, addr, size);
}

static page_entry_t
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

		if (!(kPageAccess & entry))
			break;
		level++;
	}

	return (entry);
}

bool
AppleVTD::initHardware(IOService *provider)
{
	uint32_t idx;
	vtd_unit_t * unit;

    fIsSystem = true;

	uint64_t context_width;
	fTreeBits = 0;
	unit = units[0];
	// prefer smallest tree?
	for (context_width = kAddressWidth30; 
			(context_width <= kAddressWidth64);
			context_width++)
	{
		if ((0x100 << context_width) & unit->regs->capability)
		{
			fTreeBits = (30 + 9 * context_width);  // (57+9) for 64
			break;
		}
	}

	for (idx = 0; (unit = units[idx]); idx++)
	{	
		if (!((0x100 << context_width) & unit->regs->capability))
			panic("!tree bits %d on unit %d", fTreeBits, idx);
		if (unit->selective && ((unit->rounding > fMaxRoundSize)))
			fMaxRoundSize = unit->rounding;
	}

	VTLOG("context_width %lld, treebits %d, round %d\n",
			context_width, fTreeBits, fMaxRoundSize);

    // need better legacy checks
	if (!fMaxRoundSize)                              return (false);
	if ((48 == fTreeBits) && (9 == fMaxRoundSize))   return (false);
	//

	fHWLock = IOSimpleLockAlloc();

	fSpace = space_create(fCacheLineSize, fTreeBits, kVPages, kBPagesLog2, kRPages);
	if (!fSpace) return (false);

	space_alloc_fixed(fSpace, atop_64(0xfee00000), atop_64(0xfef00000-0xfee00000));
	vtd_space_fault(fSpace, atop_64(0xfee00000), 1);
	fSpace->tables[0][atop_64(0xfee00000)].bits = 0xfee00000 | kPageAccess;

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
		
				space_alloc_fixed(fSpace, atop_64(addr), count);
				for (; count; addr += page_size, count--)
				{
					fSpace->tables[0][atop_64(addr)].bits = (addr | kPageAccess);
				}
				break;
		}
	}

	IOReturn kr;
	IOBufferMemoryDescriptor *
	md = IOBufferMemoryDescriptor::inTaskWithOptions(TASK_NULL,
						kIOMemoryPageable |
						kIOMapWriteCombineCache |
						kIOMemoryMapperNone,
						2 * page_size, page_size);
	vtassert(md);
	if (!md) return (kIOReturnNoMemory);

	kr = md->prepare(kIODirectionOutIn);
	vtassert(KERN_SUCCESS == kr);

	fContextTableMap = md->map();
	vtassert(fContextTableMap);
	md->release();

    // context entries

	context_entry_t * context_entry_table = (typeof(context_entry_table)) fContextTableMap->getVirtualAddress();
	for (idx = 0; idx < 256; idx++)
	{
		context_entry_table[idx].address_space_root = 	ptoa_64(fSpace->root_page)
														| kEntryPresent
														| kTranslationType0;
		context_entry_table[idx].context_entry = context_width
												| kTheDomain*kDomainIdentifier1;
//		if (idx == ((2<<3)|0)) context_entry_table[idx].address_space_root |= kTranslationType2;  // passthru
//		if (idx == ((27<<3)|0)) context_entry_table[idx].address_space_root = 0;
		if (!(kIOPCIConfiguratorIGIsMapped & gIOPCIFlags))
		{
			if (idx == ((2<<3)|0)) context_entry_table[idx].address_space_root &= ~kEntryPresent;
		}
	}
	ppnum_t context_page = pmap_find_phys(kernel_pmap, (uintptr_t) &context_entry_table[0]);
	if (!context_page) panic("!context_page");

	// root

	root_entry_t * root_entry_table = (typeof(root_entry_table)) (fContextTableMap->getAddress() + page_size);
	for (idx = 0; idx < 256; idx++)
	{
		root_entry_table[idx].context_entry_ptr = ptoa_64(context_page)
													| kEntryPresent;
		root_entry_table[idx].resv = 0;
	}

	fRootEntryPage = pmap_find_phys(kernel_pmap, (uintptr_t) &root_entry_table[0]);
	if (!fRootEntryPage) panic("!fRootEntryPage");
	for (idx = 0; (unit = units[idx]); idx++) 
	{
		unit->root = ptoa_64(fRootEntryPage);
	}

	// QI

	for (idx = 0; (unit = units[idx]); idx++) 
	{
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
		unit->qi_mask    = (kQIPageCount * 256) - 1;
		unit->qi_table   = (typeof(unit->qi_table)) (unit->qi_map->getAddress());
		unit->qi_address = vtd_log2down(kQIPageCount)
					     | md->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);

		ppnum_t stamp_page = pmap_find_phys(kernel_pmap, (uintptr_t) &unit->qi_stamp);
		vtassert(stamp_page);
		unit->qi_stamp_address = ptoa_64(stamp_page) | (page_mask & ((uintptr_t) &unit->qi_stamp));

		md->release();
    }

	//

	IOReturn  ret;
	uint64_t  msiAddress;
	uint32_t  msiData;
	ret = gIOPCIMessagedInterruptController->allocateDeviceInterrupts(
													this, 2, 0, &msiAddress, &msiData);
	if (kIOReturnSuccess == ret)
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
		unit_enable(unit);
	}
	if (fIntES)   fIntES->enable();
	if (fFaultES) fFaultES->enable();
	
//	fTimerES->setTimeoutMS(10);

	setProperty(kIOPlatformQuiesceActionKey, INT32_MAX - 1000, 64);
	setProperty(kIOPlatformActiveActionKey, INT32_MAX - 1000, 64);

	registerService();

	return (true);
}

IOReturn
AppleVTD::handleInterrupt(IOInterruptEventSource * source, int count)
{
	uint32_t idx;
	vtd_unit_t * unit;

	IOSimpleLockLock(fHWLock);
	for (idx = 0; idx < kFreeQCount; idx++) checkFree(idx);
	for (idx = 0; (unit = units[idx]); idx++) 
	{
		unit->regs->invalidation_completion_status = 1;
	}
	IOSimpleLockUnlock(fHWLock);

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

	IOSimpleLockLock(fHWLock);
	for (idx = 0; idx < kFreeQCount; idx++) checkFree(idx);
	IOSimpleLockUnlock(fHWLock);

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
				unit_enable(unit);
			}
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

ppnum_t 
AppleVTD::iovmMapMemory(
			  OSObject                    * memory,   // dma command or iomd
			  ppnum_t                       offsetPage,
			  ppnum_t                       pageCount,
			  uint32_t                      mapOptions,
			  upl_page_info_t             * pageList,
			  const IODMAMapSpecification * mapSpecification)
{
	vtd_vaddr_t base;

	base = space_alloc(fSpace, pageCount, mapOptions, mapSpecification, pageList);
	vtassert((base + pageCount) <= fSpace->vsize);

//	space_free(fSpace, base, pageCount);
//	base = space_alloc(fSpace, pageCount, mapOptions, mapSpecification, pageList);

#if KP
	VTLOG("iovmMapMemory: (0x%x)=0x%x\n", length, (int)base);
#endif

    return (base);
}

ppnum_t
AppleVTD::iovmAlloc(IOItemCount pages)
{
	ppnum_t result;

	result = space_alloc(fSpace, pages, 0, NULL, NULL);
#if KP
	VTLOG("iovmAlloc: 0x%x=0x%x\n", (int)pages, (int)result );
#endif
    return (result);
}

void
AppleVTD::iovmInvalidateSync(ppnum_t addr, IOItemCount pages)
{
	vtd_unit_t * unit;
	unsigned int leaf;
	unsigned int idx;
	uint32_t     wait;
	ppnum_t      unitAddr[kMaxUnits];
	IOItemCount  unitPages[kMaxUnits];
	bool		 more;

	for (idx = 0; (unit = units[idx]); idx++)
	{
		unitAddr[idx] = addr;
		unitPages[idx] = pages;
	}
	leaf = true;

	do
	{
		more = false;
		wait = 0;
		for (idx = 0; (unit = units[idx]); idx++)
		{
			if (unitPages[idx])
			{
				wait |= (1 << idx);
				unit_invalidate(unit, kTheDomain, unitAddr[idx], unit->rounding, leaf);
				if (!unit->selective 
					|| (unitPages[idx] <= (1U << unit->rounding)))
				{
					unitPages[idx] = 0;
				}
				else
				{
					more = true;
					unitPages[idx] -= (1U << unit->rounding);
					unitAddr[idx]  += (1U << unit->rounding);
				}
			}
		}
		for (idx = 0; (unit = units[idx]); idx++)
		{
			if (wait & (1U << idx)) unit_invalidate_done(unit);
		}
	}
	while (more);
}

void
AppleVTD::iovmFree(ppnum_t addr, IOItemCount pages)
{
	vtd_unit_t * unit;
	unsigned int leaf, isLarge;
	unsigned int unitIdx;
    uint32_t     did = kTheDomain;
	ppnum_t      unitAddr;
	IOItemCount  unitPages;
	uint32_t     idx;
	uint32_t     next;
	uint32_t     count;
	uint64_t     stamp;

#if KP
	VTLOG("iovmFree: 0x%x,0x%x\n", (int)pages, addr);
#endif

	vtassert((addr + pages) <= fSpace->vsize);
	vtd_space_nfault(fSpace, addr, pages);
	bzero(&fSpace->tables[0][addr], pages * sizeof(vtd_table_entry_t));
	table_flush(&fSpace->tables[0][addr], pages * sizeof(vtd_table_entry_t), fCacheLineSize);

#if !ENA_QI
	IOSimpleLockLock(fHWLock);
    iovmInvalidateSync(addr, pages);
	IOSimpleLockUnlock(fHWLock);
	space_free(fSpace, addr, pages);
	return;

#else	/* !ENA_QI */

	leaf = true;
	isLarge = (addr >= fSpace->rsize);

	IOSimpleLockLock(fHWLock);

#if 0
	int32_t      freeCount;
	freeCount = fSpace->free_tail[isLarge] - fSpace->free_head[isLarge];
	if (freeCount < 0) freeCount = kFreeQElems - freeCount;
	if (freeCount >= 8)
#endif
	{
		checkFree(isLarge);
	}

	stamp = ++fSpace->stamp;

	idx = fSpace->free_tail[isLarge];
	next = (idx + 1) & fSpace->free_mask;
	if (next == fSpace->free_head[isLarge]) panic("qfull");
	fSpace->free_queue[isLarge][idx].addr = addr;
	fSpace->free_queue[isLarge][idx].size = pages;
	fSpace->free_queue[isLarge][idx].stamp = stamp;
	fSpace->free_tail[isLarge] = next;

	for (unitIdx = 0; (unit = units[unitIdx]); unitIdx++)
	{
		unitAddr = addr;
		unitPages = pages;
		idx = unit->qi_tail;
		count = 0;
		while (unitPages)
		{
			next = (idx + 1) & unit->qi_mask;
			while ((next << 4) == unit->regs->invalidation_queue_head) {}
			
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
				if (!(count & (unit->qi_mask >> 5)))
				{
					__mfence();
					unit->regs->invalidation_queue_tail = (next << 4);
				}
			}
			idx = next;
		}
//		if (freeCount >= 64)
//		if (0 == (stamp & 3))		
		{
			next = (idx + 1) & unit->qi_mask;
			while ((next << 4) == unit->regs->invalidation_queue_head) {}
			uint64_t command = (stamp<<32) | (1<<5) | (5);
//     		command |= (1<<4); // make an int
			unit->qi_table[idx].command = command;
			unit->qi_table[idx].address = unit->qi_stamp_address;
		}
		__mfence();
		unit->regs->invalidation_queue_tail = (next << 4);
//		__mfence();
		unit->qi_tail = next;
	}

	IOSimpleLockUnlock(fHWLock);

#endif /* ENA_QI */
}

#define stampPassed(a,b)	(((int32_t)((a)-(b))) >= 0)

void 
AppleVTD::checkFree(uint32_t isLarge)
{
	vtd_unit_t * unit;
	uint32_t     unitIdx;
	uint32_t     idx;
	uint32_t     next;
	ppnum_t      addr, size, count;
    bool         ok;

	count = 0;
	idx = fSpace->free_head[isLarge];
	do
	{
		if (idx == fSpace->free_tail[isLarge]) break;
		for (unitIdx = 0, ok = true; ok && (unit = units[unitIdx]); unitIdx++)
		{
			ok &= stampPassed(unit->qi_stamp, fSpace->free_queue[isLarge][idx].stamp);
		}
	
		if (ok)
		{
			next = (idx + 1) & fSpace->free_mask;
			addr = fSpace->free_queue[isLarge][idx].addr;
			size = fSpace->free_queue[isLarge][idx].size;
#if BSIMPLE
			if (!isLarge)
		    {
				vtd_bfree(fSpace, addr, size);
				STAT_ADD(fSpace, bused, -size);
				idx = next;
		    }
		    else
#endif /* BSIMPLE */
			{
				fSpace->free_head[isLarge] = next;
				IOSimpleLockUnlock(fHWLock);
				space_free(fSpace, addr, size);
				IOSimpleLockLock(fHWLock);
				idx = fSpace->free_head[isLarge];
		    }
			count++;
		}
	}
	while (ok);

#if BSIMPLE
	fSpace->free_head[isLarge] = idx;
#endif
	if (count > fSpace->stats.max_inval[isLarge]) fSpace->stats.max_inval[isLarge] = count;
}

addr64_t 
AppleVTD::mapAddr(IOPhysicalAddress addr)
{
	ppnum_t      page = atop_64(addr);
	page_entry_t entry;

	if (page >= fSpace->vsize) return (addr);

	if (!vtd_space_present(fSpace, page)) return (addr);

	entry = fSpace->tables[0][page].bits;

#if KP
	VTLOG("mapAddr: 0x%x=0x%llx\n", (int)addr, entry);
#endif

	if (kPageAccess & entry)
		return (trunc_page_64(entry) | (addr & page_mask));
	else
		return (addr);

    return (0);
}

void 
AppleVTD::iovmInsert(ppnum_t addr, IOItemCount offset, ppnum_t page)
{
	addr += offset;
	vtassert(addr < fSpace->vsize);
	vtd_space_nfault(fSpace, addr, 1);
	fSpace->tables[0][addr].bits = ptoa_64(page) | kPageAccess;
	table_flush(&fSpace->tables[0][addr], sizeof(vtd_table_entry_t), fCacheLineSize);
	STAT_ADD(fSpace, inserts, 1);
}


void 
AppleVTD::iovmInsert(ppnum_t addr, IOItemCount offset, 
						ppnum_t *pageList, IOItemCount pageCount)
{
	ppnum_t idx;

	addr += offset;
	vtassert((addr + pageCount) <= fSpace->vsize);
	vtd_space_nfault(fSpace, addr, pageCount);
    for (idx = 0; idx < pageCount; idx++)
    {
		fSpace->tables[0][addr + idx].bits = ptoa_64(pageList[idx]) | kPageAccess;
	}
	table_flush(&fSpace->tables[0][addr], pageCount * sizeof(vtd_table_entry_t), fCacheLineSize);
	STAT_ADD(fSpace, inserts, pageCount);
}

void
AppleVTD::iovmInsert(ppnum_t addr, IOItemCount offset,
					 upl_page_info_t *pageList, IOItemCount pageCount)
{
	ppnum_t idx;

	addr += offset;

	vtassert((addr + pageCount) <= fSpace->vsize);
	vtd_space_nfault(fSpace, addr, pageCount);
    for (idx = 0; idx < pageCount; idx++)
    {
		fSpace->tables[0][addr + idx].bits = ptoa_64(pageList[idx].phys_addr) | kPageAccess;
	}
	table_flush(&fSpace->tables[0][addr], pageCount * sizeof(vtd_table_entry_t), fCacheLineSize);
	STAT_ADD(fSpace, inserts, pageCount);
}

#endif /* ACPI_SUPPORT */

