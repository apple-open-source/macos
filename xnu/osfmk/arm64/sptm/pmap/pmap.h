/*
 * Copyright (c) 2007-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/**
 * Machine-dependent structures for the physical map module.
 *
 * This header file contains the types and prototypes that make up the public
 * pmap API that's exposed to the rest of the kernel. Any types/prototypes used
 * strictly by the pmap itself should be placed into one of the osfmk/arm/pmap/
 * header files.
 *
 * To prevent circular dependencies and exposing anything not needed by the
 * rest of the kernel, this file shouldn't include ANY of the internal
 * osfmk/arm/pmap/ header files.
 */
#ifndef _ARM_PMAP_H_
#define _ARM_PMAP_H_

#include <mach_assert.h>

#include <arm64/proc_reg.h>

#ifndef ASSEMBLER

#include <stdatomic.h>
#include <stdbool.h>
#include <libkern/section_keywords.h>
#include <mach/kern_return.h>
#include <mach/machine/vm_types.h>
#include <arm64/sptm/pmap/pmap_public.h>
#include <kern/ast.h>
#include <mach/arm/thread_status.h>
#include <os/refcnt.h>

#include <arm64/tlb.h>


/* Shift for 2048 max virtual ASIDs (2048 pmaps). */
#define ASID_SHIFT (11)

/* Max supported ASIDs (can be virtual). */
#define MAX_ASIDS (1 << ASID_SHIFT)

/* Shift for the maximum ARM ASID value (256 or 65536) */
#ifndef ARM_ASID_SHIFT
#if HAS_16BIT_ASID
#define ARM_ASID_SHIFT (16)
#else
#define ARM_ASID_SHIFT (8)
#endif /* HAS_16BIT_ASID */
#endif /* ARM_ASID_SHIFT */

/* Max ASIDs supported by the hardware. */
#define ARM_MAX_ASIDS (1 << ARM_ASID_SHIFT)

/* Number of bits in a byte. */
#define NBBY (8)

/**
 * The maximum number of hardware ASIDs used by the pmap for user address spaces.
 *
 * One ASID is always dedicated to the kernel (ASID 0). On systems with software-
 * based spectre/meltdown mitigations, each address space technically uses two
 * hardware ASIDs (one for EL1 and one for EL0) so the total number of available
 * ASIDs a user process can use is halved on those systems.
 */
#if __ARM_KERNEL_PROTECT__
#define MAX_HW_ASIDS (ARM_MAX_ASIDS >> 1)
#else
#define MAX_HW_ASIDS ARM_MAX_ASIDS
#endif /* __ARM_KERNEL_PROTECT__ */

/**
 * Maximum number of Virtual Machine IDs.
 *
 * All even number physical VMIDs are reserved for SK usage. Thus only 128
 * logical VMIDs are available. Software will convert the logical VMID to
 * the proper odd numbered physical VMID when allocating/freeing VMIDs.
 */
#ifndef ARM_VMID_SHIFT
#define ARM_VMID_SHIFT (7)
#endif /* ARM_VMID_SHIFT */
#define ARM_MAX_VMIDS  (1 << ARM_VMID_SHIFT)

/* XPRR virtual register map */

/* Maximum number of CPU windows per-cpu. */
#define CPUWINDOWS_MAX 4


#if defined(ARM_LARGE_MEMORY)
/*
 * 2 L1 tables (Linear KVA and V=P), plus 2*16 L2 tables map up to (16*64GB) 1TB of DRAM
 * Upper limit on how many pages can be consumed by bootstrap page tables
 */
#define BOOTSTRAP_TABLE_SIZE (ARM_PGBYTES * 34)
#else /* defined(ARM_LARGE_MEMORY) */
#define BOOTSTRAP_TABLE_SIZE (ARM_PGBYTES * 8)
#endif /* defined(ARM_LARGE_MEMORY) */

typedef uint64_t tt_entry_t; /* translation table entry type */
typedef uint64_t pt_entry_t; /* page table entry type */

