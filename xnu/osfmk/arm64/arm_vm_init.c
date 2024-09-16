/*
 * Copyright (c) 2007-2011 Apple Inc. All rights reserved.
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

#include <mach_kdp.h>
#include <debug.h>

#include <kern/assert.h>
#include <kern/misc_protos.h>
#include <kern/monotonic.h>
#include <mach/vm_types.h>
#include <mach/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page_internal.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <arm64/proc_reg.h>
#include <arm64/lowglobals.h>
#include <arm/cpu_data_internal.h>
#include <arm/misc_protos.h>
#include <pexpert/arm64/boot.h>
#include <pexpert/device_tree.h>

#include <libkern/kernel_mach_header.h>
#include <libkern/section_keywords.h>

#include <san/kasan.h>

#if __ARM_KERNEL_PROTECT__
/*
 * If we want to support __ARM_KERNEL_PROTECT__, we need a sufficient amount of
 * mappable space preceeding the kernel (as we unmap the kernel by cutting the
 * range covered by TTBR1 in half).  This must also cover the exception vectors.
 */
static_assert(KERNEL_PMAP_HEAP_RANGE_START > ARM_KERNEL_PROTECT_EXCEPTION_START);

/* The exception vectors and the kernel cannot share root TTEs. */
static_assert((KERNEL_PMAP_HEAP_RANGE_START & ~ARM_TT_ROOT_OFFMASK) > ARM_KERNEL_PROTECT_EXCEPTION_START);

/*
 * We must have enough space in the TTBR1_EL1 range to create the EL0 mapping of
 * the exception vectors.
 */
static_assert((((~ARM_KERNEL_PROTECT_EXCEPTION_START) + 1) * 2ULL) <= (ARM_TT_ROOT_SIZE + ARM_TT_ROOT_INDEX_MASK));
#endif /* __ARM_KERNEL_PROTECT__ */

#define ARM_DYNAMIC_TABLE_XN (ARM_TTE_TABLE_PXN | ARM_TTE_TABLE_XN)

#if KASAN
extern vm_offset_t shadow_pbase;
extern vm_offset_t shadow_ptop;
extern vm_offset_t physmap_vbase;
extern vm_offset_t physmap_vtop;
#endif

/*
 * We explicitly place this in const, as it is not const from a language
 * perspective, but it is only modified before we actually switch away from
 * the bootstrap page tables.
 */
SECURITY_READ_ONLY_LATE(uint8_t) bootstrap_pagetables[BOOTSTRAP_TABLE_SIZE] __attribute__((aligned(ARM_PGBYTES)));

/*
 * Denotes the end of xnu.
 */
extern void *last_kernel_symbol;

extern void arm64_replace_bootstack(cpu_data_t*);
extern void PE_slide_devicetree(vm_offset_t);

/*
 * KASLR parameters
 */
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_base;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_top;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kext_base;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kext_top;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_stext;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_etext;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_slide;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_slid_base;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_slid_top;

SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_stext;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_etext;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_sdata;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_edata;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_sinfo;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_prelink_einfo;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_slinkedit;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_elinkedit;

SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_builtinkmod_text;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernel_builtinkmod_text_end;

SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernelcache_base;
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_kernelcache_top;

/* Used by <mach/arm/vm_param.h> */
SECURITY_READ_ONLY_LATE(unsigned long) gVirtBase;
SECURITY_READ_ONLY_LATE(unsigned long) gPhysBase;
SECURITY_READ_ONLY_LATE(unsigned long) gPhysSize;
SECURITY_READ_ONLY_LATE(unsigned long) gT0Sz = T0SZ_BOOT;
SECURITY_READ_ONLY_LATE(unsigned long) gT1Sz = T1SZ_BOOT;

/* 23543331 - step 1 of kext / kernel __TEXT and __DATA colocation is to move
 * all kexts before the kernel.  This is only for arm64 devices and looks
 * something like the following:
 * -- vmaddr order --
 * 0xffffff8004004000 __PRELINK_TEXT
 * 0xffffff8007004000 __TEXT (xnu)
 * 0xffffff80075ec000 __DATA (xnu)
 * 0xffffff80076dc000 __KLD (xnu)
 * 0xffffff80076e0000 __LAST (xnu)
 * 0xffffff80076e4000 __LINKEDIT (xnu)
 * 0xffffff80076e4000 __PRELINK_DATA (not used yet)
 * 0xffffff800782c000 __PRELINK_INFO
 * 0xffffff80078e4000 -- End of kernelcache
 */

/* 24921709 - make XNU ready for KTRR
 *
 * Two possible kernel cache layouts, depending on which kcgen is being used.
 * VAs increasing downwards.
 * Old KCGEN:
 *
 * __PRELINK_TEXT
 * __TEXT
 * __DATA_CONST
 * __TEXT_EXEC
 * __KLD
 * __LAST
 * __DATA
 * __PRELINK_DATA (expected empty)
 * __LINKEDIT
 * __PRELINK_INFO
 *
 * New kcgen:
 *
 * __PRELINK_TEXT    <--- First KTRR (ReadOnly) segment
 * __PLK_DATA_CONST
 * __PLK_TEXT_EXEC
 * __TEXT
 * __DATA_CONST
 * __TEXT_EXEC
 * __KLD
 * __LAST            <--- Last KTRR (ReadOnly) segment
 * __DATA
 * __BOOTDATA (if present)
 * __LINKEDIT
 * __PRELINK_DATA (expected populated now)
 * __PLK_LINKEDIT
 * __PRELINK_INFO
 *
 */

vm_offset_t mem_size;                             /* Size of actual physical memory present
                                                   * minus any performance buffer and possibly
                                                   * limited by mem_limit in bytes */
uint64_t    mem_actual;                           /* The "One True" physical memory size
                                                   * actually, it's the highest physical
                                                   * address + 1 */
uint64_t    max_mem;                              /* Size of physical memory (bytes), adjusted
                                                   * by maxmem */
uint64_t    max_mem_actual;                       /* Actual size of physical memory (bytes),
                                                   * adjusted by the maxmem boot-arg */
uint64_t    sane_size;                            /* Memory size to use for defaults
                                                   * calculations */
/* This no longer appears to be used; kill it? */
addr64_t    vm_last_addr = VM_MAX_KERNEL_ADDRESS; /* Highest kernel
                                                   * virtual address known
                                                   * to the VM system */

SECURITY_READ_ONLY_LATE(vm_offset_t)              segEXTRADATA;
SECURITY_READ_ONLY_LATE(unsigned long)            segSizeEXTRADATA;

/* Trust cache portion of EXTRADATA (if within it) */
SECURITY_READ_ONLY_LATE(vm_offset_t)              segTRUSTCACHE;
SECURITY_READ_ONLY_LATE(unsigned long)            segSizeTRUSTCACHE;

SECURITY_READ_ONLY_LATE(vm_offset_t)          segLOWESTTEXT;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLOWEST;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLOWESTRO;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segHIGHESTRO;

/* Only set when booted from MH_FILESET kernel collections */
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLOWESTKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segHIGHESTKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLOWESTROKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segHIGHESTROKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLOWESTAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segHIGHESTAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLOWESTROAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segHIGHESTROAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLOWESTRXAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segHIGHESTRXAuxKC;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segHIGHESTNLEAuxKC;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segTEXTB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeTEXT;

#if XNU_MONITOR
SECURITY_READ_ONLY_LATE(vm_offset_t)          segPPLTEXTB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizePPLTEXT;

SECURITY_READ_ONLY_LATE(vm_offset_t)          segPPLTRAMPB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizePPLTRAMP;

SECURITY_READ_ONLY_LATE(vm_offset_t)          segPPLDATACONSTB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizePPLDATACONST;
SECURITY_READ_ONLY_LATE(void *)               pmap_stacks_start = NULL;
SECURITY_READ_ONLY_LATE(void *)               pmap_stacks_end = NULL;
#if HAS_GUARDED_IO_FILTER
SECURITY_READ_ONLY_LATE(void *)               iofilter_stacks_start = NULL;
SECURITY_READ_ONLY_LATE(void *)               iofilter_stacks_end = NULL;
#endif
#endif

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segDATACONSTB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeDATACONST;

SECURITY_READ_ONLY_LATE(vm_offset_t)   segTEXTEXECB;
SECURITY_READ_ONLY_LATE(unsigned long) segSizeTEXTEXEC;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segDATAB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeDATA;

#if XNU_MONITOR
SECURITY_READ_ONLY_LATE(vm_offset_t)          segPPLDATAB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizePPLDATA;
#endif

SECURITY_READ_ONLY_LATE(vm_offset_t)          segBOOTDATAB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizeBOOTDATA;
extern vm_offset_t                            intstack_low_guard;
extern vm_offset_t                            intstack_high_guard;
extern vm_offset_t                            excepstack_high_guard;

SECURITY_READ_ONLY_LATE(vm_offset_t)          segLINKB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeLINK;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segKLDB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizeKLD;
SECURITY_READ_ONLY_LATE(static vm_offset_t)   segKLDDATAB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeKLDDATA;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLASTB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizeLAST;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segLASTDATACONSTB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizeLASTDATACONST;

SECURITY_READ_ONLY_LATE(vm_offset_t)          sectHIBTEXTB;
SECURITY_READ_ONLY_LATE(unsigned long)        sectSizeHIBTEXT;
SECURITY_READ_ONLY_LATE(vm_offset_t)          segHIBDATAB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizeHIBDATA;
SECURITY_READ_ONLY_LATE(vm_offset_t)          sectHIBDATACONSTB;
SECURITY_READ_ONLY_LATE(unsigned long)        sectSizeHIBDATACONST;

SECURITY_READ_ONLY_LATE(vm_offset_t)          segPRELINKTEXTB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizePRELINKTEXT;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segPLKTEXTEXECB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePLKTEXTEXEC;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segPLKDATACONSTB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePLKDATACONST;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segPRELINKDATAB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePRELINKDATA;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segPLKLLVMCOVB = 0;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePLKLLVMCOV = 0;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segPLKLINKEDITB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePLKLINKEDIT;

SECURITY_READ_ONLY_LATE(static vm_offset_t)   segPRELINKINFOB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizePRELINKINFO;

/* Only set when booted from MH_FILESET primary kernel collection */
SECURITY_READ_ONLY_LATE(vm_offset_t)          segKCTEXTEXECB;
SECURITY_READ_ONLY_LATE(unsigned long)        segSizeKCTEXTEXEC;
SECURITY_READ_ONLY_LATE(static vm_offset_t)   segKCDATACONSTB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeKCDATACONST;
SECURITY_READ_ONLY_LATE(static vm_offset_t)   segKCDATAB;
SECURITY_READ_ONLY_LATE(static unsigned long) segSizeKCDATA;

SECURITY_READ_ONLY_LATE(static boolean_t) use_contiguous_hint = TRUE;

SECURITY_READ_ONLY_LATE(int) PAGE_SHIFT_CONST;

SECURITY_READ_ONLY_LATE(vm_offset_t) end_kern;
SECURITY_READ_ONLY_LATE(vm_offset_t) etext;
SECURITY_READ_ONLY_LATE(vm_offset_t) sdata;
SECURITY_READ_ONLY_LATE(vm_offset_t) edata;

SECURITY_READ_ONLY_LATE(static vm_offset_t) auxkc_mh, auxkc_base, auxkc_right_above;

vm_offset_t alloc_ptpage(boolean_t map_static);
SECURITY_READ_ONLY_LATE(vm_offset_t) ropage_next;
extern int dtrace_keep_kernel_symbols(void);

/*
 * Bootstrap the system enough to run with virtual memory.
 * Map the kernel's code and data, and allocate the system page table.
 * Page_size must already be set.
 *
 * Parameters:
 * first_avail: first available physical page -
 *              after kernel page tables
 * avail_start: PA of first physical page
 * avail_end:   PA of last physical page
 */
SECURITY_READ_ONLY_LATE(vm_offset_t)     first_avail;
SECURITY_READ_ONLY_LATE(vm_offset_t)     static_memory_end;
SECURITY_READ_ONLY_LATE(pmap_paddr_t)    avail_start;
SECURITY_READ_ONLY_LATE(pmap_paddr_t)    avail_end;
SECURITY_READ_ONLY_LATE(pmap_paddr_t)    real_avail_end;
SECURITY_READ_ONLY_LATE(unsigned long)   real_phys_size;
SECURITY_READ_ONLY_LATE(vm_map_address_t) physmap_base = (vm_map_address_t)0;
SECURITY_READ_ONLY_LATE(vm_map_address_t) physmap_end = (vm_map_address_t)0;

/**
 * First physical address freely available to xnu.
 */
SECURITY_READ_ONLY_LATE(addr64_t) first_avail_phys = 0;

/*
 * Bounds of the kernelcache; used for accounting.
 */
SECURITY_READ_ONLY_LATE(vm_offset_t) arm_vm_kernelcache_phys_start;
SECURITY_READ_ONLY_LATE(vm_offset_t) arm_vm_kernelcache_phys_end;

#if __ARM_KERNEL_PROTECT__
extern void ExceptionVectorsBase;
extern void ExceptionVectorsEnd;
#endif /* __ARM_KERNEL_PROTECT__ */

typedef struct {
	pmap_paddr_t pa;
	vm_map_address_t va;
	vm_size_t len;
} ptov_table_entry;

#define PTOV_TABLE_SIZE 8

SECURITY_READ_ONLY_LATE(static ptov_table_entry)        ptov_table[PTOV_TABLE_SIZE];
SECURITY_READ_ONLY_LATE(static boolean_t)               kva_active = FALSE;

#define ARM64_PAGE_UNGUARDED (0)
#define ARM64_PAGE_GUARDED   (1)

