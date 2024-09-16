/*
 * Copyright (c) 2003-2019 Apple Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989, 1988 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */


#include <mach/i386/vm_param.h>

#include <string.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>
#include <mach/time_value.h>
#include <kern/spl.h>
#include <kern/assert.h>
#include <kern/debug.h>
#include <kern/misc_protos.h>
#include <kern/cpu_data.h>
#include <kern/processor.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <i386/pmap.h>
#include <i386/misc_protos.h>
#include <i386/cpuid.h>
#include <mach/thread_status.h>
#include <pexpert/i386/efi.h>
#include <pexpert/pexpert.h>
#include <i386/i386_lowmem.h>
#include <i386/misc_protos.h>
#include <x86_64/lowglobals.h>
#include <i386/pal_routines.h>
#include <vm/vm_page_internal.h>

#include <mach-o/loader.h>
#include <libkern/kernel_mach_header.h>

#define P2ROUNDUP(x, align)             (-(-(x) & -(align)))

vm_size_t       mem_size = 0;
pmap_paddr_t    first_avail = 0;/* first after page tables */

uint64_t        max_mem;        /* Size of physical memory minus carveouts (bytes), adjusted by maxmem */
uint64_t        max_mem_actual; /* Actual size of physical memory (bytes) adjusted by
                                 * the maxmem boot-arg */
uint64_t        mem_actual;
uint64_t        sane_size = 0;  /* Memory size for defaults calculations */

/*
 * KASLR parameters
 */
ppnum_t         vm_kernel_base_page;
vm_offset_t     vm_kernel_base;
vm_offset_t     vm_kernel_top;
vm_offset_t     vm_kernel_stext;
vm_offset_t     vm_kernel_etext;
vm_offset_t     vm_kernel_slide;
vm_offset_t     vm_kernel_slid_base;
vm_offset_t     vm_kernel_slid_top;
vm_offset_t vm_hib_base;
vm_offset_t     vm_kext_base = VM_MIN_KERNEL_AND_KEXT_ADDRESS;
vm_offset_t     vm_kext_top = VM_MIN_KERNEL_ADDRESS;

vm_offset_t vm_prelink_stext;
vm_offset_t vm_prelink_etext;
vm_offset_t vm_prelink_sinfo;
vm_offset_t vm_prelink_einfo;
vm_offset_t vm_slinkedit;
vm_offset_t vm_elinkedit;

vm_offset_t vm_kernel_builtinkmod_text;
vm_offset_t vm_kernel_builtinkmod_text_end;

#define MAXLORESERVE    (32 * 1024 * 1024)

ppnum_t         max_ppnum = 0;

/*
 * pmap_high_used* are the highest range of physical memory used for kernel
 * internals (page tables, vm_pages) via pmap_steal_memory() that don't
 * need to be encrypted in hibernation images. There can be one gap in
 * the middle of this due to fragmentation when using a mix of small
 * and large pages.  In that case, the fragment lives between the high
 * and middle ranges.
 */
ppnum_t pmap_high_used_top = 0;
ppnum_t pmap_high_used_bottom = 0;
ppnum_t pmap_middle_used_top = 0;
ppnum_t pmap_middle_used_bottom = 0;

enum {PMAP_MAX_RESERVED_RANGES = 32};
uint32_t pmap_reserved_pages_allocated = 0;
uint32_t pmap_reserved_range_indices[PMAP_MAX_RESERVED_RANGES];
uint32_t pmap_last_reserved_range_index = 0;
uint32_t pmap_reserved_ranges = 0;

extern unsigned int bsd_mbuf_cluster_reserve(boolean_t *);

pmap_paddr_t     avail_start, avail_end;
vm_offset_t     virtual_avail, virtual_end;
static pmap_paddr_t     avail_remaining;
vm_offset_t     static_memory_end = 0;

vm_offset_t     sHIB, eHIB, stext, etext, sdata, edata, end, sconst, econst;

/*
 * _mh_execute_header is the mach_header for the currently executing kernel
 */
vm_offset_t segTEXTB; unsigned long segSizeTEXT;
vm_offset_t segDATAB; unsigned long segSizeDATA;
vm_offset_t segLINKB; unsigned long segSizeLINK;
vm_offset_t segPRELINKTEXTB; unsigned long segSizePRELINKTEXT;
vm_offset_t segPRELINKINFOB; unsigned long segSizePRELINKINFO;
vm_offset_t segHIBB; unsigned long segSizeHIB;
unsigned long segSizeConst;

static kernel_segment_command_t *segTEXT, *segDATA;
static kernel_section_t *cursectTEXT, *lastsectTEXT;
static kernel_segment_command_t *segCONST;