/* Used to represent a NULL page/translation table entry pointer. */
#define PT_ENTRY_NULL ((pt_entry_t *) 0)
#define TT_ENTRY_NULL ((tt_entry_t *) 0)

/**
 * Number of PTE pointers in a single PVE. This must be 2, since the algorithm
 * has been optimized to that case. Should this change in the future, both
 * enter_pv() and remove_pv() will need to be modified accordingly. In addition
 * to this, the documentation and the LLDB macros that walk PV lists will also
 * need to be adapted.
 */
#define PTE_PER_PVE 2
_Static_assert(PTE_PER_PVE == 2, "PTE_PER_PVE is not 2");

/**
 * Structure to track the active mappings for a given page. This structure is
 * used in the pv_head_table when a physical page has more than one mapping to
 * it. Each entry in this linked list of structures can represent
 * up to PTE_PER_PVE mappings.
 */
typedef struct pv_entry {
	/* Linked list to the next mapping of the physical page. */
	struct pv_entry *pve_next;

	/* Pointer to the page table entry for this mapping. */
	pt_entry_t *pve_ptep[PTE_PER_PVE];
} pv_entry_t;

/**
 * Structure that tracks free pv_entry nodes for the pv_head_table. Each one
 * of these nodes represents a single mapping to a physical page, so a new node
 * is allocated whenever a new mapping is created.
 */
typedef struct {
	pv_entry_t *list;
	uint32_t count;
} pv_free_list_t;

/**
 * Forward declaration of the structure that controls page table geometry and
 * TTE/PTE format.
 */
struct page_table_attr;

struct pmap_cpu_data {
	unsigned int cpu_number;
	bool copywindow_strong_sync[CPUWINDOWS_MAX];
	bool inflight_disconnect;
	pv_free_list_t pv_free;
	pv_entry_t *pv_free_spill_marker;
};
typedef struct pmap_cpu_data pmap_cpu_data_t;

#include <mach/vm_prot.h>
#include <mach/vm_statistics.h>
#include <mach/machine/vm_param.h>
#include <kern/kern_types.h>
#include <kern/thread.h>
#include <kern/queue.h>


#include <sys/cdefs.h>

/* Base address for low globals. */
#if defined(ARM_LARGE_MEMORY)
#define LOW_GLOBAL_BASE_ADDRESS 0xfffffe0000000000ULL
#else /* defined(ARM_LARGE_MEMORY) */
#define LOW_GLOBAL_BASE_ADDRESS 0xfffffff000000000ULL
#endif /* defined(ARM_LARGE_MEMORY) */

/*
 * This indicates (roughly) where there is free space for the VM
 * to use for the heap; this does not need to be precise.
 */
#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
#if defined(ARM_LARGE_MEMORY)
#define KERNEL_PMAP_HEAP_RANGE_START (VM_MIN_KERNEL_AND_KEXT_ADDRESS+ARM_TT_L1_SIZE)
#else /* defined(ARM_LARGE_MEMORY) */
#define KERNEL_PMAP_HEAP_RANGE_START VM_MIN_KERNEL_AND_KEXT_ADDRESS
#endif /* defined(ARM_LARGE_MEMORY) */
#else /* defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR) */
#if defined(ARM_LARGE_MEMORY)
/* For large memory systems with no KTRR/CTRR such as virtual machines */
#define KERNEL_PMAP_HEAP_RANGE_START (VM_MIN_KERNEL_AND_KEXT_ADDRESS+ARM_TT_L1_SIZE)
#else
#define KERNEL_PMAP_HEAP_RANGE_START LOW_GLOBAL_BASE_ADDRESS
#endif
#endif /* defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR) */

/**
 * For setups where the VM page size does not match the hardware page size (the
 * VM page size must be a multiple of the hardware page size), we will need to
 * determine what the page ratio is.
 */