/* "physical to kernel virtual" - given a physical address, return the corresponding physical aperture address */
vm_map_address_t
phystokv(pmap_paddr_t pa)
{

	for (size_t i = 0; (i < PTOV_TABLE_SIZE) && (ptov_table[i].len != 0); i++) {
		if ((pa >= ptov_table[i].pa) && (pa < (ptov_table[i].pa + ptov_table[i].len))) {
			return pa - ptov_table[i].pa + ptov_table[i].va;
		}
	}
	if (__improbable((pa < gPhysBase) || ((pa - gPhysBase) >= real_phys_size))) {
		panic("%s: illegal PA: 0x%llx; phys base 0x%llx, size 0x%llx", __func__,
		    (unsigned long long)pa, (unsigned long long)gPhysBase, (unsigned long long)real_phys_size);
	}
	return pa - gPhysBase + gVirtBase;
}

vm_map_address_t
phystokv_range(pmap_paddr_t pa, vm_size_t *max_len)
{

	vm_size_t len;
	for (size_t i = 0; (i < PTOV_TABLE_SIZE) && (ptov_table[i].len != 0); i++) {
		if ((pa >= ptov_table[i].pa) && (pa < (ptov_table[i].pa + ptov_table[i].len))) {
			len = ptov_table[i].len - (pa - ptov_table[i].pa);
			if (*max_len > len) {
				*max_len = len;
			}
			return pa - ptov_table[i].pa + ptov_table[i].va;
		}
	}
	len = PAGE_SIZE - (pa & PAGE_MASK);
	if (*max_len > len) {
		*max_len = len;
	}
	if (__improbable((pa < gPhysBase) || ((pa - gPhysBase) >= real_phys_size))) {
		panic("%s: illegal PA: 0x%llx; phys base 0x%llx, size 0x%llx", __func__,
		    (unsigned long long)pa, (unsigned long long)gPhysBase, (unsigned long long)real_phys_size);
	}
	return pa - gPhysBase + gVirtBase;
}

vm_offset_t
ml_static_vtop(vm_offset_t va)
{
	for (size_t i = 0; (i < PTOV_TABLE_SIZE) && (ptov_table[i].len != 0); i++) {
		if ((va >= ptov_table[i].va) && (va < (ptov_table[i].va + ptov_table[i].len))) {
			return va - ptov_table[i].va + ptov_table[i].pa;
		}
	}
	if (__improbable((va < gVirtBase) || (((vm_address_t)(va) - gVirtBase) >= gPhysSize))) {
		panic("%s: illegal VA: %p; virt base 0x%llx, size 0x%llx", __func__,
		    (void*)va, (unsigned long long)gVirtBase, (unsigned long long)gPhysSize);
	}
	return (vm_address_t)(va) - gVirtBase + gPhysBase;
}

/*
 * This rounds the given address up to the nearest boundary for a PTE contiguous
 * hint.
 */
static vm_offset_t
round_up_pte_hint_address(vm_offset_t address)
{
	vm_offset_t hint_size = ARM_PTE_SIZE << ARM_PTE_HINT_ENTRIES_SHIFT;
	return (address + (hint_size - 1)) & ~(hint_size - 1);
}

/* allocate a page for a page table: we support static and dynamic mappings.
 *
 * returns a virtual address for the allocated page
 *
 * for static mappings, we allocate from the region ropagetable_begin to ro_pagetable_end-1,
 * which is defined in the DATA_CONST segment and will be protected RNX when vm_prot_finalize runs.
 *
 * for dynamic mappings, we allocate from avail_start, which should remain RWNX.
 */

vm_offset_t
alloc_ptpage(boolean_t map_static)
{
	vm_offset_t vaddr;

#if !(defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR))
	map_static = FALSE;
#endif

	if (!ropage_next) {
		ropage_next = (vm_offset_t)&ropagetable_begin;
	}

	if (map_static) {
		assert(ropage_next < (vm_offset_t)&ropagetable_end);

		vaddr = ropage_next;
		ropage_next += ARM_PGBYTES;

		return vaddr;
	} else {
		vaddr = phystokv(avail_start);
		avail_start += ARM_PGBYTES;

		return vaddr;
	}
}

#if DEBUG

void dump_kva_l2(vm_offset_t tt_base, tt_entry_t *tt, int indent, uint64_t *rosz_out, uint64_t *rwsz_out);

void
dump_kva_l2(vm_offset_t tt_base, tt_entry_t *tt, int indent, uint64_t *rosz_out, uint64_t *rwsz_out)
{
	unsigned int i;
	boolean_t cur_ro, prev_ro = 0;
	int start_entry = -1;
	tt_entry_t cur, prev = 0;
	pmap_paddr_t robegin = kvtophys((vm_offset_t)&ropagetable_begin);
	pmap_paddr_t roend = kvtophys((vm_offset_t)&ropagetable_end);
	boolean_t tt_static = kvtophys((vm_offset_t)tt) >= robegin &&
	    kvtophys((vm_offset_t)tt) < roend;

	for (i = 0; i < TTE_PGENTRIES; i++) {
		int tte_type = tt[i] & ARM_TTE_TYPE_MASK;
		cur = tt[i] & ARM_TTE_TABLE_MASK;

		if (tt_static) {
			/* addresses mapped by this entry are static if it is a block mapping,
			 * or the table was allocated from the RO page table region */
			cur_ro = (tte_type == ARM_TTE_TYPE_BLOCK) || (cur >= robegin && cur < roend);
		} else {
			cur_ro = 0;
		}

		if ((cur == 0 && prev != 0) || (cur_ro != prev_ro && prev != 0)) { // falling edge
			uintptr_t start, end, sz;

			start = (uintptr_t)start_entry << ARM_TT_L2_SHIFT;
			start += tt_base;
			end = ((uintptr_t)i << ARM_TT_L2_SHIFT) - 1;
			end += tt_base;

			sz = end - start + 1;
			printf("%*s0x%08x_%08x-0x%08x_%08x %s (%luMB)\n",
			    indent * 4, "",
			    (uint32_t)(start >> 32), (uint32_t)start,
			    (uint32_t)(end >> 32), (uint32_t)end,
			    prev_ro ? "Static " : "Dynamic",
			    (sz >> 20));

			if (prev_ro) {
				*rosz_out += sz;
			} else {
				*rwsz_out += sz;
			}
		}

		if ((prev == 0 && cur != 0) || cur_ro != prev_ro) { // rising edge: set start
			start_entry = i;
		}

		prev = cur;
		prev_ro = cur_ro;
	}
}

void
dump_kva_space()
{
	uint64_t tot_rosz = 0, tot_rwsz = 0;
	int ro_ptpages, rw_ptpages;
	pmap_paddr_t robegin = kvtophys((vm_offset_t)&ropagetable_begin);
	pmap_paddr_t roend = kvtophys((vm_offset_t)&ropagetable_end);
	boolean_t root_static = kvtophys((vm_offset_t)cpu_tte) >= robegin &&
	    kvtophys((vm_offset_t)cpu_tte) < roend;
	uint64_t kva_base = ~((1ULL << (64 - T1SZ_BOOT)) - 1);

	printf("Root page table: %s\n", root_static ? "Static" : "Dynamic");

	for (unsigned int i = 0; i < TTE_PGENTRIES; i++) {
		pmap_paddr_t cur;
		boolean_t cur_ro;
		uintptr_t start, end;
		uint64_t rosz = 0, rwsz = 0;

		if ((cpu_tte[i] & ARM_TTE_VALID) == 0) {
			continue;
		}

		cur = cpu_tte[i] & ARM_TTE_TABLE_MASK;
		start = (uint64_t)i << ARM_TT_L1_SHIFT;
		start = start + kva_base;
		end = start + (ARM_TT_L1_SIZE - 1);
		cur_ro = cur >= robegin && cur < roend;

		printf("0x%08x_%08x-0x%08x_%08x %s\n",
		    (uint32_t)(start >> 32), (uint32_t)start,
		    (uint32_t)(end >> 32), (uint32_t)end,
		    cur_ro ? "Static " : "Dynamic");

		dump_kva_l2(start, (tt_entry_t*)phystokv(cur), 1, &rosz, &rwsz);
		tot_rosz += rosz;
		tot_rwsz += rwsz;
	}

	printf("L2 Address space mapped: Static %lluMB Dynamic %lluMB Total %lluMB\n",
	    tot_rosz >> 20,
	    tot_rwsz >> 20,
	    (tot_rosz >> 20) + (tot_rwsz >> 20));

	ro_ptpages = (int)((ropage_next - (vm_offset_t)&ropagetable_begin) >> ARM_PGSHIFT);
	rw_ptpages = (int)(lowGlo.lgStaticSize  >> ARM_PGSHIFT);
	printf("Pages used: static %d dynamic %d\n", ro_ptpages, rw_ptpages);
}

#endif /* DEBUG */

#if __ARM_KERNEL_PROTECT__ || XNU_MONITOR
/*
 * arm_vm_map:
 *   root_ttp: The kernel virtual address for the root of the target page tables
 *   vaddr: The target virtual address
 *   pte: A page table entry value (may be ARM_PTE_EMPTY)
 *
 * This function installs pte at vaddr in root_ttp.  Any page table pages needed
 * to install pte will be allocated by this function.
 */
static void
arm_vm_map(tt_entry_t * root_ttp, vm_offset_t vaddr, pt_entry_t pte)
{
	vm_offset_t ptpage = 0;
	tt_entry_t * ttp = root_ttp;

	tt_entry_t * l1_ttep = NULL;
	tt_entry_t l1_tte = 0;

	tt_entry_t * l2_ttep = NULL;
	tt_entry_t l2_tte = 0;
	pt_entry_t * ptep = NULL;
	pt_entry_t cpte = 0;

	/*
	 * Walk the target page table to find the PTE for the given virtual
	 * address.  Allocate any page table pages needed to do this.
	 */
	l1_ttep = ttp + ((vaddr & ARM_TT_L1_INDEX_MASK) >> ARM_TT_L1_SHIFT);
	l1_tte = *l1_ttep;

	if (l1_tte == ARM_TTE_EMPTY) {
		ptpage = alloc_ptpage(TRUE);
		bzero((void *)ptpage, ARM_PGBYTES);
		l1_tte = kvtophys(ptpage);
		l1_tte &= ARM_TTE_TABLE_MASK;
		l1_tte |= ARM_TTE_VALID | ARM_TTE_TYPE_TABLE | ARM_TTE_TABLE_AP(ARM_TTE_TABLE_AP_USER_NA);
		*l1_ttep = l1_tte;
		ptpage = 0;
	}

	ttp = (tt_entry_t *)phystokv(l1_tte & ARM_TTE_TABLE_MASK);

	l2_ttep = ttp + ((vaddr & ARM_TT_L2_INDEX_MASK) >> ARM_TT_L2_SHIFT);
	l2_tte = *l2_ttep;

	if (l2_tte == ARM_TTE_EMPTY) {
		ptpage = alloc_ptpage(TRUE);
		bzero((void *)ptpage, ARM_PGBYTES);
		l2_tte = kvtophys(ptpage);
		l2_tte &= ARM_TTE_TABLE_MASK;
		l2_tte |= ARM_TTE_VALID | ARM_TTE_TYPE_TABLE;
		*l2_ttep = l2_tte;
		ptpage = 0;
	}

	ttp = (tt_entry_t *)phystokv(l2_tte & ARM_TTE_TABLE_MASK);

	ptep = ttp + ((vaddr & ARM_TT_L3_INDEX_MASK) >> ARM_TT_L3_SHIFT);
	cpte = *ptep;

	/*
	 * If the existing PTE is not empty, then we are replacing a valid
	 * mapping.
	 */
	if (cpte != ARM_PTE_EMPTY) {
		panic("%s: cpte=%#llx is not empty, "
		    "vaddr=%#lx, pte=%#llx",
		    __FUNCTION__, cpte,
		    vaddr, pte);
	}

	*ptep = pte;
}

#endif // __ARM_KERNEL_PROTECT || XNU_MONITOR

#if __ARM_KERNEL_PROTECT__

/*
 * arm_vm_kernel_el0_map:
 *   vaddr: The target virtual address
 *   pte: A page table entry value (may be ARM_PTE_EMPTY)
 *
 * This function installs pte at vaddr for the EL0 kernel mappings.
 */
static void
arm_vm_kernel_el0_map(vm_offset_t vaddr, pt_entry_t pte)
{
	/* Calculate where vaddr will be in the EL1 kernel page tables. */
	vm_offset_t kernel_pmap_vaddr = vaddr - ((ARM_TT_ROOT_INDEX_MASK + ARM_TT_ROOT_SIZE) / 2ULL);
	arm_vm_map(cpu_tte, kernel_pmap_vaddr, pte);
}

/*
 * arm_vm_kernel_el1_map:
 *   vaddr: The target virtual address
 *   pte: A page table entry value (may be ARM_PTE_EMPTY)
 *
 * This function installs pte at vaddr for the EL1 kernel mappings.
 */
static void
arm_vm_kernel_el1_map(vm_offset_t vaddr, pt_entry_t pte)
{
	arm_vm_map(cpu_tte, vaddr, pte);
}

/*
 * arm_vm_kernel_pte:
 *   vaddr: The target virtual address
 *
 * This function returns the PTE value for the given vaddr from the kernel page
 * tables.  If the region has been been block mapped, we return what an
 * equivalent PTE value would be (as regards permissions and flags).  We also
 * remove the HINT bit (as we are not necessarily creating contiguous mappings.
 */