extern uint64_t firmware_Conventional_bytes;
extern uint64_t firmware_RuntimeServices_bytes;
extern uint64_t firmware_ACPIReclaim_bytes;
extern uint64_t firmware_ACPINVS_bytes;
extern uint64_t firmware_PalCode_bytes;
extern uint64_t firmware_Reserved_bytes;
extern uint64_t firmware_Unusable_bytes;
extern uint64_t firmware_other_bytes;
uint64_t firmware_MMIO_bytes;

/*
 * Linker magic to establish the highest address in the kernel.
 */
extern void     *last_kernel_symbol;

#define LG_PPNUM_PAGES (I386_LPGBYTES >> PAGE_SHIFT)
#define LG_PPNUM_MASK (I386_LPGMASK >> PAGE_SHIFT)

/* set so no region large page fragment pages exist */
#define RESET_FRAG(r) (((r)->alloc_frag_up = 1), ((r)->alloc_frag_down = 0))

boolean_t       memmap = FALSE;
#if     DEBUG || DEVELOPMENT
static void
kprint_memmap(vm_offset_t maddr, unsigned int msize, unsigned int mcount)
{
	unsigned int         i;
	unsigned int         j;
	pmap_memory_region_t *p = pmap_memory_regions;
	EfiMemoryRange       *mptr;
	addr64_t             region_start, region_end;
	addr64_t             efi_start, efi_end;

	for (j = 0; j < pmap_memory_region_count; j++, p++) {
		kprintf("pmap region %d type %d base 0x%llx alloc_up 0x%llx alloc_down 0x%llx"
		    " alloc_frag_up 0x%llx alloc_frag_down 0x%llx top 0x%llx\n",
		    j, p->type,
		    (addr64_t) p->base << I386_PGSHIFT,
		    (addr64_t) p->alloc_up << I386_PGSHIFT,
		    (addr64_t) p->alloc_down << I386_PGSHIFT,
		    (addr64_t) p->alloc_frag_up << I386_PGSHIFT,
		    (addr64_t) p->alloc_frag_down << I386_PGSHIFT,
		    (addr64_t) p->end   << I386_PGSHIFT);
		region_start = (addr64_t) p->base << I386_PGSHIFT;
		region_end = ((addr64_t) p->end << I386_PGSHIFT) - 1;
		mptr = (EfiMemoryRange *) maddr;
		for (i = 0;
		    i < mcount;
		    i++, mptr = (EfiMemoryRange *)(((vm_offset_t)mptr) + msize)) {
			if (mptr->Type != kEfiLoaderCode &&
			    mptr->Type != kEfiLoaderData &&
			    mptr->Type != kEfiBootServicesCode &&
			    mptr->Type != kEfiBootServicesData &&
			    mptr->Type != kEfiConventionalMemory) {
				efi_start = (addr64_t)mptr->PhysicalStart;
				efi_end = efi_start + ((vm_offset_t)mptr->NumberOfPages << I386_PGSHIFT) - 1;
				if ((efi_start >= region_start && efi_start <= region_end) ||
				    (efi_end >= region_start && efi_end <= region_end)) {
					kprintf(" *** Overlapping region with EFI runtime region %d\n", i);
				}
			}
		}
	}
}
#define DPRINTF(x...)   do { if (memmap) kprintf(x); } while (0)

#else

static void
kprint_memmap(vm_offset_t maddr, unsigned int msize, unsigned int mcount)
{
#pragma unused(maddr, msize, mcount)
}

#define DPRINTF(x...)
#endif /* DEBUG */

/*
 * Basic VM initialization.
 */