#define PAGE_RATIO        ((1 << PAGE_SHIFT) >> ARM_PGSHIFT)
#define TEST_PAGE_RATIO_4 (PAGE_RATIO == 4)



/* superpages */
#define SUPERPAGE_NBASEPAGES 1 /* No superpages support */

/* Convert addresses to pages and vice versa. No rounding is used. */
#define arm_atop(x) (((vm_map_address_t)(x)) >> ARM_PGSHIFT)
#define arm_ptoa(x) (((vm_map_address_t)(x)) << ARM_PGSHIFT)

/**
 * Round off or truncate to the nearest page. These will work for either
 * addresses or counts (i.e. 1 byte rounds to 1 page bytes).
 */
#define arm_round_page(x) ((((vm_map_address_t)(x)) + ARM_PGMASK) & ~ARM_PGMASK)
#define arm_trunc_page(x) (((vm_map_address_t)(x)) & ~ARM_PGMASK)

extern void flush_mmu_tlb_region(vm_offset_t va, unsigned length);

extern uint64_t get_mmu_control(void);
extern uint64_t get_aux_control(void);
extern void set_aux_control(uint64_t);
extern void set_mmu_ttb(uint64_t);
extern void set_mmu_ttb_alternate(uint64_t);
extern uint64_t get_tcr(void);
extern void set_tcr(uint64_t);
extern uint64_t pmap_get_arm64_prot(pmap_t, vm_offset_t);


extern pmap_paddr_t get_mmu_ttb(void);
extern pmap_paddr_t mmu_kvtop(vm_offset_t va);
extern pmap_paddr_t mmu_kvtop_wpreflight(vm_offset_t va);
extern pmap_paddr_t mmu_uvtop(vm_offset_t va);


/* Convert address offset to translation table index */
#define ttel0num(a)         ((a & ARM_TTE_L0_MASK) >> ARM_TT_L0_SHIFT)
#define ttel1num(a)         ((a & ARM_TTE_L1_MASK) >> ARM_TT_L1_SHIFT)
#define ttel2num(a)         ((a & ARM_TTE_L2_MASK) >> ARM_TT_L2_SHIFT)

#define pa_to_tte(a)        ((a) & ARM_TTE_TABLE_MASK)
#define tte_to_pa(p)        ((p) & ARM_TTE_TABLE_MASK)

#define pa_to_pte(a)        ((a) & ARM_PTE_PAGE_MASK)
#define pte_to_pa(p)        ((p) & ARM_PTE_PAGE_MASK)
#define pte_to_ap(p)        (((p) & ARM_PTE_APMASK) >> ARM_PTE_APSHIFT)
#define pte_increment_pa(p) ((p) += ptoa(1))

#define TLBFLUSH_SIZE       (ARM_TTE_MAX/((sizeof(unsigned int))*BYTE_SIZE))


#define pmap_cs_log(level, fmt, args...)
#define pmap_cs_log_debug(fmt, args...)
#define pmap_cs_log_info(fmt, args...)
#define pmap_cs_log_error(fmt, args...)
#define pmap_cs_log_force(level, fmt, args...)


/**
 * Wrapper struct that represents a locked entry in the PV head table.
 * This struct should only be obtained as the return value from pvh_try_lock()
 * or the pvh_lock* functions.
 */
typedef struct {
	/* The pv_head_table entry obtained from the lock operation. */
	uintptr_t pvh;
	/**
	 * Token obtained from thread_priority_floor_start(), the lock was
	 * placed in sleep mode using pvh_lock_enter_sleep_mode().
	 */
	thread_pri_floor_t pri_token;
	/* The index of the locked physical page. */
	unsigned int pai;
} locked_pvh_t;



/* Convert translation/page table entry to kernel virtual address. */
#define ttetokv(a) (phystokv(tte_to_pa(a)))
#define ptetokv(a) (phystokv(pte_to_pa(a)))

struct pmap {
	/* Pointer to the root translation table. */
	tt_entry_t *tte;

	/* Physical page of the root translation table. */
	pmap_paddr_t ttep;