static pt_entry_t
arm_vm_kernel_pte(vm_offset_t vaddr)
{
	tt_entry_t * ttp = cpu_tte;
	tt_entry_t * ttep = NULL;
	tt_entry_t tte = 0;
	pt_entry_t * ptep = NULL;
	pt_entry_t pte = 0;

	ttep = ttp + ((vaddr & ARM_TT_L1_INDEX_MASK) >> ARM_TT_L1_SHIFT);
	tte = *ttep;

	assert(tte & ARM_TTE_VALID);

	if ((tte & ARM_TTE_TYPE_MASK) == ARM_TTE_TYPE_BLOCK) {
		/* This is a block mapping; return the equivalent PTE value. */
		pte = (pt_entry_t)(tte & ~ARM_TTE_TYPE_MASK);
		pte |= ARM_PTE_TYPE_VALID;
		pte |= vaddr & ((ARM_TT_L1_SIZE - 1) & ARM_PTE_PAGE_MASK);
		pte &= ~ARM_PTE_HINT_MASK;
		return pte;
	}

	ttp = (tt_entry_t *)phystokv(tte & ARM_TTE_TABLE_MASK);
	ttep = ttp + ((vaddr & ARM_TT_L2_INDEX_MASK) >> ARM_TT_L2_SHIFT);
	tte = *ttep;

	assert(tte & ARM_TTE_VALID);

	if ((tte & ARM_TTE_TYPE_MASK) == ARM_TTE_TYPE_BLOCK) {
		/* This is a block mapping; return the equivalent PTE value. */
		pte = (pt_entry_t)(tte & ~ARM_TTE_TYPE_MASK);
		pte |= ARM_PTE_TYPE_VALID;
		pte |= vaddr & ((ARM_TT_L2_SIZE - 1) & ARM_PTE_PAGE_MASK);
		pte &= ~ARM_PTE_HINT_MASK;
		return pte;
	}

	ttp = (tt_entry_t *)phystokv(tte & ARM_TTE_TABLE_MASK);

	ptep = ttp + ((vaddr & ARM_TT_L3_INDEX_MASK) >> ARM_TT_L3_SHIFT);
	pte = *ptep;
	pte &= ~ARM_PTE_HINT_MASK;
	return pte;
}

/*
 * arm_vm_prepare_kernel_el0_mappings:
 *   alloc_only: Indicates if PTE values should be copied from the EL1 kernel
 *     mappings.
 *
 * This function expands the kernel page tables to support the EL0 kernel
 * mappings, and conditionally installs the PTE values for the EL0 kernel
 * mappings (if alloc_only is false).
 */
static void
arm_vm_prepare_kernel_el0_mappings(bool alloc_only)
{
	pt_entry_t pte = 0;
	vm_offset_t start = ((vm_offset_t)&ExceptionVectorsBase) & ~PAGE_MASK;
	vm_offset_t end = (((vm_offset_t)&ExceptionVectorsEnd) + PAGE_MASK) & ~PAGE_MASK;
	vm_offset_t cur = 0;
	vm_offset_t cur_fixed = 0;

	/* Expand for/map the exceptions vectors in the EL0 kernel mappings. */
	for (cur = start, cur_fixed = ARM_KERNEL_PROTECT_EXCEPTION_START; cur < end; cur += ARM_PGBYTES, cur_fixed += ARM_PGBYTES) {
		/*
		 * We map the exception vectors at a different address than that
		 * of the kernelcache to avoid sharing page table pages with the
		 * kernelcache (as this may cause issues with TLB caching of
		 * page table pages.
		 */
		if (!alloc_only) {
			pte = arm_vm_kernel_pte(cur);
		}

		arm_vm_kernel_el1_map(cur_fixed, pte);
		arm_vm_kernel_el0_map(cur_fixed, pte);
	}

	__builtin_arm_dmb(DMB_ISH);
	__builtin_arm_isb(ISB_SY);

	if (!alloc_only) {
		/*
		 * If we have created the alternate exception vector mappings,
		 * the boot CPU may now switch over to them.
		 */
		set_vbar_el1(ARM_KERNEL_PROTECT_EXCEPTION_START);
		__builtin_arm_isb(ISB_SY);
	}
}

/*
 * arm_vm_populate_kernel_el0_mappings:
 *
 * This function adds all required mappings to the EL0 kernel mappings.
 */
static void
arm_vm_populate_kernel_el0_mappings(void)
{
	arm_vm_prepare_kernel_el0_mappings(FALSE);
}

/*
 * arm_vm_expand_kernel_el0_mappings:
 *
 * This function expands the kernel page tables to accomodate the EL0 kernel
 * mappings.
 */
static void
arm_vm_expand_kernel_el0_mappings(void)
{
	arm_vm_prepare_kernel_el0_mappings(TRUE);
}
#endif /* __ARM_KERNEL_PROTECT__ */

#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
extern void bootstrap_instructions;

/*
 * arm_replace_identity_map takes the V=P map that we construct in start.s
 * and repurposes it in order to have it map only the page we need in order
 * to turn on the MMU.  This prevents us from running into issues where
 * KTRR will cause us to fault on executable block mappings that cross the
 * KTRR boundary.
 */
static void
arm_replace_identity_map(void)
{
	vm_offset_t addr;
	pmap_paddr_t paddr;

	pmap_paddr_t l1_ptp_phys = 0;
	tt_entry_t *l1_ptp_virt = NULL;
	tt_entry_t *tte1 = NULL;
	pmap_paddr_t l2_ptp_phys = 0;
	tt_entry_t *l2_ptp_virt = NULL;
	tt_entry_t *tte2 = NULL;
	pmap_paddr_t l3_ptp_phys = 0;
	pt_entry_t *l3_ptp_virt = NULL;
	pt_entry_t *ptep = NULL;

	addr = ((vm_offset_t)&bootstrap_instructions) & ~ARM_PGMASK;
	paddr = kvtophys(addr);

	/*
	 * Grab references to the V=P page tables, and allocate an L3 page.
	 */
	l1_ptp_phys = kvtophys((vm_offset_t)&bootstrap_pagetables);
	l1_ptp_virt = (tt_entry_t *)phystokv(l1_ptp_phys);
	tte1 = &l1_ptp_virt[L1_TABLE_INDEX(paddr)];

	l2_ptp_virt = L2_TABLE_VA(tte1);
	l2_ptp_phys = (*tte1) & ARM_TTE_TABLE_MASK;
	tte2 = &l2_ptp_virt[L2_TABLE_INDEX(paddr)];

	l3_ptp_virt = (pt_entry_t *)alloc_ptpage(TRUE);
	l3_ptp_phys = kvtophys((vm_offset_t)l3_ptp_virt);
	ptep = &l3_ptp_virt[L3_TABLE_INDEX(paddr)];

	/*
	 * Replace the large V=P mapping with a mapping that provides only the
	 * mappings needed to turn on the MMU.
	 */

	bzero(l1_ptp_virt, ARM_PGBYTES);
	*tte1 = ARM_TTE_BOOT_TABLE | (l2_ptp_phys & ARM_TTE_TABLE_MASK);

	bzero(l2_ptp_virt, ARM_PGBYTES);
	*tte2 = ARM_TTE_BOOT_TABLE | (l3_ptp_phys & ARM_TTE_TABLE_MASK);

	*ptep = (paddr & ARM_PTE_MASK) |
	    ARM_PTE_TYPE_VALID |
	    ARM_PTE_SH(SH_OUTER_MEMORY) |
	    ARM_PTE_ATTRINDX(CACHE_ATTRINDX_WRITEBACK) |
	    ARM_PTE_AF |
	    ARM_PTE_AP(AP_RONA) |
	    ARM_PTE_NX;
}
#endif /* defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR) */

tt_entry_t *arm_kva_to_tte(vm_offset_t);

tt_entry_t *
arm_kva_to_tte(vm_offset_t va)
{
	tt_entry_t *tte1, *tte2;
	tte1 = cpu_tte + L1_TABLE_INDEX(va);
	tte2 = L2_TABLE_VA(tte1) + L2_TABLE_INDEX(va);

	return tte2;
}

#if XNU_MONITOR

static inline pt_entry_t *
arm_kva_to_pte(vm_offset_t va)
{
	tt_entry_t *tte2 = arm_kva_to_tte(va);
	return L3_TABLE_VA(tte2) + L3_TABLE_INDEX(va);
}

#endif

#define ARM64_GRANULE_ALLOW_BLOCK (1 << 0)
#define ARM64_GRANULE_ALLOW_HINT (1 << 1)

/**
 * Updates a translation table entry (TTE) with the supplied value, unless doing so might render
 * the pagetable region read-only before subsequent updates have finished.  In that case, the TTE
 * value will be saved off for deferred processing.
 *
 * @param ttep address of the TTE to update
 * @param entry the value to store in ttep
 * @param pa the base physical address mapped by the TTE
 * @param ttebase L3-page- or L2-block-aligned base virtual address of the pagetable region
 * @param granule mask indicating whether L2 block or L3 hint mappings are allowed for this segment
 * @param deferred_ttep_pair 2-element array of addresses of deferred TTEs
 * @param deferred_tte_pair 2-element array containing TTE values for deferred assignment to
 *        corresponding elements of deferred_ttep_pair
 */
static void
update_or_defer_tte(tt_entry_t *ttep, tt_entry_t entry, pmap_paddr_t pa, vm_map_address_t ttebase,
    unsigned granule __unused, tt_entry_t **deferred_ttep_pair, tt_entry_t *deferred_tte_pair)
{
	/*
	 * If we're trying to assign an entry that maps the current TTE region (identified by ttebase),
	 * and the pagetable is already live (indicated by kva_active), defer assignment of the current
	 * entry and possibly the entry after it until all other mappings in the segment have been
	 * updated.  Otherwise we may end up immediately marking the pagetable region read-only
	 * leading to a fault later on a later assignment if we manage to outrun the TLB.  This can
	 * happen on KTRR/CTRR-enabled devices when marking segDATACONST read-only, as the pagetables
	 * that map that segment must come from the segment itself.  We therefore store the initial
	 * recursive TTE in deferred_ttep_pair[0] and its value in deferred_tte_pair[0].  We may also
	 * defer assignment of the TTE following that recursive TTE and store its value in
	 * deferred_tte_pair[1], because the TTE region following the current one may also contain
	 * pagetables and we must avoid marking that region read-only before updating those tables.
	 *
	 * We require that such recursive mappings must exist in regions that can be mapped with L2
	 * block entries if they are sufficiently large.  This is what allows us to assume that no
	 * more than 2 deferred TTEs will be required, because:
	 * 	--If more than 2 adjacent L3 PTEs were required to map our pagetables, that would mean
	 * 	  we would have at least one full L3 pagetable page and would instead use an L2 block.
	 *	--If more than 2 adjacent L2 blocks were required to map our pagetables, that would
	 * 	  mean we would have at least one full L2-block-sized region of TTEs and something
	 *	  is very wrong because no segment should be that large.
	 */
	if ((deferred_ttep_pair != NULL) && (deferred_ttep_pair[0] != NULL) && (ttep == (deferred_ttep_pair[0] + 1))) {
		assert(deferred_tte_pair[1] == 0);
		deferred_ttep_pair[1] = ttep;
		deferred_tte_pair[1] = entry;
	} else if (kva_active && (phystokv(pa) == ttebase)) {
		assert(deferred_ttep_pair != NULL);
		assert(granule & ARM64_GRANULE_ALLOW_BLOCK);
		if (deferred_ttep_pair[0] == NULL) {
			deferred_ttep_pair[0] = ttep;
			deferred_tte_pair[0] = entry;
		} else {
			assert(deferred_ttep_pair[1] == NULL);
			deferred_ttep_pair[1] = ttep;
			deferred_tte_pair[1] = entry;
		}
	} else {
		*ttep = entry;
	}
}

/*
 * arm_vm_page_granular_helper updates protections at the L3 level.  It will (if
 * neccessary) allocate a page for the L3 table and update the corresponding L2
 * entry.  Then, it will iterate over the L3 table, updating protections as necessary.
 * This expects to be invoked on a L2 entry or sub L2 entry granularity, so this should
 * not be invoked from a context that does not do L2 iteration separately (basically,
 * don't call this except from arm_vm_page_granular_prot).
 *
 * unsigned granule: 0 => force to page granule, or a combination of
 * ARM64_GRANULE_* flags declared above.
 * 
 * unsigned int guarded => flag indicating whether this range should be
 * considered an ARM "guarded" page. This enables BTI enforcement for a region.
 */

static void
arm_vm_page_granular_helper(vm_offset_t start, vm_offset_t _end, vm_offset_t va, pmap_paddr_t pa_offset,
    int pte_prot_APX, int pte_prot_XN, unsigned granule, __unused unsigned int guarded,
    tt_entry_t **deferred_ttep_pair, tt_entry_t *deferred_tte_pair)
{
	if (va & ARM_TT_L2_OFFMASK) { /* ragged edge hanging over a ARM_TT_L2_SIZE  boundary */
		tt_entry_t *tte2;
		tt_entry_t tmplate;
		pmap_paddr_t pa;
		pt_entry_t *ppte, ptmp;
		addr64_t ppte_phys;
		unsigned i;

		va &= ~ARM_TT_L2_OFFMASK;
		pa = va - gVirtBase + gPhysBase - pa_offset;

		if (pa >= real_avail_end) {
			return;
		}

		tte2 = arm_kva_to_tte(va);

		assert(_end >= va);
		tmplate = *tte2;

		if (ARM_TTE_TYPE_TABLE == (tmplate & ARM_TTE_TYPE_MASK)) {
			/* pick up the existing page table. */
			ppte = (pt_entry_t *)phystokv((tmplate & ARM_TTE_TABLE_MASK));
		} else {
			// TTE must be reincarnated with page level mappings.

			// ... but we don't want to break up blocks on live
			// translation tables.
			assert(!kva_active);

			ppte = (pt_entry_t*)alloc_ptpage(pa_offset == 0);
			bzero(ppte, ARM_PGBYTES);
			ppte_phys = kvtophys((vm_offset_t)ppte);

			*tte2 = pa_to_tte(ppte_phys) | ARM_TTE_TYPE_TABLE | ARM_TTE_VALID;
		}

		vm_offset_t len = _end - va;
		if ((pa + len) > real_avail_end) {
			_end -= (pa + len - real_avail_end);
		}
		assert((start - gVirtBase + gPhysBase - pa_offset) >= gPhysBase);

		/* Round up to the nearest PAGE_SIZE boundary when creating mappings:
		 * PAGE_SIZE may be a multiple of ARM_PGBYTES, and we don't want to leave
		 * a ragged non-PAGE_SIZE-aligned edge. */
		vm_offset_t rounded_end = round_page(_end);
		/* Apply the desired protections to the specified page range */
		for (i = 0; i <= (ARM_TT_L3_INDEX_MASK >> ARM_TT_L3_SHIFT); i++) {
			if ((start <= va) && (va < rounded_end)) {
				ptmp = pa | ARM_PTE_AF | ARM_PTE_SH(SH_OUTER_MEMORY) | ARM_PTE_TYPE;
				ptmp = ptmp | ARM_PTE_ATTRINDX(CACHE_ATTRINDX_DEFAULT);
				ptmp = ptmp | ARM_PTE_AP(pte_prot_APX);
				ptmp = ptmp | ARM_PTE_NX;
#if __ARM_KERNEL_PROTECT__
				ptmp = ptmp | ARM_PTE_NG;
#endif /* __ARM_KERNEL_PROTECT__ */

				if (pte_prot_XN) {
					ptmp = ptmp | ARM_PTE_PNX;
				}

				/*
				 * If we can, apply the contiguous hint to this range.  The hint is
				 * applicable if the current address falls within a hint-sized range that will
				 * be fully covered by this mapping request.
				 */
				if ((va >= round_up_pte_hint_address(start)) && (round_up_pte_hint_address(va + 1) <= _end) &&
				    (granule & ARM64_GRANULE_ALLOW_HINT) && use_contiguous_hint) {
					assert((va & ((1 << ARM_PTE_HINT_ADDR_SHIFT) - 1)) == ((pa & ((1 << ARM_PTE_HINT_ADDR_SHIFT) - 1))));
					ptmp |= ARM_PTE_HINT;
					/* Do not attempt to reapply the hint bit to an already-active mapping.
					 * This very likely means we're attempting to change attributes on an already-active mapping,
					 * which violates the requirement of the hint bit.*/
					assert(!kva_active || (ppte[i] == ARM_PTE_TYPE_FAULT));
				}

#if BTI_ENFORCED
				/*
				 * Set the 'guarded page' flag to enable ARM BTI enforcement.
				 */
				if (guarded) {
					ptmp |= ARM_PTE_GP;
				}
#endif /* BTI_ENFORCED */
				/*
				 * Do not change the contiguous bit on an active mapping.  Even in a single-threaded
				 * environment, it's possible for prefetch to produce a TLB conflict by trying to pull in
				 * a hint-sized entry on top of one or more existing page-sized entries.  It's also useful
				 * to make sure we're not trying to unhint a sub-range of a larger hinted range, which
				 * could produce a later TLB conflict.
				 */
				assert(!kva_active || (ppte[i] == ARM_PTE_TYPE_FAULT) || ((ppte[i] & ARM_PTE_HINT) == (ptmp & ARM_PTE_HINT)));

				update_or_defer_tte(&ppte[i], ptmp, pa, (vm_map_address_t)ppte, granule, deferred_ttep_pair, deferred_tte_pair);
			}

			va += ARM_PGBYTES;
			pa += ARM_PGBYTES;
		}
	}
}