void
i386_vm_init(uint64_t   maxmem,
    boolean_t  IA32e,
    boot_args  *args)
{
	pmap_memory_region_t *pmptr;
	pmap_memory_region_t *prev_pmptr;
	EfiMemoryRange *mptr;
	unsigned int mcount;
	unsigned int msize;
	vm_offset_t maddr;
	ppnum_t fap;
	unsigned int i;
	ppnum_t maxpg = 0;
	uint32_t pmap_type;
	uint32_t maxloreserve;
	uint32_t maxdmaaddr;
	uint32_t  mbuf_reserve = 0;
	boolean_t mbuf_override = FALSE;
	boolean_t coalescing_permitted;
	vm_kernel_base_page = i386_btop(args->kaddr);
	vm_offset_t base_address;
	vm_offset_t static_base_address;

	PE_parse_boot_argn("memmap", &memmap, sizeof(memmap));

	/*
	 * Establish the KASLR parameters.
	 */
	static_base_address = ml_static_ptovirt(KERNEL_BASE_OFFSET);
	base_address        = ml_static_ptovirt(args->kaddr);
	vm_kernel_slide     = base_address - static_base_address;
	if (args->kslide) {
		kprintf("KASLR slide: 0x%016lx dynamic\n", vm_kernel_slide);
		if (vm_kernel_slide != ((vm_offset_t)args->kslide)) {
			panic("Kernel base inconsistent with slide - rebased?");
		}
	} else {
		/* No slide relative to on-disk symbols */
		kprintf("KASLR slide: 0x%016lx static and ignored\n",
		    vm_kernel_slide);
		vm_kernel_slide = 0;
	}

	/*
	 * Zero out local relocations to avoid confusing kxld.
	 * TODO: might be better to move this code to OSKext::initialize
	 */
	if (_mh_execute_header.flags & MH_PIE) {
		struct load_command *loadcmd;
		uint32_t cmd;

		loadcmd = (struct load_command *)((uintptr_t)&_mh_execute_header +
		    sizeof(_mh_execute_header));

		for (cmd = 0; cmd < _mh_execute_header.ncmds; cmd++) {
			if (loadcmd->cmd == LC_DYSYMTAB) {
				struct dysymtab_command *dysymtab;

				dysymtab = (struct dysymtab_command *)loadcmd;
				dysymtab->nlocrel = 0;
				dysymtab->locreloff = 0;
				kprintf("Hiding local relocations\n");
				break;
			}
			loadcmd = (struct load_command *)((uintptr_t)loadcmd + loadcmd->cmdsize);
		}
	}

	/*
	 * Now retrieve addresses for end, edata, and etext
	 * from MACH-O headers.
	 */
	segTEXTB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header,
	    "__TEXT", &segSizeTEXT);
	segDATAB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header,
	    "__DATA", &segSizeDATA);
	segLINKB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header,
	    "__LINKEDIT", &segSizeLINK);
	segHIBB  = (vm_offset_t) getsegdatafromheader(&_mh_execute_header,
	    "__HIB", &segSizeHIB);
	segPRELINKTEXTB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header,
	    "__PRELINK_TEXT", &segSizePRELINKTEXT);
	segPRELINKINFOB = (vm_offset_t) getsegdatafromheader(&_mh_execute_header,
	    "__PRELINK_INFO", &segSizePRELINKINFO);
	segTEXT = getsegbynamefromheader(&_mh_execute_header,
	    "__TEXT");
	segDATA = getsegbynamefromheader(&_mh_execute_header,
	    "__DATA");
	segCONST = getsegbynamefromheader(&_mh_execute_header,
	    "__DATA_CONST");
	cursectTEXT = lastsectTEXT = firstsect(segTEXT);
	/* Discover the last TEXT section within the TEXT segment */
	while ((cursectTEXT = nextsect(segTEXT, cursectTEXT)) != NULL) {
		lastsectTEXT = cursectTEXT;
	}

	sHIB  = segHIBB;
	eHIB  = segHIBB + segSizeHIB;
	vm_hib_base = sHIB;
	/* Zero-padded from ehib to stext if text is 2M-aligned */
	stext = segTEXTB;
	lowGlo.lgStext = stext;
	etext = (vm_offset_t) round_page_64(lastsectTEXT->addr + lastsectTEXT->size);
	/* Zero-padded from etext to sdata if text is 2M-aligned */
	sdata = segDATAB;
	edata = segDATAB + segSizeDATA;

	sconst = segCONST->vmaddr;
	segSizeConst = segCONST->vmsize;
	econst = sconst + segSizeConst;

	kc_format_t kc_format = KCFormatUnknown;

	/* XXX: FIXME_IN_dyld: For new-style kernel caches, the ending address of __DATA_CONST may not be page-aligned */
	if (PE_get_primary_kc_format(&kc_format) && kc_format == KCFormatFileset) {
		/* Round up the end */
		econst = P2ROUNDUP(econst, PAGE_SIZE);
		edata = P2ROUNDUP(edata, PAGE_SIZE);
	} else {
		assert(((sconst | econst) & PAGE_MASK) == 0);
		assert(((sdata | edata) & PAGE_MASK) == 0);
	}

	DPRINTF("segTEXTB    = %p\n", (void *) segTEXTB);
	DPRINTF("segDATAB    = %p\n", (void *) segDATAB);
	DPRINTF("segLINKB    = %p\n", (void *) segLINKB);
	DPRINTF("segHIBB     = %p\n", (void *) segHIBB);
	DPRINTF("segPRELINKTEXTB = %p\n", (void *) segPRELINKTEXTB);
	DPRINTF("segPRELINKINFOB = %p\n", (void *) segPRELINKINFOB);
	DPRINTF("sHIB        = %p\n", (void *) sHIB);
	DPRINTF("eHIB        = %p\n", (void *) eHIB);
	DPRINTF("stext       = %p\n", (void *) stext);
	DPRINTF("etext       = %p\n", (void *) etext);
	DPRINTF("sdata       = %p\n", (void *) sdata);
	DPRINTF("edata       = %p\n", (void *) edata);
	DPRINTF("sconst      = %p\n", (void *) sconst);
	DPRINTF("econst      = %p\n", (void *) econst);
	DPRINTF("kernel_top  = %p\n", (void *) &last_kernel_symbol);

	vm_kernel_base  = sHIB;
	vm_kernel_top   = (vm_offset_t) &last_kernel_symbol;
	vm_kernel_stext = stext;
	vm_kernel_etext = etext;
	vm_prelink_stext = segPRELINKTEXTB;
	vm_prelink_etext = segPRELINKTEXTB + segSizePRELINKTEXT;
	vm_prelink_sinfo = segPRELINKINFOB;
	vm_prelink_einfo = segPRELINKINFOB + segSizePRELINKINFO;
	vm_slinkedit = segLINKB;
	vm_elinkedit = segLINKB + segSizeLINK;

	/*
	 * In the fileset world, we want to be able to (un)slide addresses from
	 * the kernel or any of the kexts (e.g., for kernel logging metadata
	 * passed between the kernel and logd in userspace). VM_KERNEL_UNSLIDE
	 * (via VM_KERNEL_IS_SLID) should apply to the addresses in the range
	 * from the first basement address to the last boot kc address.
	 *
	 *                     ^
	 *                     :
	 *                     |
	 *  vm_kernel_slid_top - ---------------------------------------------
	 *                     |
	 *                     :
	 *                     : Boot kc (kexts in the boot kc here)
	 *                     : - - - - - - - - - - - - - - - - - - - - - - -
	 *                     :
	 *                     :
	 *                     | Boot kc (kernel here)
	 *                     - ---------------------------------------------
	 *                     |
	 *                     :
	 *                     | Basement (kexts in pageable and aux kcs here)
	 * vm_kernel_slid_base - ---------------------------------------------
	 *                     0
	 */

	vm_kernel_slid_base = vm_kext_base + vm_kernel_slide;
	vm_kernel_slid_top = (kc_format == KCFormatFileset) ?
	    vm_slinkedit : vm_prelink_einfo;

	vm_page_kernelcache_count = (unsigned int) (atop_64(vm_kernel_top - vm_kernel_base));

	vm_set_page_size();

	/*
	 * Compute the memory size.
	 */

	avail_remaining = 0;
	avail_end = 0;
	pmptr = pmap_memory_regions;
	prev_pmptr = 0;
	pmap_memory_region_count = pmap_memory_region_current = 0;
	fap = (ppnum_t) i386_btop(first_avail);

	maddr = ml_static_ptovirt((vm_offset_t)args->MemoryMap);
	mptr = (EfiMemoryRange *)maddr;
	if (args->MemoryMapDescriptorSize == 0) {
		panic("Invalid memory map descriptor size");
	}
	msize = args->MemoryMapDescriptorSize;
	mcount = args->MemoryMapSize / msize;