	/*
	 * The min and max fields represent the lowest and highest addressable VAs
	 * as dictated strictly by the paging hierarchy (root level + root table size)
	 * in conjunction with whether the root table is used with TTBR0, TTBR1, or VTTBR.
	 * These fields do not encapsulate any higher-level address-space partitioning
	 * policies.
	 */

	/* Lowest supported VA (inclusive) */
	vm_map_address_t min;

	/* Highest supported VA (exclusive) */
	vm_map_address_t max;

#if ARM_PARAMETERIZED_PMAP
	/* Details about the page table layout. */
	const struct page_table_attr * pmap_pt_attr;
#endif /* ARM_PARAMETERIZED_PMAP */

	/* Ledger tracking phys mappings */
	ledger_t ledger;

	decl_lck_rw_data(, rwlock);

	/* Global list of pmaps */
	queue_chain_t pmaps;

	/* Information representing the "nested" (shared) region in this pmap. */
	struct pmap      *nested_pmap;
	vm_map_address_t nested_region_addr;
	vm_map_offset_t  nested_region_size;
	vm_map_offset_t  nested_region_true_start;
	vm_map_offset_t  nested_region_true_end;
	bitmap_t         *nested_region_unnested_table_bitmap;

	/* PMAP reference count */
	os_ref_atomic_t ref_count;

	/* Number of pmaps that nested this pmap without bounds set. */
	uint32_t nested_no_bounds_refcnt;

	union {
		/**
		 * Represents the address space identifier (ASID) for this pmap.
		 * The value 0 is reserved for the kernel pmap; this field will
		 * also be 0 for nested pmaps as those pmaps are never directly
		 * activated on a CPU.  This represents a virtual ASID that
		 * is used to globally identify an address space on
		 * the system.  Depending upon hardware configuration, this
		 * identifier may have a 1:1 correspondence with the hardware
		 * ASID.
		 */
		uint16_t asid;

		/**
		 * Represents the virtual machine identifier (VMID) for this pmap.
		 * The value 0 is reserved.
		 */
		uint16_t vmid;
	};

#if MACH_ASSERT
	int pmap_pid;
	char pmap_procname[17];
#endif /* MACH_ASSERT */

	bool reserved0;

	bool pmap_vm_map_cs_enforced;

	bool reserved1;
	unsigned int reserved2;
	unsigned int reserved3;

#if defined(CONFIG_ROSETTA)
	/* Whether the pmap is used for Rosetta. */
	bool is_rosetta;
#else
	bool reserved4;
#endif /* defined(CONFIG_ROSETTA) */

#if DEVELOPMENT || DEBUG
	bool footprint_suspended;
	bool footprint_was_suspended;
#endif /* DEVELOPMENT || DEBUG */

	/* Whether the No-Execute functionality is enabled. */
	bool nx_enabled;

	/* Whether this pmap represents a 64-bit address space. */
	bool is_64bit;

	/* Nested a pmap when the bounds were not set. */
	bool nested_has_no_bounds_ref;

	/* The nesting bounds have been set. */
	bool nested_bounds_set;

#if HAS_APPLE_PAC
	bool disable_jop;
#else
	bool reserved5;
#endif /* HAS_APPLE_PAC */

	bool reserved6;

#define PMAP_TYPE_USER 0 /* ordinary pmap */
#define PMAP_TYPE_KERNEL 1 /* kernel pmap */
#define PMAP_TYPE_COMMPAGE 2 /* commpage pmap */
#define PMAP_TYPE_NESTED 3 /* pmap nested within another pmap */
	uint8_t type;

	/*
	 * TrustedExecutionMonitor manages its own address space data structure and
	 * the PMAP is used as the owning structure for keeping this structure.
	 */
	uint32_t reserved7[4];
	void *reserved8;
	uint8_t reserved9;
};

#define PMAP_VASID(pmap) ((pmap)->asid)
#define PMAP_HWASID(pmap) ((pmap)->asid & (MAX_HW_ASIDS - 1))