/*
 * arm_vm_page_granular_prot updates protections by iterating over the L2 entries and
 * changing them.  If a particular chunk necessitates L3 entries (for reasons of
 * alignment or length, or an explicit request that the entry be fully expanded), we
 * hand off to arm_vm_page_granular_helper to deal with the L3 chunk of the logic.
 */
static void
arm_vm_page_granular_prot(vm_offset_t start, unsigned long size, pmap_paddr_t pa_offset,
    int tte_prot_XN, int pte_prot_APX, int pte_prot_XN,
    unsigned granule, unsigned int guarded)
{
	tt_entry_t *deferred_ttep_pair[2] = {NULL};
	tt_entry_t deferred_tte_pair[2] = {0};
	vm_offset_t _end = start + size;
	vm_offset_t align_start = (start + ARM_TT_L2_OFFMASK) & ~ARM_TT_L2_OFFMASK;

	if (size == 0x0UL) {
		return;
	}

	if (align_start > _end) {
		align_start = _end;
	}

	arm_vm_page_granular_helper(start, align_start, start, pa_offset, pte_prot_APX, pte_prot_XN, granule, guarded, deferred_ttep_pair, deferred_tte_pair);

	while ((_end - align_start) >= ARM_TT_L2_SIZE) {
		if (!(granule & ARM64_GRANULE_ALLOW_BLOCK)) {
			arm_vm_page_granular_helper(align_start, align_start + ARM_TT_L2_SIZE, align_start + 1, pa_offset,
			    pte_prot_APX, pte_prot_XN, granule, guarded, deferred_ttep_pair, deferred_tte_pair);
		} else {
			pmap_paddr_t pa = align_start - gVirtBase + gPhysBase - pa_offset;
			assert((pa & ARM_TT_L2_OFFMASK) == 0);
			tt_entry_t *tte2;
			tt_entry_t tmplate;

			tte2 = arm_kva_to_tte(align_start);

			if ((pa >= gPhysBase) && (pa < real_avail_end)) {
				tmplate = (pa & ARM_TTE_BLOCK_L2_MASK) | ARM_TTE_TYPE_BLOCK
				    | ARM_TTE_VALID | ARM_TTE_BLOCK_AF | ARM_TTE_BLOCK_NX
				    | ARM_TTE_BLOCK_AP(pte_prot_APX) | ARM_TTE_BLOCK_SH(SH_OUTER_MEMORY)
				    | ARM_TTE_BLOCK_ATTRINDX(CACHE_ATTRINDX_WRITEBACK);

#if __ARM_KERNEL_PROTECT__
				tmplate = tmplate | ARM_TTE_BLOCK_NG;
#endif /* __ARM_KERNEL_PROTECT__ */
				if (tte_prot_XN) {
					tmplate = tmplate | ARM_TTE_BLOCK_PNX;
				}

				update_or_defer_tte(tte2, tmplate, pa, (vm_map_address_t)tte2 & ~ARM_TT_L2_OFFMASK,
				    granule, deferred_ttep_pair, deferred_tte_pair);
			}
		}
		align_start += ARM_TT_L2_SIZE;
	}

	if (align_start < _end) {
		arm_vm_page_granular_helper(align_start, _end, _end, pa_offset, pte_prot_APX, pte_prot_XN, granule, guarded, deferred_ttep_pair, deferred_tte_pair);
	}

	if (deferred_ttep_pair[0] != NULL) {
#if DEBUG || DEVELOPMENT
		/*
		 * Flush the TLB to catch bugs that might cause us to prematurely revoke write access from the pagetable page.
		 * These bugs may otherwise be hidden by TLB entries in most cases, resulting in very rare panics.
		 * Note that we always flush the TLB at the end of arm_vm_prot_finalize().
		 */
		flush_mmu_tlb();
#endif
		/*
		 * The first TTE in the pair is a recursive mapping of the pagetable region, so we must update it last
		 * to avoid potentially marking deferred_pte_pair[1] read-only.
		 */
		if (deferred_tte_pair[1] != 0) {
			os_atomic_store(deferred_ttep_pair[1], deferred_tte_pair[1], release);
		}
		os_atomic_store(deferred_ttep_pair[0], deferred_tte_pair[0], release);
	}
}

static inline void
arm_vm_page_granular_RNX(vm_offset_t start, unsigned long size, unsigned granule)
{
	arm_vm_page_granular_prot(start, size, 0, 1, AP_RONA, 1, granule, ARM64_PAGE_UNGUARDED);
}

static inline void
arm_vm_page_granular_ROX(vm_offset_t start, unsigned long size, unsigned granule, unsigned int guarded)
{
	arm_vm_page_granular_prot(start, size, 0, 0, AP_RONA, 0, granule, guarded);
}

static inline void
arm_vm_page_granular_RWNX(vm_offset_t start, unsigned long size, unsigned granule)
{
	arm_vm_page_granular_prot(start, size, 0, 1, AP_RWNA, 1, granule, ARM64_PAGE_UNGUARDED);
}

// Populate seg...AuxKC and fixup AuxKC permissions
static bool
arm_vm_auxkc_init(void)
{
	if (auxkc_mh == 0 || auxkc_base == 0) {
		return false; // no auxKC.
	}

	/* Fixup AuxKC and populate seg*AuxKC globals used below */
	arm_auxkc_init((void*)auxkc_mh, (void*)auxkc_base);

	if (segLOWESTAuxKC != segLOWEST) {
		panic("segLOWESTAuxKC (%p) not equal to segLOWEST (%p). auxkc_mh: %p, auxkc_base: %p",
		    (void*)segLOWESTAuxKC, (void*)segLOWEST,
		    (void*)auxkc_mh, (void*)auxkc_base);
	}

	/*
	 * The AuxKC LINKEDIT segment needs to be covered by the RO region but is excluded
	 * from the RO address range returned by kernel_collection_adjust_mh_addrs().
	 * Ensure the highest non-LINKEDIT address in the AuxKC is the current end of
	 * its RO region before extending it.
	 */
	assert(segHIGHESTROAuxKC == segHIGHESTNLEAuxKC);
	assert(segHIGHESTAuxKC >= segHIGHESTROAuxKC);
	if (segHIGHESTAuxKC > segHIGHESTROAuxKC) {
		segHIGHESTROAuxKC = segHIGHESTAuxKC;
	}

	/*
	 * The AuxKC RO region must be right below the device tree/trustcache so that it can be covered
	 * by CTRR, and the AuxKC RX region must be within the RO region.
	 */
	assert(segHIGHESTROAuxKC == auxkc_right_above);
	assert(segHIGHESTRXAuxKC <= segHIGHESTROAuxKC);
	assert(segLOWESTRXAuxKC <= segHIGHESTRXAuxKC);
	assert(segLOWESTROAuxKC <= segLOWESTRXAuxKC);
	assert(segLOWESTAuxKC <= segLOWESTROAuxKC);

	if (segHIGHESTRXAuxKC < segLOWEST) {
		arm_vm_page_granular_RNX(segHIGHESTRXAuxKC, segLOWEST - segHIGHESTRXAuxKC, 0);
	}
	if (segLOWESTRXAuxKC < segHIGHESTRXAuxKC) {
		/* 
		 * We cannot mark auxKC text as guarded because doing so would enforce
		 * BTI on oblivious third-party kexts and break ABI compatibility. 
		 * Doing this defeats the purpose of BTI (branches to these pages are 
		 * unchecked!) but given both the relative rarity and the diversity of
		 * third-party kexts, we expect that this is likely impractical to
		 * exploit in practice.
		 */
		arm_vm_page_granular_ROX(segLOWESTRXAuxKC, segHIGHESTRXAuxKC - segLOWESTRXAuxKC, 0, ARM64_PAGE_UNGUARDED); // Refined in OSKext::readPrelinkedExtensions
	}
	if (segLOWESTROAuxKC < segLOWESTRXAuxKC) {
		arm_vm_page_granular_RNX(segLOWESTROAuxKC, segLOWESTRXAuxKC - segLOWESTROAuxKC, 0);
	}
	if (segLOWESTAuxKC < segLOWESTROAuxKC) {
		arm_vm_page_granular_RWNX(segLOWESTAuxKC, segLOWESTROAuxKC - segLOWESTAuxKC, 0);
	}

	return true;
}