#define FOURGIG 0x0000000100000000ULL
#define ONEGIG  0x0000000040000000ULL

	for (i = 0; i < mcount; i++, mptr = (EfiMemoryRange *)(((vm_offset_t)mptr) + msize)) {
		ppnum_t base, top;
		uint64_t region_bytes = 0;

		if (pmap_memory_region_count >= PMAP_MEMORY_REGIONS_SIZE) {
			kprintf("WARNING: truncating memory region count at %d\n", pmap_memory_region_count);
			break;
		}
		base = (ppnum_t) (mptr->PhysicalStart >> I386_PGSHIFT);
		top = (ppnum_t) (((mptr->PhysicalStart) >> I386_PGSHIFT) + mptr->NumberOfPages - 1);

		if (base == 0) {
			/*
			 * Avoid having to deal with the edge case of the
			 * very first possible physical page and the roll-over
			 * to -1; just ignore that page.
			 */
			kprintf("WARNING: ignoring first page in [0x%llx:0x%llx]\n", (uint64_t) base, (uint64_t) top);
			base++;
		}
		if (top + 1 == 0) {
			/*
			 * Avoid having to deal with the edge case of the
			 * very last possible physical page and the roll-over
			 * to 0; just ignore that page.
			 */
			kprintf("WARNING: ignoring last page in [0x%llx:0x%llx]\n", (uint64_t) base, (uint64_t) top);
			top--;
		}
		if (top < base) {
			/*
			 * That was the only page in that region, so
			 * ignore the whole region.
			 */
			continue;
		}

#if     MR_RSV_TEST
		static uint32_t nmr = 0;
		if ((base > 0x20000) && (nmr++ < 4)) {
			mptr->Attribute |= EFI_MEMORY_KERN_RESERVED;
		}
#endif
		region_bytes = (uint64_t)(mptr->NumberOfPages << I386_PGSHIFT);
		pmap_type = mptr->Type;

		switch (mptr->Type) {
		case kEfiLoaderCode:
		case kEfiLoaderData:
		case kEfiBootServicesCode:
		case kEfiBootServicesData:
		case kEfiConventionalMemory:
			/*
			 * Consolidate usable memory types into one.
			 */
			pmap_type = kEfiConventionalMemory;
			sane_size += region_bytes;
			firmware_Conventional_bytes += region_bytes;
			break;
		/*
		 * sane_size should reflect the total amount of physical
		 * RAM in the system, not just the amount that is
		 * available for the OS to use.
		 * We now get this value from SMBIOS tables
		 * rather than reverse engineering the memory map.
		 * But the legacy computation of "sane_size" is kept
		 * for diagnostic information.
		 */

		case kEfiRuntimeServicesCode:
		case kEfiRuntimeServicesData:
			firmware_RuntimeServices_bytes += region_bytes;
			sane_size += region_bytes;
			break;
		case kEfiACPIReclaimMemory:
			firmware_ACPIReclaim_bytes += region_bytes;
			sane_size += region_bytes;
			break;
		case kEfiACPIMemoryNVS:
			firmware_ACPINVS_bytes += region_bytes;
			sane_size += region_bytes;
			break;
		case kEfiPalCode:
			firmware_PalCode_bytes += region_bytes;
			sane_size += region_bytes;
			break;

		case kEfiReservedMemoryType:
			firmware_Reserved_bytes += region_bytes;
			break;
		case kEfiUnusableMemory:
			firmware_Unusable_bytes += region_bytes;
			break;
		case kEfiMemoryMappedIO:
		case kEfiMemoryMappedIOPortSpace:
			firmware_MMIO_bytes += region_bytes;
			break;
		default:
			firmware_other_bytes += region_bytes;
			break;
		}

		DPRINTF("EFI region %d: type %u/%d, base 0x%x, top 0x%x %s\n",
		    i, mptr->Type, pmap_type, base, top,
		    (mptr->Attribute & EFI_MEMORY_KERN_RESERVED)? "RESERVED" :
		    (mptr->Attribute & EFI_MEMORY_RUNTIME)? "RUNTIME" : "");

		if (maxpg) {
			if (base >= maxpg) {
				break;
			}
			top = (top > maxpg) ? maxpg : top;
		}

		/*
		 * handle each region
		 */
		if ((mptr->Attribute & EFI_MEMORY_RUNTIME) == EFI_MEMORY_RUNTIME ||
		    pmap_type != kEfiConventionalMemory) {
			prev_pmptr = 0;
			continue;
		} else {
			/*
			 * Usable memory region
			 */
			if (top < I386_LOWMEM_RESERVED ||
			    !pal_is_usable_memory(base, top)) {
				prev_pmptr = 0;
				continue;
			}
			/*
			 * A range may be marked with with the
			 * EFI_MEMORY_KERN_RESERVED attribute
			 * on some systems, to indicate that the range
			 * must not be made available to devices.
			 */

			if (mptr->Attribute & EFI_MEMORY_KERN_RESERVED) {
				if (++pmap_reserved_ranges > PMAP_MAX_RESERVED_RANGES) {
					panic("Too many reserved ranges %u", pmap_reserved_ranges);
				}
			}

			if (top < fap) {
				/*
				 * entire range below first_avail
				 * salvage some low memory pages
				 * we use some very low memory at startup
				 * mark as already allocated here
				 */
				if (base >= I386_LOWMEM_RESERVED) {
					pmptr->base = base;
				} else {
					pmptr->base = I386_LOWMEM_RESERVED;
				}

				pmptr->end = top;


				if ((mptr->Attribute & EFI_MEMORY_KERN_RESERVED) &&
				    (top < vm_kernel_base_page)) {
					pmptr->alloc_up = pmptr->base;
					pmptr->alloc_down = pmptr->end;
					RESET_FRAG(pmptr);
					pmap_reserved_range_indices[pmap_last_reserved_range_index++] = pmap_memory_region_count;
				} else {
					/*
					 * mark as already mapped
					 */
					pmptr->alloc_up = top + 1;
					pmptr->alloc_down = top;
					RESET_FRAG(pmptr);
				}
				pmptr->type = pmap_type;
				pmptr->attribute = mptr->Attribute;
			} else if ((base < fap) && (top > fap)) {
				/*
				 * spans first_avail
				 * put mem below first avail in table but
				 * mark already allocated
				 */
				pmptr->base = base;
				pmptr->end = (fap - 1);
				pmptr->alloc_up = pmptr->end + 1;
				pmptr->alloc_down = pmptr->end;
				RESET_FRAG(pmptr);
				pmptr->type = pmap_type;
				pmptr->attribute = mptr->Attribute;
				/*
				 * we bump these here inline so the accounting
				 * below works correctly
				 */
				pmptr++;
				pmap_memory_region_count++;

				pmptr->alloc_up = pmptr->base = fap;
				pmptr->type = pmap_type;
				pmptr->attribute = mptr->Attribute;
				pmptr->alloc_down = pmptr->end = top;
				RESET_FRAG(pmptr);

				if (mptr->Attribute & EFI_MEMORY_KERN_RESERVED) {
					pmap_reserved_range_indices[pmap_last_reserved_range_index++] = pmap_memory_region_count;
				}
			} else {
				/*
				 * entire range useable
				 */
				pmptr->alloc_up = pmptr->base = base;
				pmptr->type = pmap_type;
				pmptr->attribute = mptr->Attribute;
				pmptr->alloc_down = pmptr->end = top;
				RESET_FRAG(pmptr);
				if (mptr->Attribute & EFI_MEMORY_KERN_RESERVED) {
					pmap_reserved_range_indices[pmap_last_reserved_range_index++] = pmap_memory_region_count;
				}
			}

			if (i386_ptob(pmptr->end) > avail_end) {
				avail_end = i386_ptob(pmptr->end);
			}

			avail_remaining += (pmptr->end - pmptr->base);
			coalescing_permitted = (prev_pmptr && (pmptr->attribute == prev_pmptr->attribute) && ((pmptr->attribute & EFI_MEMORY_KERN_RESERVED) == 0));
			/*
			 * Consolidate contiguous memory regions, if possible
			 */
			if (prev_pmptr &&
			    (pmptr->type == prev_pmptr->type) &&
			    (coalescing_permitted) &&
			    (pmptr->base == pmptr->alloc_up) &&
			    (prev_pmptr->end == prev_pmptr->alloc_down) &&
			    (pmptr->base == (prev_pmptr->end + 1))) {
				prev_pmptr->end = pmptr->end;
				prev_pmptr->alloc_down = pmptr->alloc_down;
				RESET_FRAG(pmptr);
			} else {
				pmap_memory_region_count++;
				prev_pmptr = pmptr;
				pmptr++;
			}
		}
	}

	if (memmap) {
		kprint_memmap(maddr, msize, mcount);
	}

	avail_start = first_avail;
	mem_actual = args->PhysicalMemorySize;

	/*
	 * For user visible memory size, round up to 128 Mb
	 * - accounting for the various stolen memory not reported by EFI.
	 * This is maintained for historical, comparison purposes but
	 * we now use the memory size reported by EFI/Booter.
	 */
	sane_size = (sane_size + 128 * MB - 1) & ~((uint64_t)(128 * MB - 1));
	if (sane_size != mem_actual) {
		printf("mem_actual: 0x%llx\n legacy sane_size: 0x%llx\n",
		    mem_actual, sane_size);
	}
	sane_size = mem_actual;

	/*
	 * We cap at KERNEL_MAXMEM bytes (see vm_param.h).
	 * Unless overriden by the maxmem= boot-arg
	 * -- which is a non-zero maxmem argument to this function.
	 */
	if (maxmem == 0 && sane_size > KERNEL_MAXMEM) {
		maxmem = KERNEL_MAXMEM;
		printf("Physical memory %lld bytes capped at %dGB\n",
		    sane_size, (uint32_t) (KERNEL_MAXMEM / GB));
	}

	/*
	 * if user set maxmem, reduce memory sizes
	 */
	if ((maxmem > (uint64_t)first_avail) && (maxmem < sane_size)) {
		ppnum_t discarded_pages  = (ppnum_t)((sane_size - maxmem) >> I386_PGSHIFT);
		ppnum_t highest_pn = 0;
		ppnum_t cur_end  = 0;
		uint64_t        pages_to_use;
		unsigned        cur_region = 0;

		sane_size = maxmem;

		if (avail_remaining > discarded_pages) {
			avail_remaining -= discarded_pages;
		} else {
			avail_remaining = 0;
		}

		pages_to_use = avail_remaining;

		while (cur_region < pmap_memory_region_count && pages_to_use) {
			for (cur_end = pmap_memory_regions[cur_region].base;
			    cur_end < pmap_memory_regions[cur_region].end && pages_to_use;
			    cur_end++) {
				if (cur_end > highest_pn) {
					highest_pn = cur_end;
				}
				pages_to_use--;
			}
			if (pages_to_use == 0) {
				pmap_memory_regions[cur_region].end = cur_end;
				pmap_memory_regions[cur_region].alloc_down = cur_end;
				RESET_FRAG(&pmap_memory_regions[cur_region]);
			}

			cur_region++;
		}
		pmap_memory_region_count = cur_region;

		avail_end = i386_ptob(highest_pn + 1);
	}

	/*
	 * mem_size is only a 32 bit container... follow the PPC route
	 * and pin it to a 2 Gbyte maximum
	 */
	if (sane_size > (FOURGIG >> 1)) {
		mem_size = (vm_size_t)(FOURGIG >> 1);
	} else {
		mem_size = (vm_size_t)sane_size;
	}
	max_mem = sane_size;
	max_mem_actual = sane_size;

	kprintf("Physical memory %llu MB\n", sane_size / MB);

	max_valid_low_ppnum = (2 * GB) / PAGE_SIZE;

	if (!PE_parse_boot_argn("max_valid_dma_addr", &maxdmaaddr, sizeof(maxdmaaddr))) {
		max_valid_dma_address = (uint64_t)4 * (uint64_t)GB;
	} else {
		max_valid_dma_address = ((uint64_t) maxdmaaddr) * MB;

		if ((max_valid_dma_address / PAGE_SIZE) < max_valid_low_ppnum) {
			max_valid_low_ppnum = (ppnum_t)(max_valid_dma_address / PAGE_SIZE);
		}
	}
	if (avail_end >= max_valid_dma_address) {
		if (!PE_parse_boot_argn("maxloreserve", &maxloreserve, sizeof(maxloreserve))) {
			if (sane_size >= (ONEGIG * 15)) {
				maxloreserve = (MAXLORESERVE / PAGE_SIZE) * 4;
			} else if (sane_size >= (ONEGIG * 7)) {
				maxloreserve = (MAXLORESERVE / PAGE_SIZE) * 2;
			} else {
				maxloreserve = MAXLORESERVE / PAGE_SIZE;
			}

#if SOCKETS
			mbuf_reserve = bsd_mbuf_cluster_reserve(&mbuf_override) / PAGE_SIZE;
#endif
		} else {
			maxloreserve = (maxloreserve * (1024 * 1024)) / PAGE_SIZE;
		}

		if (maxloreserve) {
			vm_lopage_free_limit = maxloreserve;

			if (mbuf_override == TRUE) {
				vm_lopage_free_limit += mbuf_reserve;
				vm_lopage_lowater = 0;
			} else {
				vm_lopage_lowater = vm_lopage_free_limit / 16;
			}

			vm_lopage_refill = TRUE;
			vm_lopage_needed = TRUE;
		}
	}

	/*
	 *	Initialize kernel physical map.
	 *	Kernel virtual address starts at VM_KERNEL_MIN_ADDRESS.
	 */
	kprintf("avail_remaining = 0x%lx\n", (unsigned long)avail_remaining);
	pmap_bootstrap(0, IA32e);
}