#if VM_DEBUG
extern int pmap_list_resident_pages(
	pmap_t pmap,
	vm_offset_t *listp,
	int space);
#else /* VM_DEBUG */
#define pmap_list_resident_pages(pmap, listp, space) (0)
#endif /* VM_DEBUG */

extern int copysafe(vm_map_address_t from, vm_map_address_t to, uint32_t cnt, int type, uint32_t *bytes_copied);

/* Globals shared between arm_vm_init and pmap */
extern tt_entry_t *cpu_tte;   /* First CPUs translation table (shared with kernel pmap) */
extern pmap_paddr_t cpu_ttep; /* Physical translation table addr */

extern void *ropagetable_begin;
extern void *ropagetable_end;


extern tt_entry_t *invalid_tte; /* Global invalid translation table */
extern pmap_paddr_t invalid_ttep; /* Physical invalid translation table addr */

#define PMAP_CONTEXT(pmap, thread)

/**
 * Platform dependent Prototypes
 */
extern void pmap_clear_user_ttb(void);
extern void pmap_bootstrap(vm_offset_t);
extern vm_map_address_t pmap_ptov(pmap_t, ppnum_t);
extern pmap_paddr_t pmap_find_pa(pmap_t map, addr64_t va);
extern pmap_paddr_t pmap_find_pa_nofault(pmap_t map, addr64_t va);
extern ppnum_t pmap_find_phys(pmap_t map, addr64_t va);
extern ppnum_t pmap_find_phys_nofault(pmap_t map, addr64_t va);
extern void pmap_switch_user(thread_t th, vm_map_t map);
extern void pmap_set_pmap(pmap_t pmap, thread_t thread);
extern  void pmap_gc(void);
#if HAS_APPLE_PAC
extern void * pmap_sign_user_ptr(void *value, ptrauth_key key, uint64_t data, uint64_t jop_key);
extern void * pmap_auth_user_ptr(void *value, ptrauth_key key, uint64_t data, uint64_t jop_key);
#endif /* HAS_APPLE_PAC */

/**
 * Interfaces implemented as macros.
 */

#define PMAP_SWITCH_USER(th, new_map, my_cpu) pmap_switch_user((th), (new_map))

#define pmap_kernel() (kernel_pmap)

#define pmap_kernel_va(VA) \
	(((VA) >= VM_MIN_KERNEL_ADDRESS) && ((VA) <= VM_MAX_KERNEL_ADDRESS))

#define pmap_attribute(pmap, addr, size, attr, value) (KERN_INVALID_ADDRESS)

#define copyinmsg(from, to, cnt) copyin(from, to, cnt)
#define copyoutmsg(from, to, cnt) copyout(from, to, cnt)

/* Unimplemented interfaces. */
#define MACRO_NOOP
#define pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr) MACRO_NOOP
#define pmap_pageable(pmap, start, end, pageable) MACRO_NOOP

extern pmap_paddr_t kvtophys(vm_offset_t va);
extern pmap_paddr_t kvtophys_nofail(vm_offset_t va);
extern vm_map_address_t phystokv(pmap_paddr_t pa);
extern vm_map_address_t phystokv_range(pmap_paddr_t pa, vm_size_t *max_len);

extern vm_map_address_t pmap_map(vm_map_address_t va, vm_offset_t sa, vm_offset_t ea, vm_prot_t prot, unsigned int flags);
extern vm_map_address_t pmap_map_high_window_bd( vm_offset_t pa, vm_size_t len, vm_prot_t prot);
extern kern_return_t pmap_map_block(pmap_t pmap, addr64_t va, ppnum_t pa, uint32_t size, vm_prot_t prot, int attr, unsigned int flags);
extern kern_return_t pmap_map_block_addr(pmap_t pmap, addr64_t va, pmap_paddr_t pa, uint32_t size, vm_prot_t prot, int attr, unsigned int flags);
extern void pmap_map_globals(void);