void
arm_vm_prot_init(__unused boot_args * args)
{
	segLOWESTTEXT = UINT64_MAX;
	if (segSizePRELINKTEXT && (segPRELINKTEXTB < segLOWESTTEXT)) {
		segLOWESTTEXT = segPRELINKTEXTB;
	}
	assert(segSizeTEXT);
	if (segTEXTB < segLOWESTTEXT) {
		segLOWESTTEXT = segTEXTB;
	}
	assert(segLOWESTTEXT < UINT64_MAX);

	segEXTRADATA = 0;
	segSizeEXTRADATA = 0;
	segTRUSTCACHE = 0;
	segSizeTRUSTCACHE = 0;

	segLOWEST = segLOWESTTEXT;
	segLOWESTRO = segLOWESTTEXT;

	if (segLOWESTKC && segLOWESTKC < segLOWEST) {
		/*
		 * kernel collections have segments below the kernel. In particular the collection mach header
		 * is below PRELINK_TEXT and is not covered by any other segments already tracked.
		 */
		arm_vm_page_granular_RNX(segLOWESTKC, segLOWEST - segLOWESTKC, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
		segLOWEST = segLOWESTKC;
		if (segLOWESTROKC && segLOWESTROKC < segLOWESTRO) {
			segLOWESTRO = segLOWESTROKC;
		}
		if (segHIGHESTROKC && segHIGHESTROKC > segHIGHESTRO) {
			segHIGHESTRO = segHIGHESTROKC;
		}
	}

	DTEntry memory_map;
	int err;

	// Device Tree portion of EXTRADATA
	if (SecureDTIsLockedDown()) {
		segEXTRADATA = (vm_offset_t)PE_state.deviceTreeHead;
		segSizeEXTRADATA = PE_state.deviceTreeSize;
	}

	// Trust Caches portion of EXTRADATA
	{
		DTMemoryMapRange const *trustCacheRange;
		unsigned int trustCacheRangeSize;

		err = SecureDTLookupEntry(NULL, "chosen/memory-map", &memory_map);
		assert(err == kSuccess);

		err = SecureDTGetProperty(memory_map, "TrustCache", (void const **)&trustCacheRange, &trustCacheRangeSize);
		if (err == kSuccess) {
			if (trustCacheRangeSize != sizeof(DTMemoryMapRange)) {
				panic("Unexpected /chosen/memory-map/TrustCache property size %u != %zu", trustCacheRangeSize, sizeof(DTMemoryMapRange));
			}

			vm_offset_t const trustCacheRegion = phystokv(trustCacheRange->paddr);
			if (trustCacheRegion < segLOWEST) {
				if (segEXTRADATA != 0) {
					if (trustCacheRegion != segEXTRADATA + segSizeEXTRADATA) {
						panic("Unexpected location of TrustCache region: %#lx != %#lx",
						    trustCacheRegion, segEXTRADATA + segSizeEXTRADATA);
					}
					segSizeEXTRADATA += trustCacheRange->length;
				} else {
					// Not all devices support CTRR device trees.
					segEXTRADATA = trustCacheRegion;
					segSizeEXTRADATA = trustCacheRange->length;
				}
			}
#if !(DEVELOPMENT || DEBUG)
			else {
				panic("TrustCache region is in an unexpected place: %#lx > %#lx", trustCacheRegion, segLOWEST);
			}
#endif
			segTRUSTCACHE = trustCacheRegion;
			segSizeTRUSTCACHE = trustCacheRange->length;
		}
	}

	if (segSizeEXTRADATA != 0) {
		if (segEXTRADATA <= segLOWEST) {
			segLOWEST = segEXTRADATA;
			if (segEXTRADATA <= segLOWESTRO) {
				segLOWESTRO = segEXTRADATA;
			}
		} else {
			panic("EXTRADATA is in an unexpected place: %#lx > %#lx", segEXTRADATA, segLOWEST);
		}

		arm_vm_page_granular_RNX(segEXTRADATA, segSizeEXTRADATA, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
	}

	const DTMemoryMapRange *auxKC_range, *auxKC_header_range;
	unsigned int auxKC_range_size, auxKC_header_range_size;

	err = SecureDTGetProperty(memory_map, "AuxKC", (const void**)&auxKC_range,
	    &auxKC_range_size);
	if (err != kSuccess) {
		goto noAuxKC;
	}
	assert(auxKC_range_size == sizeof(DTMemoryMapRange));
	err = SecureDTGetProperty(memory_map, "AuxKC-mach_header",
	    (const void**)&auxKC_header_range, &auxKC_header_range_size);
	if (err != kSuccess) {
		goto noAuxKC;
	}
	assert(auxKC_header_range_size == sizeof(DTMemoryMapRange));

	if (auxKC_header_range->paddr == 0 || auxKC_range->paddr == 0) {
		goto noAuxKC;
	}

	auxkc_mh = phystokv(auxKC_header_range->paddr);
	auxkc_base = phystokv(auxKC_range->paddr);

	if (auxkc_base < segLOWEST) {
		auxkc_right_above = segLOWEST;
		segLOWEST = auxkc_base;
	} else {
		panic("auxkc_base (%p) not below segLOWEST (%p)", (void*)auxkc_base, (void*)segLOWEST);
	}

	/* Map AuxKC RWNX initially so that arm_vm_auxkc_init can traverse
	 * it and apply fixups (after we're off the bootstrap translation
	 * tables).
	 */
	arm_vm_page_granular_RWNX(auxkc_base, auxKC_range->length, 0);

noAuxKC:
	/* Map coalesced kext TEXT segment RWNX for now */
	arm_vm_page_granular_RWNX(segPRELINKTEXTB, segSizePRELINKTEXT, ARM64_GRANULE_ALLOW_BLOCK); // Refined in OSKext::readPrelinkedExtensions

	/* Map coalesced kext DATA_CONST segment RWNX (could be empty) */
	arm_vm_page_granular_RWNX(segPLKDATACONSTB, segSizePLKDATACONST, ARM64_GRANULE_ALLOW_BLOCK); // Refined in OSKext::readPrelinkedExtensions

	/* Map coalesced kext TEXT_EXEC segment RX (could be empty) */
	arm_vm_page_granular_ROX(segPLKTEXTEXECB, segSizePLKTEXTEXEC, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT, ARM64_PAGE_GUARDED); // Refined in OSKext::readPrelinkedExtensions

	/* if new segments not present, set space between PRELINK_TEXT and xnu TEXT to RWNX
	 * otherwise we no longer expect any space between the coalesced kext read only segments and xnu rosegments
	 */
	if (!segSizePLKDATACONST && !segSizePLKTEXTEXEC) {
		if (segSizePRELINKTEXT) {
			arm_vm_page_granular_RWNX(segPRELINKTEXTB + segSizePRELINKTEXT, segTEXTB - (segPRELINKTEXTB + segSizePRELINKTEXT),
			    ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
		}
	} else {
		/*
		 * If we have the new segments, we should still protect the gap between kext
		 * read-only pages and kernel read-only pages, in the event that this gap
		 * exists.
		 */
		if ((segPLKDATACONSTB + segSizePLKDATACONST) < segTEXTB) {
			arm_vm_page_granular_RWNX(segPLKDATACONSTB + segSizePLKDATACONST, segTEXTB - (segPLKDATACONSTB + segSizePLKDATACONST),
			    ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
		}
	}

	/*
	 * Protection on kernel text is loose here to allow shenanigans early on.  These
	 * protections are tightened in arm_vm_prot_finalize().  This is necessary because
	 * we currently patch LowResetVectorBase in cpu.c.
	 *
	 * TEXT segment contains mach headers and other non-executable data. This will become RONX later.
	 */
	arm_vm_page_granular_RNX(segTEXTB, segSizeTEXT, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);

	/* Can DATACONST start out and stay RNX?
	 * NO, stuff in this segment gets modified during startup (viz. mac_policy_init()/mac_policy_list)
	 * Make RNX in prot_finalize
	 */
	arm_vm_page_granular_RWNX(segDATACONSTB, segSizeDATACONST, ARM64_GRANULE_ALLOW_BLOCK);

	arm_vm_page_granular_ROX(segTEXTEXECB, segSizeTEXTEXEC, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT, ARM64_PAGE_GUARDED);

#if XNU_MONITOR
	arm_vm_page_granular_ROX(segPPLTEXTB, segSizePPLTEXT, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT, ARM64_PAGE_UNGUARDED);
	arm_vm_page_granular_ROX(segPPLTRAMPB, segSizePPLTRAMP, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT, ARM64_PAGE_UNGUARDED);
	arm_vm_page_granular_RNX(segPPLDATACONSTB, segSizePPLDATACONST, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
#endif

	/* DATA segment will remain RWNX */
	arm_vm_page_granular_RWNX(segDATAB, segSizeDATA, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
#if XNU_MONITOR
	arm_vm_page_granular_RWNX(segPPLDATAB, segSizePPLDATA, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
#endif

	arm_vm_page_granular_RWNX(segHIBDATAB, segSizeHIBDATA, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);

	arm_vm_page_granular_RWNX(segBOOTDATAB, segSizeBOOTDATA, 0);
	arm_vm_page_granular_RNX((vm_offset_t)&intstack_low_guard, PAGE_MAX_SIZE, 0);
	arm_vm_page_granular_RNX((vm_offset_t)&intstack_high_guard, PAGE_MAX_SIZE, 0);
	arm_vm_page_granular_RNX((vm_offset_t)&excepstack_high_guard, PAGE_MAX_SIZE, 0);

	arm_vm_page_granular_ROX(segKLDB, segSizeKLD, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT, ARM64_PAGE_GUARDED);
	arm_vm_page_granular_RNX(segKLDDATAB, segSizeKLDDATA, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
	arm_vm_page_granular_RWNX(segLINKB, segSizeLINK, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);
	arm_vm_page_granular_RWNX(segPLKLINKEDITB, segSizePLKLINKEDIT, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT); // Coalesced kext LINKEDIT segment
	arm_vm_page_granular_ROX(segLASTB, segSizeLAST, ARM64_GRANULE_ALLOW_BLOCK, ARM64_PAGE_GUARDED); // __LAST may be empty, but we cannot assume this
	if (segLASTDATACONSTB) {
		arm_vm_page_granular_RWNX(segLASTDATACONSTB, segSizeLASTDATACONST, ARM64_GRANULE_ALLOW_BLOCK); // __LASTDATA_CONST may be empty, but we cannot assume this
	}
	arm_vm_page_granular_RWNX(segPRELINKDATAB, segSizePRELINKDATA, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT); // Prelink __DATA for kexts (RW data)

	if (segSizePLKLLVMCOV > 0) {
		arm_vm_page_granular_RWNX(segPLKLLVMCOVB, segSizePLKLLVMCOV, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT); // LLVM code coverage data
	}
	arm_vm_page_granular_RWNX(segPRELINKINFOB, segSizePRELINKINFO, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT); /* PreLinkInfoDictionary */

	/* Record the bounds of the kernelcache. */
	vm_kernelcache_base = segLOWEST;
	vm_kernelcache_top = end_kern;
}

/*
 * return < 0 for a < b
 *          0 for a == b
 *        > 0 for a > b
 */
typedef int (*cmpfunc_t)(const void *a, const void *b);

extern void
qsort(void *a, size_t n, size_t es, cmpfunc_t cmp);

static int
cmp_ptov_entries(const void *a, const void *b)
{
	const ptov_table_entry *entry_a = a;
	const ptov_table_entry *entry_b = b;
	// Sort in descending order of segment length
	if (entry_a->len < entry_b->len) {
		return 1;
	} else if (entry_a->len > entry_b->len) {
		return -1;
	} else {
		return 0;
	}
}

SECURITY_READ_ONLY_LATE(static unsigned int) ptov_index = 0;

#define ROUND_L1(addr) (((addr) + ARM_TT_L1_OFFMASK) & ~(ARM_TT_L1_OFFMASK))
#define ROUND_TWIG(addr) (((addr) + ARM_TT_TWIG_OFFMASK) & ~(ARM_TT_TWIG_OFFMASK))

static void
arm_vm_physmap_slide(ptov_table_entry *temp_ptov_table, vm_map_address_t orig_va, vm_size_t len, int pte_prot_APX, unsigned granule)
{
	pmap_paddr_t pa_offset;

	if (__improbable(ptov_index >= PTOV_TABLE_SIZE)) {
		panic("%s: PTOV table limit exceeded; segment va = 0x%llx, size = 0x%llx", __func__,
		    (unsigned long long)orig_va, (unsigned long long)len);
	}
	assert((orig_va & ARM_PGMASK) == 0);
	temp_ptov_table[ptov_index].pa = orig_va - gVirtBase + gPhysBase;
	if (ptov_index == 0) {
		temp_ptov_table[ptov_index].va = physmap_base;
	} else {
		temp_ptov_table[ptov_index].va = temp_ptov_table[ptov_index - 1].va + temp_ptov_table[ptov_index - 1].len;
	}
	if (granule & ARM64_GRANULE_ALLOW_BLOCK) {
		vm_map_address_t orig_offset = temp_ptov_table[ptov_index].pa & ARM_TT_TWIG_OFFMASK;
		vm_map_address_t new_offset = temp_ptov_table[ptov_index].va & ARM_TT_TWIG_OFFMASK;
		if (new_offset < orig_offset) {
			temp_ptov_table[ptov_index].va += (orig_offset - new_offset);
		} else if (new_offset > orig_offset) {
			temp_ptov_table[ptov_index].va = ROUND_TWIG(temp_ptov_table[ptov_index].va) + orig_offset;
		}
	}
	assert((temp_ptov_table[ptov_index].va & ARM_PGMASK) == 0);
	temp_ptov_table[ptov_index].len = round_page(len);
	pa_offset = temp_ptov_table[ptov_index].va - orig_va;
	arm_vm_page_granular_prot(temp_ptov_table[ptov_index].va, temp_ptov_table[ptov_index].len, pa_offset, 1, pte_prot_APX, 1, granule, ARM64_PAGE_UNGUARDED);
	++ptov_index;
}

#if XNU_MONITOR



SECURITY_READ_ONLY_LATE(static boolean_t) keep_linkedit = FALSE;

static void
arm_vm_physmap_init(boot_args *args)
{
	ptov_table_entry temp_ptov_table[PTOV_TABLE_SIZE];
	bzero(temp_ptov_table, sizeof(temp_ptov_table));

	// This is memory that will either be handed back to the VM layer via ml_static_mfree(),
	// or will be available for general-purpose use.   Physical aperture mappings for this memory
	// must be at page granularity, so that PPL ownership or cache attribute changes can be reflected
	// in the physical aperture mappings.

	// Slid region between gPhysBase and beginning of protected text
	arm_vm_physmap_slide(temp_ptov_table, gVirtBase, segLOWEST - gVirtBase, AP_RWNA, 0);

	// kext bootstrap segments
#if !defined(KERNEL_INTEGRITY_KTRR) && !defined(KERNEL_INTEGRITY_CTRR)
	/* __KLD,__text is covered by the rorgn */
	arm_vm_physmap_slide(temp_ptov_table, segKLDB, segSizeKLD, AP_RONA, 0);
#endif
	arm_vm_physmap_slide(temp_ptov_table, segKLDDATAB, segSizeKLDDATA, AP_RONA, 0);

	// Early-boot data
	arm_vm_physmap_slide(temp_ptov_table, segBOOTDATAB, segSizeBOOTDATA, AP_RONA, 0);

	PE_parse_boot_argn("keepsyms", &keep_linkedit, sizeof(keep_linkedit));
#if CONFIG_DTRACE
	if (dtrace_keep_kernel_symbols()) {
		keep_linkedit = TRUE;
	}
#endif /* CONFIG_DTRACE */
#if KASAN_DYNAMIC_BLACKLIST
	/* KASAN's dynamic blacklist needs to query the LINKEDIT segment at runtime.  As such, the
	 * kext bootstrap code will not jettison LINKEDIT on kasan kernels, so don't bother to relocate it. */
	keep_linkedit = TRUE;
#endif
	if (!keep_linkedit) {
		// Kernel LINKEDIT
		arm_vm_physmap_slide(temp_ptov_table, segLINKB, segSizeLINK, AP_RWNA, 0);

		if (segSizePLKLINKEDIT) {
			// Prelinked kernel LINKEDIT
			arm_vm_physmap_slide(temp_ptov_table, segPLKLINKEDITB, segSizePLKLINKEDIT, AP_RWNA, 0);
		}
	}

	// Prelinked kernel plists
	arm_vm_physmap_slide(temp_ptov_table, segPRELINKINFOB, segSizePRELINKINFO, AP_RWNA, 0);

	// Device tree (if not locked down), ramdisk, boot args
	arm_vm_physmap_slide(temp_ptov_table, end_kern, (args->topOfKernelData - gPhysBase + gVirtBase) - end_kern, AP_RWNA, 0);
	if (!SecureDTIsLockedDown()) {
		PE_slide_devicetree(temp_ptov_table[ptov_index - 1].va - end_kern);
	}

	// Remainder of physical memory
	arm_vm_physmap_slide(temp_ptov_table, (args->topOfKernelData - gPhysBase + gVirtBase),
	    real_avail_end - args->topOfKernelData, AP_RWNA, 0);



	assert((temp_ptov_table[ptov_index - 1].va + temp_ptov_table[ptov_index - 1].len) <= physmap_end);

	// Sort in descending order of segment length.  LUT traversal is linear, so largest (most likely used)
	// segments should be placed earliest in the table to optimize lookup performance.
	qsort(temp_ptov_table, PTOV_TABLE_SIZE, sizeof(temp_ptov_table[0]), cmp_ptov_entries);

	memcpy(ptov_table, temp_ptov_table, sizeof(ptov_table));
}

#else

static void
arm_vm_physmap_init(boot_args *args)
{
	ptov_table_entry temp_ptov_table[PTOV_TABLE_SIZE];
	bzero(temp_ptov_table, sizeof(temp_ptov_table));

	// Will be handed back to VM layer through ml_static_mfree() in arm_vm_prot_finalize()
	arm_vm_physmap_slide(temp_ptov_table, gVirtBase, segLOWEST - gVirtBase, AP_RWNA,
	    ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT);

	arm_vm_page_granular_RWNX(end_kern, phystokv(args->topOfKernelData) - end_kern,
	    ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT); /* Device Tree (if not locked down), RAM Disk (if present), bootArgs */

	arm_vm_physmap_slide(temp_ptov_table, (args->topOfKernelData - gPhysBase + gVirtBase),
	    real_avail_end - args->topOfKernelData, AP_RWNA, ARM64_GRANULE_ALLOW_BLOCK | ARM64_GRANULE_ALLOW_HINT); // rest of physmem

	assert((temp_ptov_table[ptov_index - 1].va + temp_ptov_table[ptov_index - 1].len) <= physmap_end);

	// Sort in descending order of segment length.  LUT traversal is linear, so largest (most likely used)
	// segments should be placed earliest in the table to optimize lookup performance.
	qsort(temp_ptov_table, PTOV_TABLE_SIZE, sizeof(temp_ptov_table[0]), cmp_ptov_entries);

	memcpy(ptov_table, temp_ptov_table, sizeof(ptov_table));
}

#endif // XNU_MONITOR

void
arm_vm_prot_finalize(boot_args * args __unused)
{
	/*
	 * At this point, we are far enough along in the boot process that it will be
	 * safe to free up all of the memory preceeding the kernel.  It may in fact
	 * be safe to do this earlier.
	 *
	 * This keeps the memory in the V-to-P mapping, but advertises it to the VM
	 * as usable.
	 */

	/*
	 * if old style PRELINK segment exists, free memory before it, and after it before XNU text
	 * otherwise we're dealing with a new style kernel cache, so we should just free the
	 * memory before PRELINK_TEXT segment, since the rest of the KEXT read only data segments
	 * should be immediately followed by XNU's TEXT segment
	 */

	ml_static_mfree(phystokv(gPhysBase), segLOWEST - gVirtBase);

	/*
	 * KTRR support means we will be mucking with these pages and trying to
	 * protect them; we cannot free the pages to the VM if we do this.
	 */
	if (!segSizePLKDATACONST && !segSizePLKTEXTEXEC && segSizePRELINKTEXT) {
		/* If new segments not present, PRELINK_TEXT is not dynamically sized, free DRAM between it and xnu TEXT */
		ml_static_mfree(segPRELINKTEXTB + segSizePRELINKTEXT, segTEXTB - (segPRELINKTEXTB + segSizePRELINKTEXT));
	}

	/* tighten permissions on kext read only data and code */
	arm_vm_page_granular_RNX(segPRELINKTEXTB, segSizePRELINKTEXT, ARM64_GRANULE_ALLOW_BLOCK);
	arm_vm_page_granular_RNX(segPLKDATACONSTB, segSizePLKDATACONST, ARM64_GRANULE_ALLOW_BLOCK);

	cpu_stack_alloc(&BootCpuData);
	arm64_replace_bootstack(&BootCpuData);
	ml_static_mfree(phystokv(segBOOTDATAB - gVirtBase + gPhysBase), segSizeBOOTDATA);

#if __ARM_KERNEL_PROTECT__
	arm_vm_populate_kernel_el0_mappings();
#endif /* __ARM_KERNEL_PROTECT__ */

#if XNU_MONITOR
#if !defined(KERNEL_INTEGRITY_KTRR) && !defined(KERNEL_INTEGRITY_CTRR)
	/* __KLD,__text is covered by the rorgn */
	for (vm_offset_t va = segKLDB; va < (segKLDB + segSizeKLD); va += ARM_PGBYTES) {
		pt_entry_t *pte = arm_kva_to_pte(va);
		*pte = ARM_PTE_EMPTY;
	}
#endif
	for (vm_offset_t va = segKLDDATAB; va < (segKLDDATAB + segSizeKLDDATA); va += ARM_PGBYTES) {
		pt_entry_t *pte = arm_kva_to_pte(va);
		*pte = ARM_PTE_EMPTY;
	}
	/* Clear the original stack mappings; these pages should be mapped through ptov_table. */
	for (vm_offset_t va = segBOOTDATAB; va < (segBOOTDATAB + segSizeBOOTDATA); va += ARM_PGBYTES) {
		pt_entry_t *pte = arm_kva_to_pte(va);
		*pte = ARM_PTE_EMPTY;
	}
	/* Clear the original PRELINKINFO mapping. This segment should be jettisoned during I/O Kit
	 * initialization before we reach this point. */
	for (vm_offset_t va = segPRELINKINFOB; va < (segPRELINKINFOB + segSizePRELINKINFO); va += ARM_PGBYTES) {
		pt_entry_t *pte = arm_kva_to_pte(va);
		*pte = ARM_PTE_EMPTY;
	}
	if (!keep_linkedit) {
		for (vm_offset_t va = segLINKB; va < (segLINKB + segSizeLINK); va += ARM_PGBYTES) {
			pt_entry_t *pte = arm_kva_to_pte(va);
			*pte = ARM_PTE_EMPTY;
		}
		if (segSizePLKLINKEDIT) {
			for (vm_offset_t va = segPLKLINKEDITB; va < (segPLKLINKEDITB + segSizePLKLINKEDIT); va += ARM_PGBYTES) {
				pt_entry_t *pte = arm_kva_to_pte(va);
				*pte = ARM_PTE_EMPTY;
			}
		}
	}
#endif /* XNU_MONITOR */

#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
	/*
	 * __LAST,__pinst should no longer be executable.
	 */
	arm_vm_page_granular_RNX(segLASTB, segSizeLAST, ARM64_GRANULE_ALLOW_BLOCK);

	/* __LASTDATA_CONST should no longer be writable. */
	if (segLASTDATACONSTB) {
		arm_vm_page_granular_RNX(segLASTDATACONSTB, segSizeLASTDATACONST, ARM64_GRANULE_ALLOW_BLOCK);
	}

	/*
	 * __KLD,__text should no longer be executable.
	 */
	arm_vm_page_granular_RNX(segKLDB, segSizeKLD, ARM64_GRANULE_ALLOW_BLOCK);

	/*
	 * Must wait until all other region permissions are set before locking down DATA_CONST
	 * as the kernel static page tables live in DATA_CONST on KTRR enabled systems
	 * and will become immutable.
	 */
#endif

	arm_vm_page_granular_RNX(segDATACONSTB, segSizeDATACONST, ARM64_GRANULE_ALLOW_BLOCK);

	__builtin_arm_dsb(DSB_ISH);
	flush_mmu_tlb();
}

/*
 * TBI (top-byte ignore) is an ARMv8 feature for ignoring the top 8 bits of
 * address accesses. It can be enabled separately for TTBR0 (user) and
 * TTBR1 (kernel).
 */
void
arm_set_kernel_tbi(void)
{
#if !__ARM_KERNEL_PROTECT__ && CONFIG_KERNEL_TBI
	uint64_t old_tcr, new_tcr;

	old_tcr = new_tcr = get_tcr();
	/*
	 * For kernel configurations that require TBI support on
	 * PAC systems, we enable DATA TBI only.
	 */
	new_tcr |= TCR_TBI1_TOPBYTE_IGNORED;
	new_tcr |= TCR_TBID1_ENABLE;

	if (old_tcr != new_tcr) {
		set_tcr(new_tcr);
		sysreg_restore.tcr_el1 = new_tcr;
	}
#endif /* !__ARM_KERNEL_PROTECT__ && CONFIG_KERNEL_TBI */
}

static void
arm_set_user_tbi(void)
{
#if !__ARM_KERNEL_PROTECT__
	uint64_t old_tcr, new_tcr;

	old_tcr = new_tcr = get_tcr();
	new_tcr |= TCR_TBI0_TOPBYTE_IGNORED;

	if (old_tcr != new_tcr) {
		set_tcr(new_tcr);
		sysreg_restore.tcr_el1 = new_tcr;
	}
#endif /* !__ARM_KERNEL_PROTECT__ */
}

/*
 * Initialize and enter blank (invalid) page tables in a L1 translation table for a given VA range.
 *
 * This is a helper function used to build up the initial page tables for the kernel translation table.
 * With KERNEL_INTEGRITY we keep at least the root level of the kernel page table immutable, thus the need
 * to preallocate before machine_lockdown any L1 entries necessary during the entire kernel runtime.
 *
 * For a given VA range, if necessary, allocate new L2 translation tables and install the table entries in
 * the appropriate L1 table indexes. called before the translation table is active
 *
 * parameters:
 *
 * tt: virtual address of L1 translation table to modify
 * start: beginning of VA range
 * end: end of VA range
 * static_map: whether to allocate the new translation table page from read only memory
 * table_attrs: attributes of new table entry in addition to VALID and TYPE_TABLE attributes
 *
 */

static void
init_ptpages(tt_entry_t *tt, vm_map_address_t start, vm_map_address_t end, bool static_map, uint64_t table_attrs)
{
	tt_entry_t *l1_tte;
	vm_offset_t ptpage_vaddr;

	l1_tte = tt + ((start & ARM_TT_L1_INDEX_MASK) >> ARM_TT_L1_SHIFT);

	while (start < end) {
		if (*l1_tte == ARM_TTE_EMPTY) {
			/* Allocate a page and setup L1 Table TTE in L1 */
			ptpage_vaddr = alloc_ptpage(static_map);
			*l1_tte = (kvtophys(ptpage_vaddr) & ARM_TTE_TABLE_MASK) | ARM_TTE_TYPE_TABLE | ARM_TTE_VALID | table_attrs;
			bzero((void *)ptpage_vaddr, ARM_PGBYTES);
		}

		if ((start + ARM_TT_L1_SIZE) < start) {
			/* If this is the last L1 entry, it must cover the last mapping. */
			break;
		}

		start += ARM_TT_L1_SIZE;
		l1_tte++;
	}
}

#define ARM64_PHYSMAP_SLIDE_RANGE (1ULL << 30) // 1 GB
#define ARM64_PHYSMAP_SLIDE_MASK  (ARM64_PHYSMAP_SLIDE_RANGE - 1)

void
arm_vm_init(uint64_t memory_size, boot_args * args)
{
	vm_map_address_t va_l1, va_l1_end;
	tt_entry_t       *cpu_l1_tte;
	vm_map_address_t va_l2, va_l2_end;
	tt_entry_t       *cpu_l2_tte;
	pmap_paddr_t     boot_ttep;
	tt_entry_t       *boot_tte;
	uint64_t         mem_segments;
	vm_offset_t      ptpage_vaddr;
	vm_map_address_t dynamic_memory_begin;

	/*
	 * Get the virtual and physical kernel-managed memory base from boot_args.
	 */
	gVirtBase = args->virtBase;
	gPhysBase = args->physBase;
#if KASAN
	real_phys_size = args->memSize + (shadow_ptop - shadow_pbase);
#else
	real_phys_size = args->memSize;
#endif
	/*
	 * Ensure the physical region we specify for the VM to manage ends on a
	 * software page boundary.  Note that the software page size (PAGE_SIZE)
	 * may be a multiple of the hardware page size specified in ARM_PGBYTES.
	 * We must round the reported memory size down to the nearest PAGE_SIZE
	 * boundary to ensure the VM does not try to manage a page it does not
	 * completely own.  The KASAN shadow region, if present, is managed entirely
	 * in units of the hardware page size and should not need similar treatment.
	 */
	gPhysSize = mem_size = ((gPhysBase + args->memSize) & ~PAGE_MASK) - gPhysBase;

	mem_actual = args->memSizeActual ? args->memSizeActual : mem_size;

	if ((memory_size != 0) && (mem_size > memory_size)) {
		mem_size = memory_size;
		max_mem_actual = memory_size;
	} else {
		max_mem_actual = mem_actual;
	}
#if !defined(ARM_LARGE_MEMORY)
	if (mem_size >= ((VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / 2)) {
		panic("Unsupported memory configuration %lx", mem_size);
	}
#endif

#if defined(ARM_LARGE_MEMORY)
	unsigned long physmap_l1_entries = ((real_phys_size + ARM64_PHYSMAP_SLIDE_RANGE) >> ARM_TT_L1_SHIFT) + 1;
	physmap_base = VM_MIN_KERNEL_ADDRESS - (physmap_l1_entries << ARM_TT_L1_SHIFT);
#else
	physmap_base = phystokv(args->topOfKernelData);
#endif

	// Slide the physical aperture to a random page-aligned location within the slide range
	uint64_t physmap_slide = early_random() & ARM64_PHYSMAP_SLIDE_MASK & ~((uint64_t)PAGE_MASK);
	assert(physmap_slide < ARM64_PHYSMAP_SLIDE_RANGE);

	physmap_base += physmap_slide;

#if XNU_MONITOR
	physmap_base = ROUND_TWIG(physmap_base);
#if defined(ARM_LARGE_MEMORY)
	static_memory_end = phystokv(args->topOfKernelData);
#else
	static_memory_end = physmap_base + mem_size;
#endif // ARM_LARGE_MEMORY
	physmap_end = physmap_base + real_phys_size;


#else
#if defined(ARM_LARGE_MEMORY)
	/* For large memory systems with no PPL such as virtual machines */
	static_memory_end = phystokv(args->topOfKernelData);
	physmap_end = physmap_base + real_phys_size;
#else
	static_memory_end = physmap_base + mem_size + (PTOV_TABLE_SIZE * ARM_TT_TWIG_SIZE); // worst possible case for block alignment
	physmap_end = physmap_base + real_phys_size + (PTOV_TABLE_SIZE * ARM_TT_TWIG_SIZE);
#endif // ARM_LARGE_MEMORY
#endif

#if KASAN && !defined(ARM_LARGE_MEMORY)
	/* add the KASAN stolen memory to the physmap */
	dynamic_memory_begin = static_memory_end + (shadow_ptop - shadow_pbase);
#else
	dynamic_memory_begin = static_memory_end;
#endif
#if XNU_MONITOR
	pmap_stacks_start = (void*)dynamic_memory_begin;
	dynamic_memory_begin += PPL_STACK_REGION_SIZE;
	pmap_stacks_end = (void*)dynamic_memory_begin;

#if HAS_GUARDED_IO_FILTER
    iofilter_stacks_start = (void*)dynamic_memory_begin;
    dynamic_memory_begin += IOFILTER_STACK_REGION_SIZE;
    iofilter_stacks_end = (void*)dynamic_memory_begin;
#endif
#endif
	if (dynamic_memory_begin > VM_MAX_KERNEL_ADDRESS) {
		panic("Unsupported memory configuration %lx", mem_size);
	}

	boot_tte = (tt_entry_t *)&bootstrap_pagetables;
	boot_ttep = kvtophys((vm_offset_t)boot_tte);

#if DEVELOPMENT || DEBUG
	/* Sanity check - assert that BOOTSTRAP_TABLE_SIZE is sufficiently-large to
	 * hold our bootstrap mappings for any possible slide */
	size_t bytes_mapped = dynamic_memory_begin - gVirtBase;
	size_t l1_entries = 1 + ((bytes_mapped + ARM_TT_L1_SIZE - 1) / ARM_TT_L1_SIZE);
	/* 1 L1 each for V=P and KVA, plus 1 page for each L2 */
	size_t pages_used = 2 * (l1_entries + 1);
	if (pages_used > BOOTSTRAP_TABLE_SIZE) {
		panic("BOOTSTRAP_TABLE_SIZE too small for memory config");
	}
#endif

	/*
	 *  TTBR0 L1, TTBR0 L2 - 1:1 bootstrap mapping.
	 *  TTBR1 L1, TTBR1 L2 - kernel mapping
	 */

	/*
	 * TODO: free bootstrap table memory back to allocator.
	 * on large memory systems bootstrap tables could be quite large.
	 * after bootstrap complete, xnu can warm start with a single 16KB page mapping
	 * to trampoline to KVA. this requires only 3 pages to stay resident.
	 */
	first_avail_phys = avail_start = args->topOfKernelData;

#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
	arm_replace_identity_map();
#endif

	/* Initialize invalid tte page */
	invalid_tte = (tt_entry_t *)alloc_ptpage(TRUE);
	invalid_ttep = kvtophys((vm_offset_t)invalid_tte);
	bzero(invalid_tte, ARM_PGBYTES);

	/*
	 * Initialize l1 page table page
	 */
	cpu_tte = (tt_entry_t *)alloc_ptpage(TRUE);
	cpu_ttep = kvtophys((vm_offset_t)cpu_tte);
	bzero(cpu_tte, ARM_PGBYTES);
	avail_end = gPhysBase + mem_size;
	assert(!(avail_end & PAGE_MASK));

#if KASAN
	real_avail_end = gPhysBase + real_phys_size;
#else
	real_avail_end = avail_end;
#endif

	/*
	 * Initialize l1 and l2 page table pages :
	 *   map physical memory at the kernel base virtual address
	 *   cover the kernel dynamic address range section
	 *
	 *   the so called physical aperture should be statically mapped
	 */
	init_ptpages(cpu_tte, gVirtBase, dynamic_memory_begin, TRUE, ARM_TTE_TABLE_AP(ARM_TTE_TABLE_AP_USER_NA));

#if defined(ARM_LARGE_MEMORY)
	/*
	 * Initialize l1 page table pages :
	 *   on large memory systems the physical aperture exists separately below
	 *   the rest of the kernel virtual address space
	 */
	init_ptpages(cpu_tte, physmap_base, ROUND_L1(physmap_end), TRUE, ARM_DYNAMIC_TABLE_XN | ARM_TTE_TABLE_AP(ARM_TTE_TABLE_AP_USER_NA));
#endif


#if __ARM_KERNEL_PROTECT__
	/* Expand the page tables to prepare for the EL0 mappings. */
	arm_vm_expand_kernel_el0_mappings();
#endif /* __ARM_KERNEL_PROTECT__ */

	/*
	 * Now retrieve addresses for various segments from kernel mach-o header
	 */
	segPRELINKTEXTB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PRELINK_TEXT", &segSizePRELINKTEXT);
	segPLKDATACONSTB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PLK_DATA_CONST", &segSizePLKDATACONST);
	segPLKTEXTEXECB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PLK_TEXT_EXEC", &segSizePLKTEXTEXEC);
	segTEXTB         = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__TEXT", &segSizeTEXT);
	segDATACONSTB    = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__DATA_CONST", &segSizeDATACONST);
	segTEXTEXECB     = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__TEXT_EXEC", &segSizeTEXTEXEC);
#if XNU_MONITOR
	segPPLTEXTB      = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PPLTEXT", &segSizePPLTEXT);
	segPPLTRAMPB     = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PPLTRAMP", &segSizePPLTRAMP);
	segPPLDATACONSTB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PPLDATA_CONST", &segSizePPLDATACONST);
#endif
	segDATAB         = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__DATA", &segSizeDATA);
#if XNU_MONITOR
	segPPLDATAB      = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PPLDATA", &segSizePPLDATA);
#endif

	segBOOTDATAB     = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__BOOTDATA", &segSizeBOOTDATA);
	segLINKB         = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__LINKEDIT", &segSizeLINK);
	segKLDB          = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__KLD", &segSizeKLD);
	segKLDDATAB      = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__KLDDATA", &segSizeKLDDATA);
	segPRELINKDATAB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PRELINK_DATA", &segSizePRELINKDATA);
	segPRELINKINFOB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PRELINK_INFO", &segSizePRELINKINFO);
	segPLKLLVMCOVB   = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PLK_LLVM_COV", &segSizePLKLLVMCOV);
	segPLKLINKEDITB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__PLK_LINKEDIT", &segSizePLKLINKEDIT);
	segLASTB         = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__LAST", &segSizeLAST);
	segLASTDATACONSTB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__LASTDATA_CONST", &segSizeLASTDATACONST);

	sectHIBTEXTB     = (vm_offset_t) getsectdatafromheader(&_mh_execute_header, "__TEXT_EXEC", "__hib_text", &sectSizeHIBTEXT);
	sectHIBDATACONSTB = (vm_offset_t) getsectdatafromheader(&_mh_execute_header, "__DATA_CONST", "__hib_const", &sectSizeHIBDATACONST);
	segHIBDATAB      = (vm_offset_t) getsegdatafromheader(&_mh_execute_header, "__HIBDATA", &segSizeHIBDATA);

	if (kernel_mach_header_is_in_fileset(&_mh_execute_header)) {
		kernel_mach_header_t *kc_mh = PE_get_kc_header(KCKindPrimary);

		// fileset has kext PLK_TEXT_EXEC under kernel collection TEXT_EXEC following kernel's LAST
		segKCTEXTEXECB = (vm_offset_t) getsegdatafromheader(kc_mh,             "__TEXT_EXEC", &segSizeKCTEXTEXEC);
		assert(segPLKTEXTEXECB && !segSizePLKTEXTEXEC);                        // kernel PLK_TEXT_EXEC must be empty

		assert(segLASTB);                                                      // kernel LAST can be empty, but it must have
		                                                                       // a valid address for computations below.

		assert(segKCTEXTEXECB <= segLASTB);                                    // KC TEXT_EXEC must contain kernel LAST
		assert(segKCTEXTEXECB + segSizeKCTEXTEXEC >= segLASTB + segSizeLAST);
		segPLKTEXTEXECB = segLASTB + segSizeLAST;
		segSizePLKTEXTEXEC = segSizeKCTEXTEXEC - (segPLKTEXTEXECB - segKCTEXTEXECB);

		// fileset has kext PLK_DATA_CONST under kernel collection DATA_CONST following kernel's LASTDATA_CONST
		segKCDATACONSTB = (vm_offset_t) getsegdatafromheader(kc_mh,            "__DATA_CONST", &segSizeKCDATACONST);
		assert(segPLKDATACONSTB && !segSizePLKDATACONST);                      // kernel PLK_DATA_CONST must be empty
		assert(segLASTDATACONSTB && segSizeLASTDATACONST);                     // kernel LASTDATA_CONST must be non-empty
		assert(segKCDATACONSTB <= segLASTDATACONSTB);                          // KC DATA_CONST must contain kernel LASTDATA_CONST
		assert(segKCDATACONSTB + segSizeKCDATACONST >= segLASTDATACONSTB + segSizeLASTDATACONST);
		segPLKDATACONSTB = segLASTDATACONSTB + segSizeLASTDATACONST;
		segSizePLKDATACONST = segSizeKCDATACONST - (segPLKDATACONSTB - segKCDATACONSTB);

		// fileset has kext PRELINK_DATA under kernel collection DATA following kernel's empty PRELINK_DATA
		segKCDATAB      = (vm_offset_t) getsegdatafromheader(kc_mh,            "__DATA", &segSizeKCDATA);
		assert(segPRELINKDATAB && !segSizePRELINKDATA);                        // kernel PRELINK_DATA must be empty
		assert(segKCDATAB <= segPRELINKDATAB);                                 // KC DATA must contain kernel PRELINK_DATA
		assert(segKCDATAB + segSizeKCDATA >= segPRELINKDATAB + segSizePRELINKDATA);
		segSizePRELINKDATA = segSizeKCDATA - (segPRELINKDATAB - segKCDATAB);

		// fileset has consolidated PRELINK_TEXT, PRELINK_INFO and LINKEDIT at the kernel collection level
		assert(segPRELINKTEXTB && !segSizePRELINKTEXT);                        // kernel PRELINK_TEXT must be empty
		segPRELINKTEXTB = (vm_offset_t) getsegdatafromheader(kc_mh,            "__PRELINK_TEXT", &segSizePRELINKTEXT);
		assert(segPRELINKINFOB && !segSizePRELINKINFO);                        // kernel PRELINK_INFO must be empty
		segPRELINKINFOB = (vm_offset_t) getsegdatafromheader(kc_mh,            "__PRELINK_INFO", &segSizePRELINKINFO);
		segLINKB        = (vm_offset_t) getsegdatafromheader(kc_mh,            "__LINKEDIT", &segSizeLINK);
	}

	(void) PE_parse_boot_argn("use_contiguous_hint", &use_contiguous_hint, sizeof(use_contiguous_hint));
	assert(segSizePRELINKTEXT < 0x03000000); /* 23355738 */

	/* if one of the new segments is present, the other one better be as well */
	if (segSizePLKDATACONST || segSizePLKTEXTEXEC) {
		assert(segSizePLKDATACONST && segSizePLKTEXTEXEC);
	}

	etext = (vm_offset_t) segTEXTB + segSizeTEXT;
	sdata = (vm_offset_t) segDATAB;
	edata = (vm_offset_t) segDATAB + segSizeDATA;
	end_kern = round_page(segHIGHESTKC ? segHIGHESTKC : getlastkerneladdr()); /* Force end to next page */

	vm_set_page_size();

	vm_kernel_base = segTEXTB;
	vm_kernel_top = (vm_offset_t) &last_kernel_symbol;
	vm_kext_base = segPRELINKTEXTB;
	vm_kext_top = vm_kext_base + segSizePRELINKTEXT;

	vm_prelink_stext = segPRELINKTEXTB;
	if (!segSizePLKTEXTEXEC && !segSizePLKDATACONST) {
		vm_prelink_etext = segPRELINKTEXTB + segSizePRELINKTEXT;
	} else {
		vm_prelink_etext = segPRELINKTEXTB + segSizePRELINKTEXT + segSizePLKDATACONST + segSizePLKTEXTEXEC;
	}
	vm_prelink_sinfo = segPRELINKINFOB;
	vm_prelink_einfo = segPRELINKINFOB + segSizePRELINKINFO;
	vm_slinkedit = segLINKB;
	vm_elinkedit = segLINKB + segSizeLINK;

	vm_prelink_sdata = segPRELINKDATAB;
	vm_prelink_edata = segPRELINKDATAB + segSizePRELINKDATA;

	arm_vm_prot_init(args);

	/*
	 * Initialize the page tables for the low globals:
	 *   cover this address range:
	 *     LOW_GLOBAL_BASE_ADDRESS + 2MB
	 */
	va_l1 = va_l2 = LOW_GLOBAL_BASE_ADDRESS;
	cpu_l1_tte = cpu_tte + ((va_l1 & ARM_TT_L1_INDEX_MASK) >> ARM_TT_L1_SHIFT);
	cpu_l2_tte = ((tt_entry_t *) phystokv(((*cpu_l1_tte) & ARM_TTE_TABLE_MASK))) + ((va_l2 & ARM_TT_L2_INDEX_MASK) >> ARM_TT_L2_SHIFT);
	ptpage_vaddr = alloc_ptpage(TRUE);
	*cpu_l2_tte = (kvtophys(ptpage_vaddr) & ARM_TTE_TABLE_MASK) | ARM_TTE_TYPE_TABLE | ARM_TTE_VALID | ARM_TTE_TABLE_PXN | ARM_TTE_TABLE_XN;
	bzero((void *)ptpage_vaddr, ARM_PGBYTES);

	/*
	 * Initialize l2 page table pages :
	 *   cover this address range:
	 *    KERNEL_DYNAMIC_ADDR - VM_MAX_KERNEL_ADDRESS
	 */
#if defined(ARM_LARGE_MEMORY)
	/*
	 * dynamic mapped memory outside the VM allocator VA range required to bootstrap VM system
	 * don't expect to exceed 64GB, no sense mapping any more space between here and the VM heap range
	 */
	init_ptpages(cpu_tte, dynamic_memory_begin, ROUND_L1(dynamic_memory_begin), FALSE, ARM_DYNAMIC_TABLE_XN | ARM_TTE_TABLE_AP(ARM_TTE_TABLE_AP_USER_NA));
#else
	/*
	 * TODO: do these pages really need to come from RO memory?
	 * With legacy 3 level table systems we never mapped more than a single L1 entry so this may be dead code
	 */
	init_ptpages(cpu_tte, dynamic_memory_begin, VM_MAX_KERNEL_ADDRESS, TRUE, ARM_DYNAMIC_TABLE_XN | ARM_TTE_TABLE_AP(ARM_TTE_TABLE_AP_USER_NA));
#endif

#if KASAN
	/* record the extent of the physmap */
	physmap_vbase = physmap_base;
	physmap_vtop = physmap_end;
	kasan_init();
#endif /* KASAN */

#if CONFIG_CPU_COUNTERS
	mt_early_init();
#endif /* CONFIG_CPU_COUNTERS */

	arm_set_user_tbi();

	arm_vm_physmap_init(args);
	set_mmu_ttb_alternate(cpu_ttep & TTBR_BADDR_MASK);

	ml_enable_monitor();

	set_mmu_ttb(invalid_ttep & TTBR_BADDR_MASK);

	flush_mmu_tlb();
	kva_active = TRUE;
	// global table pointers may need to be different due to physical aperture remapping
	cpu_tte = (tt_entry_t*)(phystokv(cpu_ttep));
	invalid_tte = (tt_entry_t*)(phystokv(invalid_ttep));

	// From here on out, we're off the bootstrap translation tables.


	/* AuxKC initialization has to be deferred until this point, since
	 * the AuxKC may not have been fully mapped in the bootstrap
	 * tables, if it spilled downwards into the prior L2 block.
	 *
	 * Now that its mapping set up by arm_vm_prot_init() is active,
	 * we can traverse and fix it up.
	 */

	/* Calculate the physical bounds of the kernelcache; using
	 * gVirtBase/gPhysBase math to do this directly is generally a bad idea
	 * as the physmap is no longer physically contiguous.  However, this is
	 * done here as segLOWEST and end_kern are both virtual addresses the
	 * bootstrap physmap, and because kvtophys references the page tables
	 * (at least at the time this comment was written), meaning that at
	 * least end_kern may not point to a valid mapping on some kernelcache
	 * configurations, so kvtophys would report a physical address of 0.
	 *
	 * Long term, the kernelcache should probably be described in terms of
	 * multiple physical ranges, as there is no strong guarantee or
	 * requirement that the kernelcache will always be physically
	 * contiguous.
	 */
	arm_vm_kernelcache_phys_start = segLOWEST - gVirtBase + gPhysBase;
	arm_vm_kernelcache_phys_end = end_kern - gVirtBase + gPhysBase;;

	/* Calculate the number of pages that belong to the kernelcache. */
	vm_page_kernelcache_count = (unsigned int) (atop_64(arm_vm_kernelcache_phys_end - arm_vm_kernelcache_phys_start));

	if (arm_vm_auxkc_init()) {
		if (segLOWESTROAuxKC < segLOWESTRO) {
			segLOWESTRO = segLOWESTROAuxKC;
		}
		if (segHIGHESTROAuxKC > segHIGHESTRO) {
			segHIGHESTRO = segHIGHESTROAuxKC;
		}
		if (segLOWESTRXAuxKC < segLOWESTTEXT) {
			segLOWESTTEXT = segLOWESTRXAuxKC;
		}
		assert(segLOWEST == segLOWESTAuxKC);

		// The preliminary auxKC mapping has been broken up.
		flush_mmu_tlb();
	}

	sane_size = mem_size - (avail_start - gPhysBase);
	max_mem = mem_size;
	vm_kernel_slid_base = segLOWESTTEXT;
	// vm_kernel_slide is set by arm_init()->arm_slide_rebase_and_sign_image()
	vm_kernel_stext = segTEXTB;

	if (kernel_mach_header_is_in_fileset(&_mh_execute_header)) {
		vm_kernel_etext = segTEXTEXECB + segSizeTEXTEXEC;
		vm_kernel_slid_top = vm_slinkedit;
	} else {
		assert(segDATACONSTB == segTEXTB + segSizeTEXT);
		assert(segTEXTEXECB == segDATACONSTB + segSizeDATACONST);
		vm_kernel_etext = segTEXTB + segSizeTEXT + segSizeDATACONST + segSizeTEXTEXEC;
		vm_kernel_slid_top = vm_prelink_einfo;
	}

	dynamic_memory_begin = ROUND_TWIG(dynamic_memory_begin);
#if defined(KERNEL_INTEGRITY_CTRR) && defined(CONFIG_XNUPOST)
	// reserve a 32MB region without permission overrides to use later for a CTRR unit test
	{
		extern vm_offset_t ctrr_test_page;
		tt_entry_t *new_tte;

		ctrr_test_page = dynamic_memory_begin;
		dynamic_memory_begin += ARM_TT_L2_SIZE;
		cpu_l1_tte = cpu_tte + ((ctrr_test_page & ARM_TT_L1_INDEX_MASK) >> ARM_TT_L1_SHIFT);
		assert((*cpu_l1_tte) & ARM_TTE_VALID);
		cpu_l2_tte = ((tt_entry_t *) phystokv(((*cpu_l1_tte) & ARM_TTE_TABLE_MASK))) + ((ctrr_test_page & ARM_TT_L2_INDEX_MASK) >> ARM_TT_L2_SHIFT);
		assert((*cpu_l2_tte) == ARM_TTE_EMPTY);
		new_tte = (tt_entry_t *)alloc_ptpage(FALSE);
		bzero(new_tte, ARM_PGBYTES);
		*cpu_l2_tte = (kvtophys((vm_offset_t)new_tte) & ARM_TTE_TABLE_MASK) | ARM_TTE_TYPE_TABLE | ARM_TTE_VALID;
	}
#endif /* defined(KERNEL_INTEGRITY_CTRR) && defined(CONFIG_XNUPOST) */
#if XNU_MONITOR
	for (vm_offset_t cur = (vm_offset_t)pmap_stacks_start; cur < (vm_offset_t)pmap_stacks_end; cur += ARM_PGBYTES) {
		arm_vm_map(cpu_tte, cur, ARM_PTE_EMPTY);
	}
#if HAS_GUARDED_IO_FILTER
    for (vm_offset_t cur = (vm_offset_t)iofilter_stacks_start; cur < (vm_offset_t)iofilter_stacks_end; cur += ARM_PGBYTES) {
        arm_vm_map(cpu_tte, cur, ARM_PTE_EMPTY);
    }
#endif
#endif
	pmap_bootstrap(dynamic_memory_begin);

	disable_preemption();

	/*
	 * Initialize l3 page table pages :
	 *   cover this address range:
	 *    2MB + FrameBuffer size + 10MB for each 256MB segment
	 */

	mem_segments = (mem_size + 0x0FFFFFFF) >> 28;

	va_l1 = dynamic_memory_begin;
	va_l1_end = va_l1 + ((2 + (mem_segments * 10)) << 20);
	va_l1_end += round_page(args->Video.v_height * args->Video.v_rowBytes);
	va_l1_end = (va_l1_end + 0x00000000007FFFFFULL) & 0xFFFFFFFFFF800000ULL;

	cpu_l1_tte = cpu_tte + ((va_l1 & ARM_TT_L1_INDEX_MASK) >> ARM_TT_L1_SHIFT);

	while (va_l1 < va_l1_end) {
		va_l2 = va_l1;

		if (((va_l1 & ~ARM_TT_L1_OFFMASK) + ARM_TT_L1_SIZE) < va_l1) {
			/* If this is the last L1 entry, it must cover the last mapping. */
			va_l2_end = va_l1_end;
		} else {
			va_l2_end = MIN((va_l1 & ~ARM_TT_L1_OFFMASK) + ARM_TT_L1_SIZE, va_l1_end);
		}

		cpu_l2_tte = ((tt_entry_t *) phystokv(((*cpu_l1_tte) & ARM_TTE_TABLE_MASK))) + ((va_l2 & ARM_TT_L2_INDEX_MASK) >> ARM_TT_L2_SHIFT);

		while (va_l2 < va_l2_end) {
			pt_entry_t *    ptp;
			pmap_paddr_t    ptp_phys;

			/* Allocate a page and setup L3 Table TTE in L2 */
			ptp = (pt_entry_t *) alloc_ptpage(FALSE);
			ptp_phys = (pmap_paddr_t)kvtophys((vm_offset_t)ptp);

			bzero(ptp, ARM_PGBYTES);
			pmap_init_pte_page(kernel_pmap, ptp, va_l2, 3, TRUE);

			*cpu_l2_tte = (pa_to_tte(ptp_phys)) | ARM_TTE_TYPE_TABLE | ARM_TTE_VALID | ARM_DYNAMIC_TABLE_XN;

			va_l2 += ARM_TT_L2_SIZE;
			cpu_l2_tte++;
		}

		va_l1 = va_l2_end;
		cpu_l1_tte++;
	}

#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
	/*
	 * In this configuration, the bootstrap mappings (arm_vm_init) and
	 * the heap mappings occupy separate L1 regions.  Explicitly set up
	 * the heap L1 allocations here.
	 */
#if defined(ARM_LARGE_MEMORY)
	init_ptpages(cpu_tte, KERNEL_PMAP_HEAP_RANGE_START & ~ARM_TT_L1_OFFMASK, VM_MAX_KERNEL_ADDRESS, FALSE, ARM_DYNAMIC_TABLE_XN | ARM_TTE_TABLE_AP(ARM_TTE_TABLE_AP_USER_NA));
#else // defined(ARM_LARGE_MEMORY)
	va_l1 = VM_MIN_KERNEL_ADDRESS & ~ARM_TT_L1_OFFMASK;
	init_ptpages(cpu_tte, VM_MIN_KERNEL_ADDRESS & ~ARM_TT_L1_OFFMASK, VM_MAX_KERNEL_ADDRESS, FALSE, ARM_DYNAMIC_TABLE_XN | ARM_TTE_TABLE_AP(ARM_TTE_TABLE_AP_USER_NA));
#endif // defined(ARM_LARGE_MEMORY)
#else
#if defined(ARM_LARGE_MEMORY)
	/* For large memory systems with no KTRR/CTRR such as virtual machines */
	init_ptpages(cpu_tte, KERNEL_PMAP_HEAP_RANGE_START & ~ARM_TT_L1_OFFMASK, VM_MAX_KERNEL_ADDRESS, FALSE, ARM_DYNAMIC_TABLE_XN | ARM_TTE_TABLE_AP(ARM_TTE_TABLE_AP_USER_NA));
#endif
#endif // defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)

	/*
	 * Initialize l3 page table pages :
	 *   cover this address range:
	 *   ((VM_MAX_KERNEL_ADDRESS & CPUWINDOWS_BASE_MASK) - PE_EARLY_BOOT_VA) to VM_MAX_KERNEL_ADDRESS
	 */
	va_l1 = (VM_MAX_KERNEL_ADDRESS & CPUWINDOWS_BASE_MASK) - PE_EARLY_BOOT_VA;
	va_l1_end = VM_MAX_KERNEL_ADDRESS;

	cpu_l1_tte = cpu_tte + ((va_l1 & ARM_TT_L1_INDEX_MASK) >> ARM_TT_L1_SHIFT);

	while (va_l1 < va_l1_end) {
		va_l2 = va_l1;

		if (((va_l1 & ~ARM_TT_L1_OFFMASK) + ARM_TT_L1_SIZE) < va_l1) {
			/* If this is the last L1 entry, it must cover the last mapping. */
			va_l2_end = va_l1_end;
		} else {
			va_l2_end = MIN((va_l1 & ~ARM_TT_L1_OFFMASK) + ARM_TT_L1_SIZE, va_l1_end);
		}

		cpu_l2_tte = ((tt_entry_t *) phystokv(((*cpu_l1_tte) & ARM_TTE_TABLE_MASK))) + ((va_l2 & ARM_TT_L2_INDEX_MASK) >> ARM_TT_L2_SHIFT);

		while (va_l2 < va_l2_end) {
			pt_entry_t *    ptp;
			pmap_paddr_t    ptp_phys;

			/* Allocate a page and setup L3 Table TTE in L2 */
			ptp = (pt_entry_t *) alloc_ptpage(FALSE);
			ptp_phys = (pmap_paddr_t)kvtophys((vm_offset_t)ptp);

			bzero(ptp, ARM_PGBYTES);
			pmap_init_pte_page(kernel_pmap, ptp, va_l2, 3, TRUE);

			*cpu_l2_tte = (pa_to_tte(ptp_phys)) | ARM_TTE_TYPE_TABLE | ARM_TTE_VALID | ARM_DYNAMIC_TABLE_XN;

			va_l2 += ARM_TT_L2_SIZE;
			cpu_l2_tte++;
		}

		va_l1 = va_l2_end;
		cpu_l1_tte++;
	}


	/*
	 * Adjust avail_start so that the range that the VM owns
	 * starts on a PAGE_SIZE aligned boundary.
	 */
	avail_start = (avail_start + PAGE_MASK) & ~PAGE_MASK;

#if XNU_MONITOR
	pmap_static_allocations_done();
#endif
	first_avail = avail_start;
	patch_low_glo_static_region(args->topOfKernelData, avail_start - args->topOfKernelData);
	enable_preemption();
}

/*
 * Returns true if the address is within __TEXT, __TEXT_EXEC or __DATA_CONST
 * segment range. This is what [vm_kernel_stext, vm_kernel_etext) range used to
 * cover. The segments together may not be continuous anymore and so individual
 * intervals are inspected.
 */
bool
kernel_text_contains(vm_offset_t addr)
{
	if (segTEXTB <= addr && addr < (segTEXTB + segSizeTEXT)) {
		return true;
	}
	if (segTEXTEXECB <= addr && addr < (segTEXTEXECB + segSizeTEXTEXEC)) {
		return true;
	}
	return segDATACONSTB <= addr && addr < (segDATACONSTB + segSizeDATACONST);
}