unsigned int
pmap_free_pages(void)
{
	return (unsigned int)avail_remaining;
}

boolean_t pmap_next_page_reserved(ppnum_t *);

/*
 * Pick a page from a "kernel private" reserved range; works around
 * errata on some hardware. EFI marks pages which can't be used for
 * certain kinds of I/O-ish activities as reserved. We reserve them for
 * kernel internal usage and prevent them from ever going on regular
 * free list.
 */
boolean_t
pmap_next_page_reserved(
	ppnum_t              *pn)
{
	uint32_t             n;
	pmap_memory_region_t *region;
	uint32_t             reserved_index;

	if (pmap_reserved_ranges) {
		for (n = 0; n < pmap_last_reserved_range_index; n++) {
			reserved_index = pmap_reserved_range_indices[n];
			region = &pmap_memory_regions[reserved_index];
			if (region->alloc_up <= region->alloc_down) {
				*pn = region->alloc_up++;
			} else if (region->alloc_frag_up <= region->alloc_frag_down) {
				*pn = region->alloc_frag_up++;
			} else {
				continue;
			}
			avail_remaining--;

			if (*pn > max_ppnum) {
				max_ppnum = *pn;
			}

			pmap_reserved_pages_allocated++;
#if DEBUG
			if (region->alloc_up > region->alloc_down) {
				kprintf("Exhausted reserved range index: %u, base: 0x%x end: 0x%x, type: 0x%x, attribute: 0x%llx\n", reserved_index, region->base, region->end, region->type, region->attribute);
			}
#endif
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * Return the highest large page available. Fails once there are no more large pages.
 */
kern_return_t
pmap_next_page_large(
	ppnum_t              *pn)
{
	int                  r;
	pmap_memory_region_t *region;
	ppnum_t              frag_start;
	ppnum_t              lgpg;

	if (avail_remaining < LG_PPNUM_PAGES) {
		return KERN_FAILURE;
	}

	for (r = pmap_memory_region_count - 1; r >= 0; r--) {
		region = &pmap_memory_regions[r];

		/*
		 * First check if there is enough memory.
		 */
		if (region->alloc_down < region->alloc_up ||
		    (region->alloc_down - region->alloc_up + 1) < LG_PPNUM_PAGES) {
			continue;
		}

		/*
		 * Find the starting large page, creating a fragment if needed.
		 */
		if ((region->alloc_down & LG_PPNUM_MASK) == LG_PPNUM_MASK) {
			lgpg = (region->alloc_down & ~LG_PPNUM_MASK);
		} else {
			/* Can only have 1 fragment per region at a time */
			if (region->alloc_frag_up <= region->alloc_frag_down) {
				continue;
			}

			/* Check for enough room below any fragment. */
			frag_start = (region->alloc_down & ~LG_PPNUM_MASK);
			if (frag_start < region->alloc_up ||
			    frag_start - region->alloc_up < LG_PPNUM_PAGES) {
				continue;
			}

			lgpg = frag_start - LG_PPNUM_PAGES;
			region->alloc_frag_up = frag_start;
			region->alloc_frag_down = region->alloc_down;
		}

		*pn = lgpg;
		region->alloc_down = lgpg - 1;


		avail_remaining -= LG_PPNUM_PAGES;
		if (*pn + LG_PPNUM_MASK > max_ppnum) {
			max_ppnum = *pn + LG_PPNUM_MASK;
		}

		return KERN_SUCCESS;
	}
	return KERN_FAILURE;
}

boolean_t
pmap_next_page_hi(
	ppnum_t              *pn,
	boolean_t            might_free)
{
	pmap_memory_region_t *region;
	int                  n;

	if (!might_free && pmap_next_page_reserved(pn)) {
		return TRUE;
	}

	if (avail_remaining) {
		for (n = pmap_memory_region_count - 1; n >= 0; n--) {
			region = &pmap_memory_regions[n];
			if (region->alloc_frag_up <= region->alloc_frag_down) {
				*pn = region->alloc_frag_down--;
			} else if (region->alloc_down >= region->alloc_up) {
				*pn = region->alloc_down--;
			} else {
				continue;
			}

			avail_remaining--;

			if (*pn > max_ppnum) {
				max_ppnum = *pn;
			}

			return TRUE;
		}
	}
	return FALSE;
}

/*
 * Record which high pages have been allocated so far,
 * so that pmap_init() can mark them PMAP_NOENCRYPT, which
 * makes hibernation faster.
 *
 * Because of the code in pmap_next_page_large(), we could
 * theoretically have fragments in several regions.
 * In practice that just doesn't happen. The last pmap region
 * is normally the largest and will satisfy all pmap_next_hi/large()
 * allocations. Since this information is used as an optimization
 * and it's ok to be conservative, we'll just record the information
 * for the final region.
 */
void
pmap_hi_pages_done(void)
{
	pmap_memory_region_t *r;

	r = &pmap_memory_regions[pmap_memory_region_count - 1];
	pmap_high_used_top = r->end;
	if (r->alloc_frag_up <= r->alloc_frag_down) {
		pmap_high_used_bottom = r->alloc_frag_down + 1;
		pmap_middle_used_top = r->alloc_frag_up - 1;
		if (r->alloc_up <= r->alloc_down) {
			pmap_middle_used_bottom = r->alloc_down + 1;
		} else {
			pmap_high_used_bottom = r->base;
		}
	} else {
		if (r->alloc_up <= r->alloc_down) {
			pmap_high_used_bottom = r->alloc_down + 1;
		} else {
			pmap_high_used_bottom = r->base;
		}
	}
#if     DEBUG || DEVELOPMENT
	kprintf("pmap_high_used_top      0x%x\n", pmap_high_used_top);
	kprintf("pmap_high_used_bottom   0x%x\n", pmap_high_used_bottom);
	kprintf("pmap_middle_used_top    0x%x\n", pmap_middle_used_top);
	kprintf("pmap_middle_used_bottom 0x%x\n", pmap_middle_used_bottom);
#endif
}

/*
 * Return the next available page from lowest memory for general use.
 */
boolean_t
pmap_next_page(
	ppnum_t              *pn)
{
	pmap_memory_region_t *region;

	if (avail_remaining) {
		while (pmap_memory_region_current < pmap_memory_region_count) {
			region = &pmap_memory_regions[pmap_memory_region_current];
			if (region->alloc_up <= region->alloc_down) {
				*pn = region->alloc_up++;
			} else if (region->alloc_frag_up <= region->alloc_frag_down) {
				*pn = region->alloc_frag_up++;
			} else {
				pmap_memory_region_current++;
				continue;
			}
			avail_remaining--;

			if (*pn > max_ppnum) {
				max_ppnum = *pn;
			}

			return TRUE;
		}
	}
	return FALSE;
}


boolean_t
pmap_valid_page(
	ppnum_t pn)
{
	unsigned int i;
	pmap_memory_region_t *pmptr = pmap_memory_regions;

	for (i = 0; i < pmap_memory_region_count; i++, pmptr++) {
		if ((pn >= pmptr->base) && (pn <= pmptr->end)) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * Returns true if the address lies in the kernel __TEXT segment range.
 */
bool
kernel_text_contains(vm_offset_t addr)
{
	return vm_kernel_stext <= addr && addr < vm_kernel_etext;
}