#define PMAP_MAP_BD_DEVICE                    0x0
#define PMAP_MAP_BD_WCOMB                     0x1
#define PMAP_MAP_BD_POSTED                    0x2
#define PMAP_MAP_BD_POSTED_REORDERED          0x3
#define PMAP_MAP_BD_POSTED_COMBINED_REORDERED 0x4
#define PMAP_MAP_BD_MASK                      0x7

extern vm_map_address_t pmap_map_bd_with_options(vm_map_address_t va, vm_offset_t sa, vm_offset_t ea, vm_prot_t prot, int32_t options);
extern vm_map_address_t pmap_map_bd(vm_map_address_t va, vm_offset_t sa, vm_offset_t ea, vm_prot_t prot);

extern void pmap_init_pte_page(pmap_t, pt_entry_t *, vm_offset_t, unsigned int ttlevel, boolean_t alloc_ptd);

extern boolean_t pmap_valid_address(pmap_paddr_t addr);
extern void pmap_disable_NX(pmap_t pmap);
extern void pmap_set_nested(pmap_t pmap);
extern void pmap_create_commpages(vm_map_address_t *kernel_data_addr, vm_map_address_t *kernel_text_addr,
    vm_map_address_t *kernel_ro_data_addr, vm_map_address_t *user_text_addr);
extern void pmap_insert_commpage(pmap_t pmap);

extern vm_offset_t pmap_cpu_windows_copy_addr(int cpu_num, unsigned int index);
extern unsigned int pmap_map_cpu_windows_copy(ppnum_t pn, vm_prot_t prot, unsigned int wimg_bits);
extern void pmap_unmap_cpu_windows_copy(unsigned int index);

extern vm_offset_t pmap_ro_zone_align(vm_offset_t);
extern void pmap_ro_zone_memcpy(zone_id_t zid, vm_offset_t va, vm_offset_t offset,
    vm_offset_t new_data, vm_size_t new_data_size);
extern uint64_t pmap_ro_zone_atomic_op(zone_id_t zid, vm_offset_t va, vm_offset_t offset,
    uint32_t op, uint64_t value);
extern void pmap_ro_zone_bzero(zone_id_t zid, vm_offset_t va, vm_offset_t offset, vm_size_t size);

extern boolean_t pmap_valid_page(ppnum_t pn);
extern boolean_t pmap_bootloader_page(ppnum_t pn);

extern boolean_t pmap_is_empty(pmap_t pmap, vm_map_offset_t start, vm_map_offset_t end);


#define ARM_PMAP_MAX_OFFSET_DEFAULT 0x01
#define ARM_PMAP_MAX_OFFSET_MIN     0x02
#define ARM_PMAP_MAX_OFFSET_MAX     0x04
#define ARM_PMAP_MAX_OFFSET_DEVICE  0x08
#define ARM_PMAP_MAX_OFFSET_JUMBO   0x10
#if XNU_PLATFORM_iPhoneOS && EXTENDED_USER_VA_SUPPORT
#define ARM_PMAP_MAX_OFFSET_EXTRA_JUMBO 0x20
#endif /* XNU_PLATFORM_iPhoneOS && EXTENDED_USER_VA_SUPPORT */

extern vm_map_offset_t pmap_max_offset(boolean_t is64, unsigned int option);
extern vm_map_offset_t pmap_max_64bit_offset(unsigned int option);
extern vm_map_offset_t pmap_max_32bit_offset(unsigned int option);

boolean_t pmap_virtual_region(unsigned int region_select, vm_map_offset_t *startp, vm_map_size_t *size);

boolean_t pmap_enforces_execute_only(pmap_t pmap);

void pmap_abandon_measurement(void);



/* pmap dispatch indices */
#define ARM_FAST_FAULT_INDEX 0
#define ARM_FORCE_FAST_FAULT_INDEX 1
#define MAPPING_FREE_PRIME_INDEX 2
#define MAPPING_REPLENISH_INDEX 3
#define PHYS_ATTRIBUTE_CLEAR_INDEX 4
#define PHYS_ATTRIBUTE_SET_INDEX 5
#define PMAP_BATCH_SET_CACHE_ATTRIBUTES_INDEX 6
#define PMAP_CHANGE_WIRING_INDEX 7
#define PMAP_CREATE_INDEX 8
#define PMAP_DESTROY_INDEX 9
#define PMAP_ENTER_OPTIONS_INDEX 10
/* #define PMAP_EXTRACT_INDEX 11 -- Not used*/
#define PMAP_FIND_PA_INDEX 12
#define PMAP_INSERT_COMMPAGE_INDEX 13
#define PMAP_IS_EMPTY_INDEX 14
#define PMAP_MAP_CPU_WINDOWS_COPY_INDEX 15
#define PMAP_MARK_PAGE_AS_PMAP_PAGE_INDEX 16
#define PMAP_NEST_INDEX 17
#define PMAP_PAGE_PROTECT_OPTIONS_INDEX 18
#define PMAP_PROTECT_OPTIONS_INDEX 19
#define PMAP_QUERY_PAGE_INFO_INDEX 20
#define PMAP_QUERY_RESIDENT_INDEX 21
#define PMAP_REFERENCE_INDEX 22
#define PMAP_REMOVE_OPTIONS_INDEX 23
#define PMAP_SET_CACHE_ATTRIBUTES_INDEX 25
#define PMAP_SET_NESTED_INDEX 26
#define PMAP_SET_PROCESS_INDEX 27
#define PMAP_SWITCH_INDEX 28
#define PMAP_SWITCH_USER_TTB_INDEX 29
#define PMAP_CLEAR_USER_TTB_INDEX 30
#define PMAP_UNMAP_CPU_WINDOWS_COPY_INDEX 31
#define PMAP_UNNEST_OPTIONS_INDEX 32
#define PMAP_FOOTPRINT_SUSPEND_INDEX 33
#define PMAP_CPU_DATA_INIT_INDEX 34
#define PMAP_RELEASE_PAGES_TO_KERNEL_INDEX 35
#define PMAP_SET_JIT_ENTITLED_INDEX 36


#define PMAP_UPDATE_COMPRESSOR_PAGE_INDEX 55
#define PMAP_TRIM_INDEX 56
#define PMAP_LEDGER_VERIFY_SIZE_INDEX 57
#define PMAP_LEDGER_ALLOC_INDEX 58
#define PMAP_LEDGER_FREE_INDEX 59

#if HAS_APPLE_PAC
#define PMAP_SIGN_USER_PTR 60
#define PMAP_AUTH_USER_PTR 61
#endif /* HAS_APPLE_PAC */

#define PHYS_ATTRIBUTE_CLEAR_RANGE_INDEX 66


#if __has_feature(ptrauth_calls) && (defined(XNU_TARGET_OS_OSX) || (DEVELOPMENT || DEBUG))
#define PMAP_DISABLE_USER_JOP_INDEX 69
#endif /* __has_feature(ptrauth_calls) && (defined(XNU_TARGET_OS_OSX) || (DEVELOPMENT || DEBUG)) */



#define PMAP_SET_VM_MAP_CS_ENFORCED_INDEX 72

#define PMAP_SET_COMPILATION_SERVICE_CDHASH_INDEX 73
#define PMAP_MATCH_COMPILATION_SERVICE_CDHASH_INDEX 74
#define PMAP_NOP_INDEX 75

#define PMAP_RO_ZONE_MEMCPY_INDEX 76
#define PMAP_RO_ZONE_ATOMIC_OP_INDEX 77

#if DEVELOPMENT || DEBUG
#define PMAP_TEST_TEXT_CORRUPTION_INDEX 79
#endif /* DEVELOPMENT || DEBUG */



#define PMAP_SET_LOCAL_SIGNING_PUBLIC_KEY_INDEX 84
#define PMAP_UNRESTRICT_LOCAL_SIGNING_INDEX 85



#define PMAP_RO_ZONE_BZERO_INDEX 90




#define PMAP_SET_TPRO_INDEX 98

#define PMAP_COUNT 99

/**
 * Value used when initializing pmap per-cpu data to denote that the structure
 * hasn't been initialized with its associated CPU number yet.
 */
#define PMAP_INVALID_CPU_NUM (~0U)

/**
 * Align the pmap per-cpu data to the L2 cache size for each individual CPU's
 * data. This prevents accesses from one CPU affecting another, especially
 * when atomically updating fields.
 */
struct pmap_cpu_data_array_entry {
	pmap_cpu_data_t cpu_data;
} __attribute__((aligned(MAX_L2_CLINE_BYTES)));

/* Initialize the pmap per-CPU data for the current CPU. */
extern void pmap_cpu_data_init(void);

/* Get the pmap per-CPU data for the current CPU. */
extern pmap_cpu_data_t *pmap_get_cpu_data(void);

/* Get the pmap per-CPU data for an arbitrary CPU. */
extern pmap_cpu_data_t *pmap_get_remote_cpu_data(unsigned int cpu);

/*
 * For long-running PV list operations, we pick a reasonable maximum chunk size
 * beyond which we will exit to preemptible context to avoid excessive preemption
 * latency and PVH lock timeouts.
 */
#define PMAP_MAX_PV_LIST_CHUNK_SIZE 64

/*
 * For most batched page operations, we pick a sane default page count
 * interval at which to check for pending preemption and exit the PPL if found.
 */
#define PMAP_DEFAULT_PREEMPTION_CHECK_PAGE_INTERVAL 64

static inline bool
_pmap_pending_preemption_real(void)
{
	return !!(*((volatile ast_t*)ast_pending()) & AST_URGENT);
}

#if SCHED_HYGIENE_DEBUG && (DEBUG || DEVELOPMENT)
bool pmap_pending_preemption(void); // more complicated, so externally defined
#else /* SCHED_HYGIENE_DEBUG && (DEBUG || DEVELOPMENT) */
#define pmap_pending_preemption _pmap_pending_preemption_real
#endif /* SCHED_HYGIENE_DEBUG && (DEBUG || DEVELOPMENT) */

#define MARK_AS_PMAP_TEXT
#define MARK_AS_PMAP_DATA
#define MARK_AS_PMAP_RODATA

extern void pmap_nop(pmap_t);

extern lck_grp_t pmap_lck_grp;

extern void CleanPoC_DcacheRegion_Force_nopreempt_nohid_nobarrier(vm_offset_t va, size_t length);

#define pmap_force_dcache_clean(va, sz) CleanPoC_DcacheRegion_Force(va, sz)
#define pmap_simple_lock(l)             simple_lock(l, &pmap_lck_grp)
#define pmap_simple_unlock(l)           simple_unlock(l)
#define pmap_simple_lock_try(l)         simple_lock_try(l, &pmap_lck_grp)
#define pmap_simple_lock_assert(l, t)   simple_lock_assert(l, t)

#if DEVELOPMENT || DEBUG
extern kern_return_t pmap_test_text_corruption(pmap_paddr_t);
#endif /* DEVELOPMENT || DEBUG */

/* Check if a page has any mappings. */
extern bool pmap_is_page_free(pmap_paddr_t paddr);

#endif /* #ifndef ASSEMBLER */

#if __ARM_KERNEL_PROTECT__
/*
 * The exception vector mappings start at the middle of the kernel page table
 * range (so that the EL0 mapping can be located at the base of the range).
 */
#define ARM_KERNEL_PROTECT_EXCEPTION_START ((~((ARM_TT_ROOT_SIZE + ARM_TT_ROOT_INDEX_MASK) / 2ULL)) + 1ULL)
#endif /* __ARM_KERNEL_PROTECT__ */

#endif /* #ifndef _ARM_PMAP_H_ */
